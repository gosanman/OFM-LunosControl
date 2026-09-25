#include "KwlRoom.h"

#include "knxprod.h"

namespace Kwl
{
    namespace
    {
        /// Laufzeiten stehen in der ETS in Minuten, 0 heisst unendlich.
        uint32_t minutesToMs(uint8_t minutes)
        {
            return static_cast<uint32_t>(minutes) * 60000u;
        }

        /// Intermittierende Abluft: Aktivzeit und Periode in Minuten, Index 0 = dauernd.
        /// Reihenfolge wie im ETS-Enum PT-RoomExhaustInterval.
        struct Intermittent
        {
            uint8_t activeMin;
            uint8_t periodMin;
        };
        constexpr Intermittent kIntermittent[] = {
            {0, 0},  // dauernd
            {1, 5},  {2, 5},  {1, 15}, {2, 15}, {5, 15},
            {1, 30}, {2, 30}, {5, 30}, {10, 30}};

        /// Ist der Messwert vorhanden und nicht zu alt?
        bool fresh(bool valid, uint32_t stamp, uint32_t now, uint32_t timeout)
        {
            if (!valid)
                return false;
            if (timeout == 0)
                return true;
            return (now - stamp) < timeout;
        }
    } // namespace

    // Freigaben je Betriebsart. Die ETS-Makros sind eigene Namen je Betriebsart und
    // lassen sich nicht indizieren; die Zuordnung Betriebsart -> ETS-Praefix steht
    // deshalb genau hier und nirgends sonst.
    KwlRoom::ModeFlags KwlRoom::readModeFlags(OperatingMode mode) const
    {
        ModeFlags f{};
        switch (mode)
        {
#define KWL_MODE_FLAGS(enumName, prefix)                                                   case OperatingMode::enumName:                                                              f.leadHum = ParamROOM_r##prefix##LeadHum;                                              f.leadCo2 = ParamROOM_r##prefix##LeadCO2;                                              f.leadVoc = ParamROOM_r##prefix##LeadVOC;                                              f.leadTemp = ParamROOM_r##prefix##LeadTemp;                                            f.dehum = ParamROOM_r##prefix##Dehum;                                                  f.frost = ParamROOM_r##prefix##Frost;                                                  f.cycleWish = ParamROOM_r##prefix##Cycle;                                              break;

            KWL_MODE_FLAGS(Comfort, Comfort)
            KWL_MODE_FLAGS(Standby, Standby)
            KWL_MODE_FLAGS(Eco, Night)
            KWL_MODE_FLAGS(Protection, Protect)
            KWL_MODE_FLAGS(Boost, Boost)
            KWL_MODE_FLAGS(Reduction, Lower)
            KWL_MODE_FLAGS(Quiet, Quiet)
#undef KWL_MODE_FLAGS
            default:
                break;
        }
        return f;
    }

    KwlRoom::KwlRoom(uint8_t index)
    {
        _channelIndex = index;
    }

    void KwlRoom::setup()
    {
        mActive = ParamROOM_rActive;
        if (!mActive)
            return;

        // --- Parametersatz je Betriebsart (Rang 6) --------------------------
        // Grundstufe laeuft immer, Maximalstufe deckelt die Fuehrungen. Die
        // Reihenfolge folgt OperatingMode, nicht der ETS-Seitenreihenfolge.
        mArbiter.setModeParams(OperatingMode::Comfort,
                               {ParamROOM_rComfortBase, ParamROOM_rComfortMax});
        mArbiter.setModeParams(OperatingMode::Standby,
                               {ParamROOM_rStandbyBase, ParamROOM_rStandbyMax});
        mArbiter.setModeParams(OperatingMode::Eco,
                               {ParamROOM_rNightBase, ParamROOM_rNightMax});
        mArbiter.setModeParams(OperatingMode::Protection,
                               {ParamROOM_rProtectBase, ParamROOM_rProtectMax});
        mArbiter.setModeParams(OperatingMode::Boost,
                               {ParamROOM_rBoostBase, ParamROOM_rBoostMax});
        mArbiter.setModeParams(OperatingMode::Reduction,
                               {ParamROOM_rLowerBase, ParamROOM_rLowerMax});
        mArbiter.setModeParams(OperatingMode::Quiet,
                               {ParamROOM_rQuietBase, ParamROOM_rQuietMax});

        mArbiter.setStandardMode(static_cast<OperatingMode>(ParamROOM_rDefaultMode));

        // --- Ueberlagerungen (Raenge 5 und 7…9) -----------------------------
        mArbiter.setManualRuntime(minutesToMs(ParamROOM_rManualTimeout));
        mArbiter.setForcedModeRuntime(minutesToMs(ParamROOM_rForcedMode));
        mArbiter.setForcedObjectHierarchical(ParamROOM_rForcePrio);
        mArbiter.setForcedObject(1, static_cast<OperatingMode>(ParamROOM_rForce1Mode),
                                 minutesToMs(ParamROOM_rForce1Time));
        mArbiter.setForcedObject(2, static_cast<OperatingMode>(ParamROOM_rForce2Mode),
                                 minutesToMs(ParamROOM_rForce2Time));
        mArbiter.setForcedObject(3, static_cast<OperatingMode>(ParamROOM_rForce3Mode),
                                 minutesToMs(ParamROOM_rForce3Time));

        // Das Nacht-Objekt traegt nicht zwingend "Eco": es kann auch auf
        // Temperatur-Absenkung oder Ruhe gelegt werden.
        mArbiter.setNightMode(static_cast<OperatingMode>(ParamROOM_rNightMode));

        // --- Schutz (Rang 2) und Sperre (Rang 1) ----------------------------
        // Sperrverhalten: 0 = Stillstand, 1 = Grundstufe der Betriebsart. Der
        // Arbiter kennt nur Stillstand; die zweite Spielart wird in loop()
        // nachgebildet, damit Rang 1 eine einzige Stelle bleibt.
        mLockKeepsBase = ParamROOM_rLockBehaviour;

        mTemp.setFrostLimit(static_cast<float>(ParamROOM_rFrostTemp));
        mTemp.setHeatLimit(static_cast<float>(ParamROOM_rHeatTemp));
        mTemp.setDistance(static_cast<float>(ParamROOM_rTempGap));
        mTemp.setStages(ParamROOM_rCoolStage, ParamROOM_rHeatStage);

        // --- Fuehrungen (Rang 6) --------------------------------------------
        const float humidity[kStageMax + 1] = {
            static_cast<float>(ParamROOM_rHumGW0), static_cast<float>(ParamROOM_rHumGW1),
            static_cast<float>(ParamROOM_rHumGW2), static_cast<float>(ParamROOM_rHumGW3),
            static_cast<float>(ParamROOM_rHumGW4)};
        mHumidity.setThresholds(humidity);

        const float co2[kStageMax + 1] = {
            static_cast<float>(ParamROOM_rCO2GW0), static_cast<float>(ParamROOM_rCO2GW1),
            static_cast<float>(ParamROOM_rCO2GW2), static_cast<float>(ParamROOM_rCO2GW3),
            static_cast<float>(ParamROOM_rCO2GW4)};
        mCo2.setThresholds(co2);

        const float voc[kStageMax + 1] = {
            static_cast<float>(ParamROOM_rVOCGW0), static_cast<float>(ParamROOM_rVOCGW1),
            static_cast<float>(ParamROOM_rVOCGW2), static_cast<float>(ParamROOM_rVOCGW3),
            static_cast<float>(ParamROOM_rVOCGW4)};
        mVoc.setThresholds(voc);

        mCompare.setThresholds(static_cast<float>(ParamROOM_rDehumOn) / 10.0f,
                               static_cast<float>(ParamROOM_rDehumOff) / 10.0f);

        // --- Sensorik --------------------------------------------------------
        mSensorTimeout = static_cast<uint32_t>(ParamROOM_rSensorTimeout) * 60000u;
        mStopOnMissing = ParamROOM_rOnMissing;
        mVocUnit = ParamROOM_rVOCUnit;
        mAltitude = static_cast<float>(ParamROOM_Altitude);

        // --- Abluftanforderung ----------------------------------------------
        mExhaustStage = ParamROOM_rExhaustStage;
        mExhaustLeadMs = static_cast<uint32_t>(ParamROOM_rExhaustLead) * 1000u;
        mExhaustLagMs = minutesToMs(ParamROOM_rExhaustLag);
        mExhaustIntervalIdx = ParamROOM_rExhaustInterval;
        mSendSupplyReq = ParamROOM_rExhaustSupply;

        // --- Intervallbetrieb ------------------------------------------------
        mIntervalPeriodMs = static_cast<uint32_t>(ParamROOM_rIntervalPeriod) * 60000u;
        mIntervalActiveMs = static_cast<uint32_t>(ParamROOM_rIntervalActive) * 60000u;

        logDebugP("Raum bereit, Standard-Betriebsart %u, Sensorzeit %u min",
                  (unsigned)ParamROOM_rDefaultMode, (unsigned)ParamROOM_rSensorTimeout);
    }

    // ------------------------------------------------------------ Eingaenge

    void KwlRoom::processInputKo(GroupObject& ko)
    {
        if (!mActive)
            return;

        const int index = ROOM_KoCalcIndex(ko.asap());
        const uint32_t now = millis();

        switch (index)
        {
            // --- Rang 1: Sperre ---------------------------------------------
            case ROOM_KoLock:
                mLocked = ko.value(DPT_Switch);
                mArbiter.setLock(mLocked && !mLockKeepsBase);
                break;

            // --- Rang 5: Handstufe ------------------------------------------
            case ROOM_KoStageSet:
                // DPT 5.100 Luefterstufe, nicht 5.010.
                mArbiter.setManualStage(ko.value(DPT_Value_1_Ucount), now);
                break;

            case ROOM_KoStageSetPct:
                // Prozent ist Eingabeformat, Stufe ist Wahrheit: die Uebersetzung
                // findet hier am Rand statt und geht nicht weiter nach innen.
                mArbiter.setManualStage(mPercent.update(ko.value(DPT_Scaling)), now);
                break;

            case ROOM_KoStageStep:
                mArbiter.stepManual(ko.value(DPT_Step) ? +1 : -1, now);
                break;

            case ROOM_KoManualAct:
                // Ein- und Ausgang: eine 0 vom Bus beendet den Handbetrieb sofort.
                mArbiter.setManualActive(ko.value(DPT_Switch), now);
                break;

            // --- Raenge 7…10: Betriebsart -----------------------------------
            case ROOM_KoMode:
            {
                // KNXValue laesst sich nicht direkt casten - erst in den
                // Zahlenwert, dann in die Betriebsart.
                const uint8_t mode = ko.value(DPT_HVACMode);
                mArbiter.setMode(static_cast<OperatingMode>(mode), now);
                break;
            }

            case ROOM_KoForcedMode:
            {
                const uint8_t mode = ko.value(DPT_HVACMode);
                if (mode == 0)
                    mArbiter.clearForcedMode(now);
                else
                    mArbiter.setForcedMode(static_cast<OperatingMode>(mode), now);
                break;
            }

            case ROOM_KoNight:
                mArbiter.setNight(ko.value(DPT_Switch), now);
                break;

            case ROOM_KoForce1:
                mArbiter.setForcedObjectState(1, ko.value(DPT_Switch), now);
                break;
            case ROOM_KoForce2:
                mArbiter.setForcedObjectState(2, ko.value(DPT_Switch), now);
                break;
            case ROOM_KoForce3:
                mArbiter.setForcedObjectState(3, ko.value(DPT_Switch), now);
                break;

            // --- Rang 6: Messwerte -------------------------------------------
            case ROOM_KoHumIn:
                mHumIn.set(ko.value(DPT_Value_Humidity), now);
                break;
            case ROOM_KoTempIn:
                mTempIn.set(ko.value(DPT_Value_Temp), now);
                break;
            case ROOM_KoHumOut:
                mHumOut.set(ko.value(DPT_Value_Humidity), now);
                break;
            case ROOM_KoTempOut:
                mTempOut.set(ko.value(DPT_Value_Temp), now);
                break;
            case ROOM_KoTempSet:
                mTempSet.set(ko.value(DPT_Value_Temp), now);
                break;
            case ROOM_KoCO2:
                mCo2Val.set(ko.value(DPT_Value_AirQuality), now);
                break;

            case ROOM_KoVOC:
                // ppb als 9.008. Steht der Parameter auf Index, gilt das andere KO.
                if (mVocUnit == 0)
                    mVocVal.set(ko.value(DPT_Value_AirQuality), now);
                break;
            case ROOM_KoVOCByte:
                if (mVocUnit == 1)
                    mVocVal.set(ko.value(DPT_Value_1_Ucount), now);
                else if (mVocUnit == 2)
                    mVocVal.set(ko.value(DPT_Value_2_Ucount), now);
                break;

            // --- Freigaben der Fuehrungen (Arcus Obj 13/16/18) ----------------
            case ROOM_KoHumLead:
                mLeadHumKo = ko.value(DPT_Switch);
                break;
            case ROOM_KoCO2Lead:
                mLeadCo2Ko = ko.value(DPT_Switch);
                break;
            case ROOM_KoVOCLead:
                mLeadVocKo = ko.value(DPT_Switch);
                break;
            case ROOM_KoTempLead:
                // Nur als Eingang: als Ausgang meldet dasselbe KO die freie
                // Kuehlung an die Heizungsregelung (Parameter rTempLeadMode).
                if (!ParamROOM_rTempLeadMode)
                    mLeadTempKo = ko.value(DPT_Switch);
                break;

            case ROOM_KoSummer:
                mSummer = ko.value(DPT_Switch);
                break;

            // --- Rang 3: Betriebsweise und Abluftanforderung ------------------
            case ROOM_KoDirMode:
                // 0 = auto, 1 = WRG, 2 = Zuluft, 3 = Abluft.
                mDirMode = ko.value(DPT_Value_1_Ucount);
                break;

            case ROOM_KoExhaustReq:
                mExhaustReq = ko.value(DPT_Switch);
                break;

            default:
                // Betriebsweise-KO, Abluftanforderung und Intervallbetrieb folgen
                // im naechsten Schritt.
                break;
        }
    }

    // ------------------------------------------------------------ Ablauf

    void KwlRoom::expireSensors(uint32_t now)
    {
        Sensor* all[] = {&mHumIn, &mTempIn, &mHumOut, &mTempOut, &mCo2Val, &mVocVal,
                         &mTempSet};
        for (Sensor* s : all)
            if (s->valid && !fresh(true, s->stamp, now, mSensorTimeout))
                s->valid = false;
    }

    bool KwlRoom::intervalActive(uint32_t now) const
    {
        if (mIntervalPeriodMs == 0 || mIntervalActiveMs >= mIntervalPeriodMs)
            return true; // entartete Parametrierung: dauernd lueften
        return (now % mIntervalPeriodMs) < mIntervalActiveMs;
    }

    uint8_t KwlRoom::applyRunOn(uint8_t guidance, uint8_t runOnMinutes, uint32_t now)
    {
        // Nachlauf nach Anforderungsende: faellt der Wunsch der Fuehrungen, bleibt
        // er fuer die parametrierte Zeit stehen. Ein Bad soll nach dem Duschen
        // nicht in dem Augenblick abschalten, in dem die Feuchte unter den
        // Grenzwert rutscht - die Luft ist dann noch nicht draussen.
        if (guidance >= mGuidanceHold)
        {
            mGuidanceHold = guidance;
            mGuidanceHoldSince = now;
            return guidance;
        }

        const uint32_t runOn = minutesToMs(runOnMinutes);
        if (runOn != 0 && (now - mGuidanceHoldSince) < runOn)
            return mGuidanceHold;

        mGuidanceHold = guidance;
        mGuidanceHoldSince = now;
        return guidance;
    }

    void KwlRoom::runExhaust(const ModeFlags& flags, uint32_t now)
    {
        (void)flags;

        switch (mExhaust)
        {
            case ExhaustState::Idle:
                if (mExhaustReq)
                {
                    mExhaust = mExhaustLeadMs > 0 ? ExhaustState::Lead
                                                  : ExhaustState::Active;
                    mExhaustSince = now;
                }
                break;

            case ExhaustState::Lead:
                // Wird die Anforderung im Vorlauf zurueckgenommen, ist nichts
                // geschehen - kein Nachlauf fuer eine Lueftung, die nie lief.
                if (!mExhaustReq)
                    mExhaust = ExhaustState::Idle;
                else if ((now - mExhaustSince) >= mExhaustLeadMs)
                {
                    mExhaust = ExhaustState::Active;
                    mExhaustSince = now;
                }
                break;

            case ExhaustState::Active:
                if (!mExhaustReq)
                {
                    mExhaust = mExhaustLagMs > 0 ? ExhaustState::Lag
                                                 : ExhaustState::Idle;
                    mExhaustSince = now;
                }
                break;

            case ExhaustState::Lag:
                if (mExhaustReq)
                {
                    mExhaust = ExhaustState::Active;
                    mExhaustSince = now;
                }
                else if ((now - mExhaustSince) >= mExhaustLagMs)
                    mExhaust = ExhaustState::Idle;
                break;
        }

        bool running = (mExhaust == ExhaustState::Active || mExhaust == ExhaustState::Lag);

        // Intermittierend: innerhalb der Anforderung wird nur die Aktivzeit je
        // Periode gefoerdert. In der Pause faellt der Raum auf seine Automatik
        // zurueck, statt bei Stufe 0 zu stehen - die Grundlueftung laeuft weiter.
        if (running && mExhaustIntervalIdx > 0 &&
            mExhaustIntervalIdx < (sizeof(kIntermittent) / sizeof(kIntermittent[0])))
        {
            const Intermittent& iv = kIntermittent[mExhaustIntervalIdx];
            const uint32_t period = static_cast<uint32_t>(iv.periodMin) * 60000u;
            const uint32_t active = static_cast<uint32_t>(iv.activeMin) * 60000u;
            if (period > 0 && ((now - mExhaustSince) % period) >= active)
                running = false;
        }

        mArbiter.setAirDemand(running, Direction::Exhaust, mExhaustStage);

        // Zuluftanforderung an die Partner: solange dieser Knoten Abluft faehrt,
        // muss anderswo nachstroemen, sonst pfeift es an den Fenstern.
        KoROOM_SupplyReq.value(running && mSendSupplyReq, DPT_Switch);
    }

    uint8_t KwlRoom::runGuidance(OperatingMode mode, uint32_t now)
    {
        (void)now;
        const ModeFlags f = readModeFlags(mode);
        uint8_t wish = 0;

        // --- Feuchtevergleich ------------------------------------------------
        // Er entscheidet, ob Lueften ueberhaupt trocknet. Ohne Aussenwerte bleibt
        // er gesperrt: nicht gemessen heisst nicht entfeuchten.
        bool dehumAllowed = false;
        if (f.dehum && mHumIn.valid && mTempIn.valid && mHumOut.valid && mTempOut.valid)
        {
            const float eIn = MoistAir::vapourPressure(mTempIn.value, mHumIn.value);
            const float eOut = MoistAir::vapourPressure(mTempOut.value, mHumOut.value);
            dehumAllowed = mCompare.update(eIn, eOut);
        }
        else
        {
            mCompare.reset();
        }
        mDehumBlocked = f.dehum && !dehumAllowed;

        // --- Grenzwert-Treppen -----------------------------------------------
        // Die rF-Treppe laeuft nur, wenn Lueften auch trocknet. Steht draussen
        // feuchtere Luft, macht mehr Lueften den Raum feuchter statt trockener.
        if (f.leadHum && mLeadHumKo && mHumIn.valid && dehumAllowed)
        {
            const uint8_t s = mHumidity.update(mHumIn.value);
            if (s > wish)
                wish = s;
        }
        else
        {
            mHumidity.reset();
        }

        if (f.leadCo2 && mLeadCo2Ko && mCo2Val.valid)
        {
            const uint8_t s = mCo2.update(mCo2Val.value);
            if (s > wish)
                wish = s;
        }
        else
        {
            mCo2.reset();
        }

        if (f.leadVoc && mLeadVocKo && mVocVal.valid)
        {
            const uint8_t s = mVoc.update(mVocVal.value);
            if (s > wish)
                wish = s;
        }
        else
        {
            mVoc.reset();
        }

        // --- Temperaturfuehrung und Schutz ------------------------------------
        uint8_t cycleWish = f.cycleWish;
        if (mTempIn.valid && mTempOut.valid && mTempSet.valid)
        {
            const TempResult t =
                mTemp.update(mTempIn.value, mTempOut.value, mTempSet.value);

            // Frostschutz gilt nur, wenn die Betriebsart ihn zulaesst - im
            // Gebaeudeschutz immer.
            mProtection = (t.frostProtection && (f.frost || mode == OperatingMode::Protection)) ||
                          t.heatProtection;

            if (f.leadTemp && mLeadTempKo)
            {
                if (t.stageRequest > wish)
                    wish = t.stageRequest;
                // Die Temperaturfuehrung darf den Takt vorgeben, aber nur wenn die
                // Betriebsart ihn nicht fest vorschreibt.
                if (t.cycle != CycleRule::FromOperatingMode && cycleWish <= 2)
                    cycleWish = static_cast<uint8_t>(t.cycle);
            }
        }
        else
        {
            mProtection = false;
        }

        applyCycleWish(cycleWish);
        return wish;
    }

    void KwlRoom::applyCycleWish(uint8_t wish)
    {
        // ETS: 0 = ueber das Objekt "Sommer", 1 = WRG, 2 = Sommer,
        //      3 = feste Richtung Zuluft, 4 = feste Richtung Abluft.
        switch (wish)
        {
            case 1:
                mCycleWish = CycleRule::Wrg;
                break;
            case 2:
                mCycleWish = CycleRule::Summer;
                break;
            case 3:
            case 4:
                // Feste Richtung ist kein Takt, sondern eine Forderung an den
                // Verbund. Der Takt bleibt WRG, damit die Zykluszeit definiert ist.
                mCycleWish = CycleRule::Wrg;
                break;
            case 0:
            default:
                mCycleWish = mSummer ? CycleRule::Summer : CycleRule::Wrg;
                break;
        }
    }

    void KwlRoom::loop()
    {
        if (!mActive)
            return;

        const uint32_t ms = millis();
        expireSensors(ms);

        const StageResult before = mArbiter.result();

        // Erst die Betriebsart feststellen - die Freigaben der Fuehrungen haengen
        // an ihr. Dann rechnen, dann noch einmal bewerten, damit das Ergebnis
        // nicht einen Durchlauf hinterherhinkt.
        const OperatingMode mode = mArbiter.update(ms).mode;
        const ModeFlags flags = readModeFlags(mode);
        uint8_t guidance = runGuidance(mode, ms);

        // Fehlen Messwerte, gilt der Parameter "bei fehlenden Messwerten":
        // Grundstufe weiterfahren oder Stillstand.
        mSensorsMissing = !mHumIn.valid && !mCo2Val.valid && !mVocVal.valid;
        if (mSensorsMissing && mStopOnMissing)
            guidance = 0;

        guidance = applyRunOn(guidance, flags.runOn, ms);

        // Intervallbetrieb: in der Pause gibt Rang 6 nichts aus, Grundstufe
        // eingeschlossen. Hand, Schutz und Sperre bleiben davon unberuehrt.
        mIntervalRunning = !flags.interval || intervalActive(ms);
        mArbiter.setAutomaticSuppressed(!mIntervalRunning);

        // Betriebsweise-KO: 2 und 3 erzwingen eine Richtung, 1 bleibt beim Takt.
        mArbiter.setDirectionOverride(mDirMode >= 2,
                                      mDirMode == 2 ? Direction::Supply
                                                    : Direction::Exhaust);
        if (mDirMode == 1)
            mCycleWish = CycleRule::Wrg;

        runExhaust(flags, ms);

        mArbiter.setGuidanceStage(guidance);
        mArbiter.setProtection(mProtection);

        StageResult now = mArbiter.update(ms);

        // Sperrverhalten "Grundstufe der Betriebsart": der Arbiter bleibt
        // unangetastet, die Ausgabe wird auf die Grundstufe begrenzt. So bleibt
        // Rang 1 eine einzige Stelle, und nach dem Entsperren steht wieder das
        // da, was ohne die Sperre gegolten haette.
        if (mLocked && mLockKeepsBase)
        {
            const uint8_t base = mArbiter.modeParams(now.mode).baseStage;
            if (now.stage > base)
            {
                now.stage = base;
                now.source = StageSource::Lock;
            }
        }
        mStage = now;

        if (now.stage != before.stage || now.source != before.source)
            sendStage();
        if (now.mode != before.mode)
            sendMode();

        sendHumidity();
        KoROOM_ProtectAct.value(mProtection, DPT_Switch);
        KoROOM_DehumBlock.value(mDehumBlocked, DPT_Switch);
        KoROOM_IntervalAct.value(mIntervalRunning, DPT_Switch);
        KoROOM_DirModeAct.value(mDirMode, DPT_Value_1_Ucount);
    }

    void KwlRoom::sendHumidity()
    {
        // Anzeige in g/kg. Der Vergleich selbst laeuft ueber den Partialdruck und
        // braucht die Hoehe nicht - nur diese beiden Objekte tun es.
        if (mHumIn.valid && mTempIn.valid)
            KoROOM_AbsHumIn.value(
                MoistAir::mixingRatioAt(mTempIn.value, mHumIn.value, mAltitude),
                Dpt(9, 29));
        if (mHumOut.valid && mTempOut.valid)
            KoROOM_AbsHumOut.value(
                MoistAir::mixingRatioAt(mTempOut.value, mHumOut.value, mAltitude),
                Dpt(9, 29));
    }

    void KwlRoom::sendStage()
    {
        KoROOM_DemandStage.value(mStage.stage, DPT_Value_1_Ucount);
        KoROOM_DemandPct.value(StageMap::stageToPercent(mStage.stage), DPT_Scaling);
    }

    void KwlRoom::sendMode()
    {
        KoROOM_ModeAct.value(static_cast<uint8_t>(mStage.mode), DPT_HVACMode);
    }

    // ------------------------------------------------------------ Konsole

    void KwlRoom::printStatusLine()
    {
        if (!mActive)
        {
            logInfoP("Raum %u: nicht aktiv", (unsigned)(_channelIndex + 1));
            return;
        }

        const StageResult r = mArbiter.result();
        logInfoP("Raum %u: Stufe %u aus Rang %u, Betriebsart %u von Ebene %u%s",
                 (unsigned)(_channelIndex + 1), (unsigned)mStage.stage,
                 (unsigned)static_cast<uint8_t>(r.source),
                 (unsigned)static_cast<uint8_t>(r.mode), (unsigned)r.modeRank,
                 mLocked ? ", gesperrt" : "");
    }
} // namespace Kwl
