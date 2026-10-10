#include "KwlFan.h"

#include "CableComp.h"
#include "StageMap.h"
#include "knxprod.h"

namespace Kwl
{
    namespace
    {
        /// Aderquerschnitt aus dem ETS-Enum in mm².
        constexpr float kSection[] = {0.5f, 0.7f, 1.0f, 1.5f};
    } // namespace

    KwlFan::KwlFan(uint8_t index)
    {
        _channelIndex = index;
    }

    void KwlFan::setup()
    {
        // Die Param-Makros rechnen ueber _channelIndex; sie stehen deshalb erst
        // hier zur Verfuegung und nicht im Konstruktor.
        mActive = ParamFAN_fActive;
        if (!mActive)
            return;

        mType = static_cast<FanType>(ParamFAN_fType);

        // Vor dem Start kann der Werkswert 0 V = Volllast angelegen haben: erst
        // die Totzeit bei 5,00 V, dann eine Richtung (entschieden 2026-10-06).
        restartGuard(millis());
        mDacChannel = ParamFAN_fChannel;
        mRoomNo = ParamFAN_fRoom;
        mGroupNo = ParamFAN_fGroup;
        mPhase = ParamFAN_fPhase ? 1 : 0;
        mShare = ParamFAN_fShare;

        // Polaritaet je Luefter (entschieden 2026-10-10): 0 = ueber 5 V Zuluft,
        // unter 5 V Abluft (LUNOS); 1 = umgekehrt, etwa fuer Fremdgeraete. Bis
        // dahin galt FANDRV_BELOW_5V_IS_SUPPLY im Board-Header fuer alle.
        mBelow5vIsSupply = ParamFAN_fPolarity == 1;

        // Der ETS-Parameter zaehlt ab 1, die Ausgabeschicht ab 0.
        if (mDacChannel > 0)
            mDacChannel--;

        // --- Kalibrierfaktor aus Messung P7 ----------------------------------
        // 10000 = 1,0000. Ein Kanal, der 0,25 % zu viel ausgibt, bekommt 9975 -
        // dann kommt an der Klemme die Sollspannung heraus.
        mCalib = static_cast<float>(ParamFAN_fCalibGain) / 10000.0f;

        // --- Leitungskompensation --------------------------------------------
        mCableComp = ParamFAN_fCableComp;
        const uint8_t sectionIdx = ParamFAN_fCableSection;
        mCableResistance = CableComp::resistance(
            static_cast<float>(ParamFAN_fCableLength),
            kSection[sectionIdx < 4 ? sectionIdx : 1]);

        // Stroeme stehen in der ETS in mA.
        mCurrent[0] = 0.0f;
        mCurrent[1] = static_cast<float>(ParamFAN_fCurrentS1) / 1000.0f;
        mCurrent[2] = static_cast<float>(ParamFAN_fCurrentS2) / 1000.0f;
        mCurrent[3] = static_cast<float>(ParamFAN_fCurrentS3) / 1000.0f;
        mCurrent[4] = static_cast<float>(ParamFAN_fCurrentS4) / 1000.0f;

        mFlow[0] = 0;
        mFlow[1] = ParamFAN_fFlowS1;
        mFlow[2] = ParamFAN_fFlowS2;
        mFlow[3] = ParamFAN_fFlowS3;
        mFlow[4] = ParamFAN_fFlowS4;

        // --- Freigabe und Suspendierung --------------------------------------
        mUseEnable = ParamFAN_fUseEnable;
        mSuspendAllowed = ParamFAN_fSuspendAllowed;

        // --- Filterzaehler ----------------------------------------------------
        mFilterMode = ParamFAN_fFilterMode;
        mFilterLimitSeconds = static_cast<uint32_t>(ParamFAN_fFilterHours) * 3600u;
        mFilterLimitVolume = static_cast<uint32_t>(ParamFAN_fFilterVolume) * 1000u;
        mFilterRemindMs = static_cast<uint32_t>(ParamFAN_fFilterRemind) * 3600000u;

        // Zyklisches Senden: ein Wert, der sich nie aendert, wird trotzdem hin und
        // wieder bestaetigt. 0 heisst aus.
        mSendCycleStatusMs = static_cast<uint32_t>(ParamFAN_fSendCycleStatus) * 60000u;
        mSendCycleHoursMs = static_cast<uint32_t>(ParamFAN_fSendCycleHours) * 3600000u;

        mStage = 0;
        mDirection = Direction::Supply;
        mStatusValid = false;
        mLastTick = millis();
    }

    // ------------------------------------------------------------ Persistenz

    KwlFan::PersistentState KwlFan::persistentState() const
    {
        PersistentState s{};
        s.flags = (mEnableLatched ? kFlagEnabled : 0) |
                  (mSuspended ? kFlagSuspended : 0);
        s.runSeconds = mRunSeconds;
        s.filterSeconds = mFilterSeconds;
        s.filterVolume = mFilterVolume;
        return s;
    }

    void KwlFan::restore(const PersistentState& state)
    {
        mEnableLatched = (state.flags & kFlagEnabled) != 0;
        mSuspended = (state.flags & kFlagSuspended) != 0;
        mRunSeconds = state.runSeconds;
        mFilterSeconds = state.filterSeconds;
        mFilterVolume = state.filterVolume;
    }

    // ------------------------------------------------------------ Eingaenge

    void KwlFan::processInputKo(GroupObject& ko)
    {
        if (!mActive)
            return;

        switch (FAN_KoCalcIndex(ko.asap()))
        {
            case FAN_KoEnable:
                // Selbsthaltend: eine einmal empfangene Freigabe bleibt, auch ueber
                // einen Spannungsausfall. Eine 0 nimmt sie wieder zurueck.
                mEnableLatched = ko.value(DPT_Enable);
                break;

            case FAN_KoSuspend:
                if (mSuspendAllowed)
                    mSuspended = ko.value(DPT_Switch);
                break;

            case FAN_KoFilterAck:
                // Nur die Quittung setzt die Zaehler zurueck - vom Objekt oder von
                // der Taste. Eine 0 auf "Filterwechsel faellig" unterdrueckt die
                // Meldung bloss.
                if (ko.value(DPT_Ack))
                    acknowledgeFilter();
                break;

            case FAN_KoFilterDue:
                // Eine 0 heisst "habe ich gesehen, mache ich morgen".
                if (!ko.value(DPT_Alarm))
                {
                    mFilterMuted = true;
                    mFilterMutedSince = millis();
                }
                break;

            default:
                break;
        }
    }

    void KwlFan::acknowledgeFilter()
    {
        mFilterSeconds = 0;
        mFilterVolume = 0;
        mFilterVolumeRemainder = 0;
        mFilterDue = false;
        mFilterMuted = false;
        logInfoP("Filterzaehler zurueckgesetzt");
    }

    // ------------------------------------------------------------ Sperren

    bool KwlFan::blocked() const
    {
        return (mUseEnable && !mEnableLatched) || mSuspended;
    }

    ErrorCode KwlFan::errorCode(bool groupConflict, ErrorCode moduleError,
                                ErrorCode roomError) const
    {
        // Die Reihenfolge der Codes IST die Prioritaet: der kleinste anliegende
        // Wert groesser 0 gewinnt. Deshalb wird das Minimum gebildet und keine
        // if-Kette gepflegt - in die setzt man einen neuen Code frueher oder
        // spaeter an der falschen Stelle ein, und der Fehler faellt nie auf.
        ErrorCode best = ErrorCode::None;
        best = lowestError(best, mUseEnable && !mEnableLatched ? ErrorCode::NoRelease
                                                               : ErrorCode::None);
        best = lowestError(best, moduleError);
        best = lowestError(best, roomError);
        best = lowestError(best, mSuspended ? ErrorCode::MonitoringPaused : ErrorCode::None);
        best = lowestError(best, mFilterDue ? ErrorCode::FilterDue : ErrorCode::None);
        best = lowestError(best, groupConflict ? ErrorCode::DirectionConflict
                                               : ErrorCode::None);
        return best;
    }

    void KwlFan::sendFault(bool groupConflict, ErrorCode moduleError,
                           ErrorCode roomError)
    {
        if (!mActive)
            return;

        const ErrorCode code = errorCode(groupConflict, moduleError, roomError);
        if (mFaultValid && code == mLastError)
            return;

        mLastError = code;
        mFaultValid = true;
        KoFAN_FaultCode.value(static_cast<uint8_t>(code), DPT_Value_1_Ucount);
        KoFAN_Fault.value(isAlarm(code), DPT_Alarm);

        if (code != ErrorCode::None)
            logInfoP("Fehlercode %u%s", (unsigned)code,
                     isAlarm(code) ? " (Alarm)" : "");
    }

    void KwlFan::loop()
    {
        if (!mActive)
            return;

        const uint32_t now = millis();
        const uint32_t elapsed = now - mLastTick;
        if (elapsed < 1000)
            return;

        // Nur volle Sekunden verrechnen, den Rest stehen lassen. Sonst summieren
        // sich die abgeschnittenen Millisekunden ueber Jahre zu einem Fehler.
        const uint32_t seconds = elapsed / 1000;
        mLastTick += seconds * 1000;

        // "Status zyklisch senden": bis 2026-10-06 wirkte der Parameter nur auf
        // die Filter-Restlaufzeit. Jetzt prueft sendStatus() ihn je Objekt.
        if (mStatusValid)
            sendStatus();

        if (mStage > 0)
        {
            mRunSeconds += seconds;
            mFilterSeconds += seconds;

            // Durchgesetzte Luftmenge. In m³·s aufsummiert und erst beim
            // Uebertrag durch 3600 geteilt: 60 m³/h mal eine Sekunde sind 0,017 m³,
            // und eine Division je Sekunde haette das jedes Mal auf 0 gerundet -
            // der Zaehler waere nie vorangekommen.
            mFilterVolumeRemainder += static_cast<uint32_t>(mFlow[mStage]) * seconds;
            mFilterVolume += mFilterVolumeRemainder / 3600u;
            mFilterVolumeRemainder %= 3600u;
        }

        // --- Filterueberwachung ----------------------------------------------
        bool due = false;
        if (mFilterMode == 1)
            due = mFilterLimitSeconds > 0 && mFilterSeconds >= mFilterLimitSeconds;
        else if (mFilterMode == 2)
            due = mFilterLimitVolume > 0 && mFilterVolume >= mFilterLimitVolume;

        // Eine unterdrueckte Meldung kommt nach der Wiederholzeit zurueck.
        if (mFilterMuted && mFilterRemindMs > 0 &&
            (now - mFilterMutedSince) >= mFilterRemindMs)
            mFilterMuted = false;

        if (due != mFilterDue)
        {
            mFilterDue = due;
            if (due)
                mFilterMuted = false;
        }

        // Senden nur bei Aenderung: GroupObject::value() markiert das Objekt
        // immer, und dieser Zweig laeuft jede Sekunde.
        const bool alarm = mFilterDue && !mFilterMuted;
        if (mSentFilterDue.due(alarm, now, 0))
        {
            KoFAN_FilterDue.value(alarm, DPT_Alarm);
            mSentFilterDue.mark(alarm, now);
        }

        // Restlaufzeit in Prozent, fuer die Anzeige.
        if (mFilterMode != 0)
        {
            const uint8_t left = filterLeftPercent();
            if (mSentFilterLeft.due(left, now, mSendCycleStatusMs))
            {
                KoFAN_FilterLeft.value(left, DPT_Scaling);
                mSentFilterLeft.mark(left, now);
            }
        }

        const uint16_t hours = static_cast<uint16_t>(mRunSeconds / 3600u);
        if (mSentHours.due(hours, now, mSendCycleHoursMs))
        {
            KoFAN_RunHours.value(hours, DPT_Value_2_Ucount);
            mSentHours.mark(hours, now);
        }
    }

    float KwlFan::condition(float volt, uint8_t stage) const
    {
        float v = volt;

        // 1. Was am Luefter ankommen soll: der Spannungsabfall auf der
        //    gemeinsamen Masse wird aufgeschlagen. Immer additiv - es verschiebt
        //    sich der Massebezug, nicht das Vorzeichen.
        if (mCableComp && stage > 0)
        {
            const float delta =
                CableComp::deltaVolt(mCurrent[stage > kStageMax ? kStageMax : stage],
                                     mCableResistance);
            v = CableComp::apply(v, delta);
        }

        // 2. Was der Wandler dafuer ausgeben muss: der Steigungsfehler des Kanals
        //    wird herausgerechnet. Der Fehler ist multiplikativ um 0 V, also wird
        //    die absolute Spannung skaliert - auch die 5,00 V des Stillstands.
        v *= mCalib;

        if (v < 0.0f)
            v = 0.0f;
        if (v > kVoltMax)
            v = kVoltMax;
        return v;
    }

    bool KwlFan::drive(KwlOutput& output, const DriveCommand& cmd)
    {
        if (!mActive)
            return true;

        const uint8_t stage = cmd.stage > kStageMax ? kStageMax : cmd.stage;
        const VoltPair nominal =
            Curve::stageVolt(mType, stage, cmd.direction, mBelow5vIsSupply);
        const bool pair = channelsNeeded() == 2;
        const VoltPair v{condition(nominal.a, stage), pair ? condition(nominal.b, stage) : 0.0f};

        // Sicherheitsinvariante 5 je Motor: wechselt die Spannung die Seite von
        // 5 V, ohne dass der Kanal die Totzeit lang bei 5,00 V stand, wird
        // gehalten - egal warum (Abluftstoss des ego, 12-V-Wiederkehr, Start).
        // Beim ego beide Motoren gemeinsam: haelt einer, halten beide.
        const uint32_t now = millis();
        const bool bipolar = Curve::isBipolar(mType);
        const int8_t sideA = DirectionGuard::sideOf(v.a);
        const int8_t sideB = pair ? DirectionGuard::sideOf(v.b) : 0;
        if (bipolar && (!mGuard[0].allow(sideA, now, cmd.deadTimeMs) ||
                        (pair && !mGuard[1].allow(sideB, now, cmd.deadTimeMs))))
            return holdSafe(output, now);

        bool ok;
        if (pair)
            ok = output.setEgo(mDacChannel, v); // Sicherheitsinvariante 6: ein Frame
        else
            ok = output.setVolt(mDacChannel, v.a);
        mLastVolt = v.a;

        if (!ok)
            return false;
        if (bipolar)
        {
            mGuard[0].written(sideA, now);
            if (pair)
                mGuard[1].written(sideB, now);
        }

        // Beim ego ist die Waermerueckgewinnung in den Stufen 1 bis 3 immer aktiv:
        // er pendelt mit seinen zwei Motoren fuer sich, unabhaengig vom Verbund.
        // Erst den wirksamen Wert bilden, DANN vergleichen: bis 2026-10-06 stand
        // hier cmd.hrv != mHrv - beim ego ohne WRG im Takt (Sommerzyklus, feste
        // Richtung) war der Status damit in jedem Durchlauf "geaendert", und alle
        // Statusobjekte sendeten in jeder Schleife (Befund A, Simulator).
        const bool hrv = (mType == FanType::EGO && stage > 0 && stage < kStageMax) ? true : cmd.hrv;
        mStage = stage;
        mDirection = cmd.direction;
        mHrv = hrv;
        mStatusValid = true;

        // sendStatus() entscheidet je Objekt selbst, ob es senden muss.
        sendStatus();
        return true;
    }

    bool KwlFan::driveSafe(KwlOutput& output)
    {
        if (!mActive)
            return true;

        // Ohne Kalibrierung und ohne Kompensation: der sichere Zustand ist der
        // Notweg und soll von so wenig abhaengen wie moeglich. Die Abweichung ist
        // der Steigungsfehler bei 5 V, also wenige Millivolt.
        const float safe = Curve::safeVolt(mType);
        bool ok;
        if (channelsNeeded() == 2)
            ok = output.setEgo(mDacChannel, {safe, safe});
        else
            ok = output.setVolt(mDacChannel, safe);

        if (ok)
            noteSafe(millis());
        if (ok)
        {
            mStage = 0;
            mLastVolt = safe;
            mStatusValid = true;
            sendStatus();
        }
        return ok;
    }

    bool KwlFan::driveHold(KwlOutput& output)
    {
        if (!mActive)
            return true;
        return holdSafe(output, millis());
    }

    bool KwlFan::holdSafe(KwlOutput& output, uint32_t now)
    {
        // Totzeit-Halt je Kanal: 5,00 V schreiben, aber Stufe und Status stehen
        // lassen - der Halt ist Teil des Wechsels, kein Stillstand.
        const float safe = Curve::safeVolt(mType);
        bool ok;
        if (channelsNeeded() == 2)
            ok = output.setEgo(mDacChannel, {safe, safe});
        else
            ok = output.setVolt(mDacChannel, safe);
        if (ok)
        {
            noteSafe(now);
            mLastVolt = safe;
        }
        return ok;
    }

    void KwlFan::restartGuard(uint32_t now)
    {
        mGuard[0].restart(now);
        mGuard[1].restart(now);
    }

    void KwlFan::noteSafe(uint32_t now)
    {
        mGuard[0].written(0, now);
        mGuard[1].written(0, now);
    }

    void KwlFan::sendStatus()
    {
        // Je Objekt nur bei Aenderung oder nach dem Zyklus. Bis 2026-10-06
        // gingen bei jeder Statusaenderung alle sechs Objekte hinaus, und die
        // Totzeit galt als Stufe 0: 12 Telegramme je Pendelwechsel und Luefter
        // (Befund F). Jetzt sind es beim Wechsel Richtung, Spannung und
        // Volumenstrom.
        const uint32_t now = millis();
        const uint32_t cycle = mSendCycleStatusMs;

        if (mSentStage.due(mStage, now, cycle))
        {
            KoFAN_StageAct.value(mStage, DPT_Value_1_Ucount);
            mSentStage.mark(mStage, now);
        }
        const uint8_t pct = StageMap::stageToPercent(mStage);
        if (mSentStagePct.due(pct, now, cycle))
        {
            KoFAN_StageActPct.value(pct, DPT_Scaling);
            mSentStagePct.mark(pct, now);
        }
        // 1 = Zuluft, 0 = Abluft - so steht es in der ETS und in der
        // Applikationsbeschreibung. Bis 2026-10-06 sendete die Firmware genau
        // umgekehrt (1 = Abluft, Befund I).
        const bool supply = mDirection == Direction::Supply;
        if (mSentDir.due(supply, now, cycle))
        {
            KoFAN_DirAct.value(supply, DPT_Switch);
            mSentDir.mark(supply, now);
        }
        if (mSentHrv.due(mHrv, now, cycle))
        {
            KoFAN_HrvActive.value(mHrv, DPT_Switch);
            mSentHrv.mark(mHrv, now);
        }

        // Ausgangsspannung in mV (DPT 9.020), auf ganze mV verglichen.
        const int32_t mv = static_cast<int32_t>(mLastVolt * 1000.0f + 0.5f);
        if (mSentVoltMv.due(mv, now, cycle))
        {
            KoFAN_OutVolt.value(static_cast<float>(mv), Dpt(9, 20));
            mSentVoltMv.mark(mv, now);
        }

        // Volumenstrom, positiv = Zuluft. Die Vorzeichenregel steht im
        // ETS-Funktionstext und macht aus zwei Objekten eines.
        const int16_t flow = static_cast<int16_t>(mDirection == Direction::Supply
                                                      ? mFlow[mStage]
                                                      : -static_cast<int16_t>(mFlow[mStage]));
        if (mSentFlow.due(flow, now, cycle))
        {
            KoFAN_Flow.value(static_cast<float>(flow), Dpt(9, 9));
            mSentFlow.mark(flow, now);
        }
    }

    void KwlFan::sendGroupState(uint8_t stage, bool tact)
    {
        KoFAN_GroupStage.value(stage, DPT_Value_1_Ucount);
        KoFAN_GroupTact.value(tact, DPT_Start);
        // Lebenszeichen: der Slave misst die Abstaende, nicht den Inhalt.
        KoFAN_GroupAlive.value(true, DPT_Switch);
    }

    uint8_t KwlFan::filterLeftPercent() const
    {
        if (mFilterMode == 0)
            return 0;
        const uint32_t used = mFilterMode == 1 ? mFilterSeconds : mFilterVolume;
        const uint32_t limit = mFilterMode == 1 ? mFilterLimitSeconds : mFilterLimitVolume;
        if (limit == 0 || used >= limit)
            return 0;
        return static_cast<uint8_t>(((limit - used) * 100u) / limit);
    }

    void KwlFan::printDiagnose(bool detail)
    {
#ifdef BASE_KoDiagnose
        // 14 Zeichen je Telegramm. Die Zeilen sind so gebaut, dass auch die
        // laengsten Werte hineinpassen: "F12 S4 Z 10025" sind genau 14.
        const unsigned no = _channelIndex + 1;
        if (!mActive)
        {
            openknx.console.writeDiagnoseKo("F%u inaktiv", no);
            return;
        }
        openknx.console.writeDiagnoseKo("F%u S%u %c %d", no, (unsigned)mStage,
                                        mDirection == Direction::Supply ? 'Z' : 'A',
                                        (int)(mLastVolt * 1000.0f));
        if (mLastError != ErrorCode::None)
            openknx.console.writeDiagnoseKo("F%u E%u%s", no, (unsigned)mLastError,
                                            isAlarm(mLastError) ? " ALARM" : "");
        if (detail)
        {
            // "Kal9975 Fi100%" = 14
            openknx.console.writeDiagnoseKo("Kal%d Fi%u%%", (int)(mCalib * 10000.0f),
                                            (unsigned)filterLeftPercent());
            // "Bh65535 Frei" = 12
            openknx.console.writeDiagnoseKo("Bh%u %s", (unsigned)(mRunSeconds / 3600u),
                                            blocked() ? "GESPERRT" : "Frei");
        }
#else
        (void)detail;
#endif
    }

    void KwlFan::printDetail()
    {
        const unsigned no = _channelIndex + 1;
        if (!mActive)
        {
            logInfoP("Luefter %u ist nicht aktiviert", no);
            return;
        }

        static const char* kTypeName[] = {"LUNOS e2-60", "LUNOS ego", "LUNOS RA 15-60",
                                          "generisch bipolar", "generisch unipolar"};
        const uint8_t t = static_cast<uint8_t>(mType);

        logInfoP("Luefter %u", no);
        logIndentUp();
        logInfoP("Typ %s, %u Stellkanal%s ab S%u", t < 5 ? kTypeName[t] : "?",
                 (unsigned)channelsNeeded(), channelsNeeded() == 2 ? "/-kanaele" : "",
                 (unsigned)(mDacChannel + 1));
        logInfoP("Raum %u, Verbund %u, Phase %u, Anteil %u %%", (unsigned)mRoomNo,
                 (unsigned)mGroupNo, (unsigned)mPhase, (unsigned)mShare);
        if (Curve::isBipolar(mType))
            logInfoP("Zuluft %s 5 V, Abluft %s 5 V", mBelow5vIsSupply ? "unter" : "ueber",
                     mBelow5vIsSupply ? "ueber" : "unter");
        logInfoP("Stufe %u %s, %d mV am Ausgang%s", (unsigned)mStage,
                 mDirection == Direction::Supply ? "Zuluft" : "Abluft",
                 (int)(mLastVolt * 1000.0f), mHrv ? ", WRG aktiv" : "");
        logInfoP("Volumenstrom %u m3/h", (unsigned)mFlow[mStage]);

        // Die beiden Korrekturen ausdruecklich, weil sie erklaeren, warum die
        // gemessene Spannung nicht die Zahl aus der Kennlinientabelle ist.
        logInfoP("Kalibrierfaktor %d (10000 = 1,0000)", (int)(mCalib * 10000.0f));
        if (mCableComp)
            logInfoP("Leitungskompensation an: %d mOhm je Leiter",
                     (int)(mCableResistance * 1000.0f));
        else
            logInfoP("Leitungskompensation aus");

        logInfoP("Freigabe %s, %s", mUseEnable ? (mEnableLatched ? "erteilt" : "FEHLT")
                                               : "nicht verlangt",
                 mSuspended ? "SUSPENDIERT" : "in Betrieb");
        logInfoP("Betriebsstunden %u h", (unsigned)(mRunSeconds / 3600u));

        if (mFilterMode == 1)
            logInfoP("Filter nach Laufzeit: %u von %u h%s",
                     (unsigned)(mFilterSeconds / 3600u),
                     (unsigned)(mFilterLimitSeconds / 3600u),
                     mFilterDue ? " - FAELLIG" : "");
        else if (mFilterMode == 2)
            logInfoP("Filter nach Luftmenge: %u von %u m3%s", (unsigned)mFilterVolume,
                     (unsigned)mFilterLimitVolume, mFilterDue ? " - FAELLIG" : "");
        else
            logInfoP("Filterzaehler aus");

        logInfoP("Fehlercode %u%s", (unsigned)mLastError,
                 isAlarm(mLastError) ? " (Alarm)" : "");
        logIndentDown();
    }

    void KwlFan::printStatusLine()
    {
        if (!mActive)
        {
            logInfoP("Kanal %u: nicht aktiv", (unsigned)(_channelIndex + 1));
            return;
        }

        static const char* kTypeName[] = {"e2-60", "ego", "RA15-60", "bipolar", "unipolar"};
        const uint8_t t = static_cast<uint8_t>(mType);

        logInfoP("Kanal %u: %s auf S%u, Raum %u, Verbund %u, Phase %u, %u%%, "
                 "Stufe %u %s, %d mV%s",
                 (unsigned)(_channelIndex + 1),
                 t < 5 ? kTypeName[t] : "?",
                 (unsigned)(mDacChannel + 1),
                 (unsigned)mRoomNo,
                 (unsigned)mGroupNo,
                 (unsigned)mPhase,
                 (unsigned)mShare,
                 (unsigned)mStage,
                 mDirection == Direction::Supply ? "Zuluft" : "Abluft",
                 (int)(mLastVolt * 1000.0f),
                 mHrv ? ", WRG" : "");
    }
} // namespace Kwl
