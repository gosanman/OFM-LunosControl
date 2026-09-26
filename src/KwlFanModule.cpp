#include "KwlFanModule.h"

#include "Drive/Gp8413Drive.h"
#include "KwlRoomModule.h"

Kwl::KwlFanModule openknxKwlFanModule;

namespace Kwl
{
    namespace
    {
        /// Parameter eines Verbunds. Die ETS-Makros sind je Verbund eigene Namen
        /// und lassen sich nicht indizieren - deshalb diese Tabelle statt einer
        /// Schleife ueber %n%. Sie ist stumpf, aber sie ist an einer Stelle.
        struct GroupParams
        {
            uint8_t role;
            uint16_t heartbeat;
            uint16_t masterTimeout;
            uint8_t followSupply;
            uint8_t stageRule;
            uint8_t cycleConflict;
            bool active;
            uint8_t leadRoom;
            uint8_t deadTime;
            uint16_t cycle[kStageMax + 1]; // Index 1…4
            uint16_t summer;
        };

        /// KO-Index innerhalb des Kanalblocks, ohne _channelIndex - das Makro
        /// FAN_KoCalcIndex rechnet darueber und gibt es nur im Kanal.
        int koIndex(uint16_t asap)
        {
            if (asap < FAN_KoBlockOffset)
                return -1;
            const uint16_t rel = asap - FAN_KoBlockOffset;
            if (rel >= FAN_ChannelCount * FAN_KoBlockSize)
                return -1;
            return rel % FAN_KoBlockSize;
        }

        GroupParams readGroup(uint8_t group)
        {
            GroupParams p{};
            switch (group)
            {
#define KWL_READ_GROUP(n)                                                              \
    case n:                                                                            \
        p.role = ParamFAN_Grp##n##Role;                                                \
        p.heartbeat = ParamFAN_Grp##n##Heartbeat;                                      \
        p.masterTimeout = ParamFAN_Grp##n##MasterTimeout;                              \
        p.followSupply = ParamFAN_Grp##n##FollowSupply;                                \
        p.stageRule = ParamFAN_Grp##n##StageRule;                                      \
        p.cycleConflict = ParamFAN_Grp##n##CycleRule;                                  \
        p.active = ParamFAN_Grp##n##Active;                                            \
        p.leadRoom = ParamFAN_Grp##n##LeadRoom;                                        \
        p.deadTime = ParamFAN_Grp##n##DeadTime;                                        \
        p.cycle[1] = ParamFAN_Grp##n##CycleS1;                                         \
        p.cycle[2] = ParamFAN_Grp##n##CycleS2;                                         \
        p.cycle[3] = ParamFAN_Grp##n##CycleS3;                                         \
        p.cycle[4] = ParamFAN_Grp##n##CycleS4;                                         \
        p.summer = ParamFAN_Grp##n##CycleSummer;                                       \
        break;

                KWL_READ_GROUP(1)
                KWL_READ_GROUP(2)
                KWL_READ_GROUP(3)
                KWL_READ_GROUP(4)
                KWL_READ_GROUP(5)
                KWL_READ_GROUP(6)
                KWL_READ_GROUP(7)
                KWL_READ_GROUP(8)
#undef KWL_READ_GROUP
                default:
                    break;
            }
            return p;
        }
    } // namespace

    bool KwlFanModule::checkHardware()
    {
        // Die ETS bietet mehr Platinen an, als in diesem Geraet steckt. Stimmen
        // Auswahl und Bestueckung nicht ueberein, waeren alle Kanalzuordnungen
        // verschoben - und eine verschobene Zuordnung stellt den falschen Luefter
        // auf Vollgas. Deshalb Fehlercode 3 und kein stilles Weiterlaufen.
        if (ParamFAN_Hardware == boardId())
            return true;

        logErrorP("Hardwareauswahl %u passt nicht zur Platine (%u, %u Kanaele) - "
                  "alle Ausgaenge bleiben im sicheren Zustand",
                  (unsigned)ParamFAN_Hardware, (unsigned)boardId(),
                  (unsigned)boardChannels());
        mError = ErrorCode::Configuration;
        return false;
    }

    void KwlFanModule::setupChannels()
    {
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            if (mFan[i] == nullptr)
                mFan[i] = new KwlFan(i);
            mFan[i]->setup();

            // Der Typ entscheidet ueber den sicheren Zustand des DAC-Kanals, muss
            // also VOR KwlOutput::begin() stehen.
            if (mFan[i]->isActive())
            {
                mOutput.setChannelType(mFan[i]->dacChannel(), mFan[i]->type());
                if (mFan[i]->channelsNeeded() == 2)
                    mOutput.setChannelType(mFan[i]->dacChannel() + 1, mFan[i]->type());
            }
        }
    }

    void KwlFanModule::setupGroups()
    {
        for (uint8_t g = 0; g < kGroupsMax; g++)
        {
            const GroupParams p = readGroup(g + 1);
            mGroupActive[g] = p.active;
            if (!p.active)
                continue;

            mGroupRole[g] = static_cast<GroupRole>(p.role);
            mGroupLeadRoom[g] = p.leadRoom;
            mGroupFollowSupply[g] = p.followSupply;
            mGroupHeartbeatMs[g] = static_cast<uint32_t>(p.heartbeat) * 1000u;
            mGroupTimeoutMs[g] = static_cast<uint32_t>(p.masterTimeout) * 1000u;
            mGroupSpeaker[g] = -1;
            mGroupLastAlive[g] = millis();
            mGroupTimedOut[g] = false;

            // Die Gruppen-KOs haengen am Luefter mit der kleinsten Nummer in
            // diesem Verbund - deterministisch und ohne eigenen Parameter.
            for (uint8_t i = 0; i < FAN_ChannelCount; i++)
                if (mFan[i] != nullptr && mFan[i]->isActive() &&
                    mFan[i]->groupNo() == g + 1)
                {
                    mGroupSpeaker[g] = static_cast<int8_t>(i);
                    break;
                }

            mGroup[g].setStageRule(static_cast<GroupStageRule>(p.stageRule));
            mGroup[g].setCycleConflict(static_cast<GroupCycleConflict>(p.cycleConflict));
            mGroup[g].setLeadRoom(p.leadRoom);
            mGroup[g].setDeadTime(p.deadTime);
            mGroup[g].setSummerCycleTime(p.summer);
            for (uint8_t s = 1; s <= kStageMax; s++)
                mGroup[g].setCycleTime(s, p.cycle[s]);
        }
    }

    void KwlFanModule::setup(bool configured)
    {
        mBelow5vIsSupply = boardBelow5vIsSupply();
        mOutput.attach(&boardDacDrive(), boardLeftAligned());

        if (!configured)
        {
            // Ohne gueltige Parametrierung wird nicht gefahren - aber die Ausgaenge
            // muessen trotzdem definiert stehen.
            mOutput.begin();
            mOutput.safeAll();
            return;
        }

        // Kanaele und Verbuende VOR der Hardwarepruefung: schlaegt sie fehl,
        // muessen die Luefter den Fehlercode trotzdem auf den Bus bringen koennen.
        setupChannels();
        setupGroups();
        setupButton();

        // Beide Gruppen existieren immer; ob die LED der Platine an einer haengt,
        // entscheidet die ETS. active() ist sonst falsch, und updateLed() tut nichts.
        mLedAll = openknx.ledFunctions.get(kLedFunctionAll);
        mLedFaults = openknx.ledFunctions.get(kLedFunctionFaultsOnly);
        mLedShown = 0xFF;

        if (!checkHardware())
        {
            mOutput.begin();
            mOutput.safeAll();
            return;
        }

        // Reihenfolge nach Sicherheitsinvariante 3: Bereichsregister, sicherer
        // Zustand, erst danach Sollwerte. begin() macht genau das.
        if (!mOutput.begin())
        {
            mError = mOutput.error();
            logErrorP("DAC nicht erreichbar - Fehlercode %u", (unsigned)mError);
            return;
        }

        logInfoP("%u Stellkanaele bereit, Busformat %s", (unsigned)boardChannels(),
                 boardLeftAligned() ? "linksbuendig" : "rechtsbuendig");
    }

    // ------------------------------------------------------------ Ablauf

    void KwlFanModule::loop(bool configured)
    {
        if (!configured)
            return;

        // Ein Fehler aus setup() oder aus dem Betrieb (Konfiguration, DAC) laesst
        // die Ausgaenge im sicheren Zustand - aber er wird GEMELDET, nicht
        // verschwiegen (Invariante 10). Der Fehlercode geht an jeden Luefter.
        if (mError != ErrorCode::None || !mOutput.ready())
        {
            for (uint8_t i = 0; i < FAN_ChannelCount; i++)
                if (mFan[i] != nullptr)
                    mFan[i]->sendFault(false, mError, ErrorCode::None);
            updateLed();
            return;
        }

        const uint32_t now = millis();

        // Geste der Funktionstaste, gesetzt im Interrupt, ausgefuehrt hier.
        const uint8_t pending = mButtonPending;
        if (pending != 0)
        {
            mButtonPending = 0;
            handleButton(pending == 1 ? mButtonShort
                       : pending == 2 ? mButtonLong
                                      : mButtonDouble, now);
        }

        // Zaehler und Filterueberwachung laufen unabhaengig vom Verbund.
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
            if (mFan[i] != nullptr)
                mFan[i]->loop();

        for (uint8_t g = 0; g < kGroupsMax; g++)
        {
            if (!mGroupActive[g])
                continue;
            driveGroup(g, now);
        }

        updateLed();
    }

    // ------------------------------------------------------------ LED

    void KwlFanModule::updateLed()
    {
        const bool all = mLedAll != nullptr && mLedAll->active();
        const bool faultsOnly = mLedFaults != nullptr && mLedFaults->active();
        if (!all && !faultsOnly)
            return;

        // Der schlimmste Zustand ueber alle Luefter - dieselbe Regel wie beim
        // Fehlercode: der kleinste anliegende Code gewinnt.
        ErrorCode worst = mError;
        bool running = false;
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            const KwlFan* fan = mFan[i];
            if (fan == nullptr || !fan->isActive())
                continue;
            worst = lowestError(worst, fan->lastError());
            if (fan->stage() > 0)
                running = true;
        }

        // 0 aus, 1 an, 2 Meldung (blinkt), 0x80|Code Alarm (Blinkcode).
        uint8_t want;
        if (isAlarm(worst))
            want = 0x80 | static_cast<uint8_t>(worst);
        else if (worst != ErrorCode::None)
            want = 2;
        else
            want = (all && running) ? 1 : 0;

        if (want == mLedShown)
            return;
        mLedShown = want;

        OpenKNX::Led::FunctionGroup* led = all ? mLedAll : mLedFaults;
        if (want & 0x80)
        {
            // Die "rote LED" der Invariante 10: OpenKNX zeigt den Code als
            // Blinkfolge - so viele Blitze wie der Code, dann Pause.
            led->errorCode(want & 0x7F);
            return;
        }
        led->errorCode(0);
        if (want == 2)
            led->blinking();
        else if (want == 1)
            led->on();
        else
            led->off();
    }

    void KwlFanModule::flashLed()
    {
        // Quittung fuer eine Geste. Danach den gemerkten Zustand vergessen, damit
        // updateLed() die Anzeige wieder aufbaut.
        OpenKNX::Led::FunctionGroup* led =
            (mLedAll != nullptr && mLedAll->active()) ? mLedAll
          : (mLedFaults != nullptr && mLedFaults->active()) ? mLedFaults
          : nullptr;
        if (led == nullptr)
            return;
        led->flash(1);
        mLedShown = 0xFF;
    }

    // ------------------------------------------------------------ Funktionstaste

    void KwlFanModule::setupButton()
    {
        mButtonShort = static_cast<ButtonAction>(ParamFAN_ButtonShort);
        mButtonLong = static_cast<ButtonAction>(ParamFAN_ButtonLong);
        mButtonDouble = static_cast<ButtonAction>(ParamFAN_ButtonDouble);

#ifdef FUNC1_BUTTON_PIN
        // Die Rueckrufe laufen im Timer-Interrupt: nur merken, nichts tun.
        openknx.func1Button.onShortClick([this] { mButtonPending = 1; });
        openknx.func1Button.onLongClick([this] { mButtonPending = 2; });
        openknx.func1Button.onDoubleClick([this] { mButtonPending = 3; });
#endif
    }

    void KwlFanModule::toggleForcedMode(OperatingMode mode, uint32_t now)
    {
        // Steht die Betriebsart in irgendeinem Raum schon als Zwang, nimmt die
        // Geste sie ueberall zurueck - sonst setzt sie sie ueberall. So kommt man
        // mit einer Taste immer in beide Richtungen.
        bool anyActive = false;
        for (uint8_t r = 1; r <= ROOM_ChannelCount; r++)
        {
            KwlRoom* room = openknxKwlRoomModule.room(r);
            if (room != nullptr && room->forcedModeActive(mode))
                anyActive = true;
        }
        for (uint8_t r = 1; r <= ROOM_ChannelCount; r++)
        {
            KwlRoom* room = openknxKwlRoomModule.room(r);
            if (room == nullptr)
                continue;
            if (anyActive)
                room->clearForcedModeFromDevice(now);
            else
                room->forceModeFromDevice(mode, now);
        }
        logInfoP("Taste: %s %s", mode == OperatingMode::Boost ? "Stosslueften" : "Ruhe",
                 anyActive ? "aus" : "ein");
    }

    void KwlFanModule::handleButton(ButtonAction action, uint32_t now)
    {
        switch (action)
        {
            case ButtonAction::Boost:
                toggleForcedMode(OperatingMode::Boost, now);
                break;
            case ButtonAction::Quiet:
                toggleForcedMode(OperatingMode::Quiet, now);
                break;
            case ButtonAction::FilterAck:
            {
                // Nur faellige Filter: ein versehentlicher Druck setzt keinen
                // Zaehler zurueck, der noch laeuft.
                uint8_t count = 0;
                for (uint8_t i = 0; i < FAN_ChannelCount; i++)
                    if (mFan[i] != nullptr && mFan[i]->isActive() && mFan[i]->filterDue())
                    {
                        mFan[i]->acknowledgeFilter();
                        count++;
                    }
                logInfoP("Taste: %u Filter quittiert", (unsigned)count);
                break;
            }
            case ButtonAction::None:
            default:
                return; // keine Quittung fuer eine Geste ohne Wirkung
        }
        flashLed();
    }

    void KwlFanModule::collectMembers(uint8_t group)
    {
        KwlGroup& grp = mGroup[group];
        grp.clearMembers();

        // Slave: der Verbund rechnet nicht aus den Raeumen, sondern folgt dem, was
        // der Master sendet. Das kommt als EIN Mitglied mit Richtungsforderung
        // herein - damit laeuft dieselbe Zustandsmaschine, samt Totzeit beim
        // Wechsel der Halbwelle.
        if (mGroupRole[group] == GroupRole::Slave)
        {
            GroupMember m{};
            m.roomNo = 0;
            m.stage = mGroupRxStage[group];
            m.maxStage = kStageMax;
            m.cycleWish = CycleRule::Wrg;
            m.directionDemanded = true;
            m.direction = mGroupRxTact[group] ? Direction::Exhaust : Direction::Supply;
            m.phase = 0;
            m.share = 100;
            grp.addMember(m);
            return;
        }

        // Mitglieder in jedem Durchlauf neu einsammeln: Stufe, Deckel und
        // Richtungsforderung eines Raums aendern sich staendig, und der Verbund
        // soll auf dem Stand rechnen, der jetzt gilt.
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            KwlFan* fan = mFan[i];
            if (fan == nullptr || !fan->isActive() || fan->groupNo() != group + 1)
                continue;

            KwlRoom* room = openknxKwlRoomModule.room(fan->roomNo());
            if (room == nullptr || !room->isActive())
                continue;

            const StageResult& rs = room->stage();

            GroupMember m;
            m.roomNo = fan->roomNo();
            m.stage = rs.stage;
            m.maxStage = kStageMax; // Deckel steckt schon in rs.stage
            m.cycleWish = room->cycleWish();
            m.directionDemanded = rs.directionForced;
            m.direction = rs.direction;
            m.phase = fan->phase();
            m.share = fan->share();
            grp.addMember(m);
        }

        // "Folgt Zuluftanforderung von Raum n": zieht anderswo ein Geraet Abluft
        // ab, muss hier nachstroemen. Der Wunsch kommt in-process, nicht ueber den
        // Bus - das KO am Raum gibt es zusaetzlich fuer fremde Geraete.
        const uint8_t followRoom = mGroupFollowSupply[group];
        if (followRoom > 0)
        {
            KwlRoom* source = openknxKwlRoomModule.room(followRoom);
            if (source != nullptr && source->isActive() && source->supplyRequested())
            {
                GroupMember m{};
                m.roomNo = followRoom;
                m.stage = 0; // nur die Richtung, keine Stufe
                m.maxStage = kStageMax;
                m.cycleWish = CycleRule::Wrg;
                m.directionDemanded = true;
                m.direction = Direction::Supply;
                m.phase = 0;
                m.share = 100;
                grp.addMember(m);
            }
        }
    }

    void KwlFanModule::sendGroup(uint8_t group, const GroupResult& r, uint32_t now)
    {
        const int8_t speaker = mGroupSpeaker[group];
        if (speaker < 0 || mFan[speaker] == nullptr)
            return;

        const bool tact = r.phase0Direction == Direction::Exhaust;
        const bool changed = r.stage != mGroupSentStage[group] ||
                             tact != mGroupSentTact[group];
        const bool heartbeat = mGroupHeartbeatMs[group] > 0 &&
                               (now - mGroupLastSent[group]) >= mGroupHeartbeatMs[group];
        if (!changed && !heartbeat)
            return;

        mGroupSentStage[group] = r.stage;
        mGroupSentTact[group] = tact;
        mGroupLastSent[group] = now;

        mFan[speaker]->sendGroupState(r.stage, tact);
    }

    void KwlFanModule::driveGroup(uint8_t group, uint32_t now)
    {
        KwlGroup& grp = mGroup[group];

        // Master-Ueberwachung: bleibt das Lebenszeichen aus, weiss der Slave nicht
        // mehr, in welcher Halbwelle der Verbund steht. Weiterlaufen hiesse raten -
        // also Stillstand und Fehlercode 2.
        if (mGroupRole[group] == GroupRole::Slave && mGroupTimeoutMs[group] > 0)
        {
            const bool timedOut =
                (now - mGroupLastAlive[group]) >= mGroupTimeoutMs[group];
            if (timedOut != mGroupTimedOut[group])
            {
                mGroupTimedOut[group] = timedOut;
                if (timedOut)
                    logErrorP("Verbund %u: Lebenszeichen des Masters bleibt aus",
                              (unsigned)(group + 1));
            }
        }

        collectMembers(group);

        if (grp.memberCount() == 0)
            return;

        const GroupResult r = grp.update(now);

        if (mGroupRole[group] == GroupRole::Master)
            sendGroup(group, r, now);

        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            KwlFan* fan = mFan[i];
            if (fan == nullptr || !fan->isActive() || fan->groupNo() != group + 1)
                continue;

            // Fehlende Freigabe, Suspendierung oder ein fehlender Master schlagen
            // alles - sie sind der Grund, warum es Rang 1 gibt.
            if (r.inDeadTime || fan->blocked() || mGroupTimedOut[group])
            {
                // Sicherheitsinvariante 5: waehrend des Wechsels steht alles auf
                // 5,00 V, beim ego beide Motoren gleichzeitig.
                fan->driveSafe(mOutput);
                continue;
            }

            DriveCommand cmd;
            cmd.stage = KwlGroup::stageForShare(r.stage, fan->share());
            cmd.direction = KwlGroup::directionFor(r, fan->phase());
            cmd.hrv = r.hrv;
            fan->drive(mOutput, cmd, mBelow5vIsSupply);
        }

        if (!mOutput.ready())
        {
            // Die Luefter, die vor dem Fehler noch geschrieben wurden, stehen auf
            // ihrer Stufe - und ohne weiteren Durchlauf blieben sie dort. Also
            // alles in den sicheren Zustand, so gut es der Bus noch hergibt.
            mError = mOutput.error();
            logErrorP("Ausgabe fehlgeschlagen - Fehlercode %u", (unsigned)mError);
            mOutput.safeAll();
        }

        const ErrorCode groupError =
            mGroupTimedOut[group] ? ErrorCode::MasterTimeout : mError;
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            KwlFan* fan = mFan[i];
            if (fan == nullptr || fan->groupNo() != group + 1)
                continue;

            // Der Raum meldet Einschraenkungen (Feuchtevergleich, fehlende
            // Messwerte, Schutz) - er hat aber kein eigenes Stoerungsobjekt.
            // Ausgegeben werden sie an seinen Lueftern.
            // Ein Slave folgt dem Bus, nicht seinem Raum - dessen Befunde
            // (fehlende Sensoren, Schutz) haben mit dem Antrieb nichts zu tun.
            ErrorCode roomError = ErrorCode::None;
            if (mGroupRole[group] != GroupRole::Slave)
            {
                KwlRoom* room = openknxKwlRoomModule.room(fan->roomNo());
                if (room != nullptr && room->isActive())
                    roomError = room->errorCode();
            }

            fan->sendFault(r.conflict && r.conflictRoom == fan->roomNo(), groupError,
                           roomError);
        }
    }

    int8_t KwlFanModule::groupOf(GroupObject& ko) const
    {
        const int channel = FAN_KoCalcChannel(ko.asap());
        if (channel < 0 || channel >= FAN_ChannelCount)
            return -1;
        for (uint8_t g = 0; g < kGroupsMax; g++)
            if (mGroupSpeaker[g] == static_cast<int8_t>(channel))
                return static_cast<int8_t>(g);
        return -1;
    }

    void KwlFanModule::processInputKo(GroupObject& ko)
    {
        const int channel = FAN_KoCalcChannel(ko.asap());
        if (channel < 0 || channel >= FAN_ChannelCount)
            return;

        // Gruppen-KOs gehoeren dem Verbund, nicht dem Luefter - auch wenn sie an
        // seinem Kanal haengen. Ein Slave hoert darauf, ein Master und ein
        // interner Verbund nicht: sonst wuerde er sich selbst folgen.
        const int8_t group = groupOf(ko);
        if (group >= 0 && mGroupRole[group] == GroupRole::Slave)
        {
            switch (koIndex(ko.asap()))
            {
                case FAN_KoGroupStage:
                    mGroupRxStage[group] = ko.value(DPT_Value_1_Ucount);
                    mGroupLastAlive[group] = millis();
                    return;
                case FAN_KoGroupTact:
                    mGroupRxTact[group] = ko.value(DPT_Start);
                    mGroupLastAlive[group] = millis();
                    return;
                case FAN_KoGroupAlive:
                    mGroupLastAlive[group] = millis();
                    return;
                default:
                    break;
            }
        }

        if (mFan[channel] != nullptr)
            mFan[channel]->processInputKo(ko);
    }

    // ------------------------------------------------------------ Flash

    uint16_t KwlFanModule::flashSize()
    {
        // Versionsbyte, dann je Kanal ein Flagbyte und drei Zaehler. Die Groesse
        // haengt an FAN_ChannelCount und nicht an der Platine: das Feld wird vom
        // Framework vor setup() abgefragt, da steht die Hardwareauswahl noch nicht.
        return 1 + FAN_ChannelCount * 13;
    }

    void KwlFanModule::writeFlash()
    {
        openknx.flash.writeByte(kFlashVersion);

        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            KwlFan::PersistentState s{};
            if (mFan[i] != nullptr)
                s = mFan[i]->persistentState();

            openknx.flash.writeByte(s.flags);
            openknx.flash.writeInt(s.runSeconds);
            openknx.flash.writeInt(s.filterSeconds);
            openknx.flash.writeInt(s.filterVolume);
        }
    }

    void KwlFanModule::readFlash(const uint8_t* data, const uint16_t size)
    {
        (void)data;
        if (size < flashSize())
            return; // noch nichts oder ein aelteres Layout gespeichert

        if (openknx.flash.readByte() != kFlashVersion)
        {
            logInfoP("Gespeicherte Daten haben eine andere Version, werden verworfen");
            return;
        }

        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            KwlFan::PersistentState s;
            s.flags = openknx.flash.readByte();
            s.runSeconds = openknx.flash.readInt();
            s.filterSeconds = openknx.flash.readInt();
            s.filterVolume = openknx.flash.readInt();

            if (mFan[i] != nullptr)
                mFan[i]->restore(s);
        }
    }

    void KwlFanModule::processBeforeRestart()
    {
        // Sicherheitsinvariante 8: eine ETS-Neuprogrammierung darf keinen
        // Vollgas-Moment erzeugen.
        mOutput.safeAll();
        logInfoP("Sicherer Zustand vor Neustart geschrieben");
    }

    // ------------------------------------------------------------ Konsole

    void KwlFanModule::printStatus()
    {
        logInfoP("Luefterkanaele:");
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
            if (mFan[i] != nullptr)
                mFan[i]->printStatusLine();

        logInfoP("Busformat %s, Platine %u mit %u Kanaelen, Fehlercode %u",
                 boardLeftAligned() ? "linksbuendig" : "rechtsbuendig",
                 (unsigned)boardId(), (unsigned)boardChannels(),
                 (unsigned)mError);
    }

    void KwlFanModule::printGroups()
    {
        logInfoP("Verbuende:");
        for (uint8_t g = 0; g < kGroupsMax; g++)
        {
            if (!mGroupActive[g])
                continue;
            const GroupResult r = mGroup[g].result();
            logInfoP("  %u: Stufe %u, Phase-0-Richtung %s, Zyklus %u s%s%s",
                     (unsigned)(g + 1), (unsigned)r.stage,
                     r.phase0Direction == Direction::Supply ? "Zuluft" : "Abluft",
                     (unsigned)r.cycleSeconds, r.inDeadTime ? ", Totzeit" : "",
                     r.conflict ? ", RICHTUNGSKONFLIKT" : "");
        }
    }

    void KwlFanModule::showHelp()
    {
        openknx.console.printHelpLine("kwl st", "Alle Luefterkanaele je eine Zeile");
        openknx.console.printHelpLine("kwl grp", "Verbuende mit Takt und Richtung");
        openknx.console.printHelpLine("kwl fNN", "Luefter NN ausfuehrlich, z.B. kwl f1");
        openknx.console.printHelpLine("kwl rNN", "Raum NN ausfuehrlich, z.B. kwl r1");
    }

    void KwlFanModule::printGroupsDiagnose()
    {
#ifdef BASE_KoDiagnose
        // "G8 S4 Z 3600 K" = 14. K = Richtungskonflikt, T = Totzeit.
        for (uint8_t g = 0; g < kGroupsMax; g++)
        {
            if (!mGroupActive[g])
                continue;
            const GroupResult r = mGroup[g].result();
            openknx.console.writeDiagnoseKo("G%u S%u %c %u%s", (unsigned)(g + 1),
                                            (unsigned)r.stage,
                                            r.phase0Direction == Direction::Supply ? 'Z' : 'A',
                                            (unsigned)r.cycleSeconds,
                                            r.conflict ? " K" : r.inDeadTime ? " T" : "");
            // Bei Massenausgabe verschluckt der Bus sonst jede zweite Zeile -
            // dieselbe Abhilfe wie im Logikmodul.
            openknx.console.writeDiagnoseKo("");
        }
#endif
    }

    bool KwlFanModule::processCommand(const std::string cmd, bool debugKo)
    {
        // debugKo: der Befehl kam ueber das Diagnose-KO, die Antwort geht dorthin
        // zurueck - in Telegrammen zu 14 Zeichen. Ohne USB-Kabel der einzige Weg
        // in ein Geraet im Schacht.

        if (cmd.rfind("kwl", 0) != 0)
            return false;

        if (cmd == "kwl st")
        {
            if (debugKo)
            {
                for (uint8_t i = 0; i < FAN_ChannelCount; i++)
                    if (mFan[i] != nullptr && mFan[i]->isActive())
                    {
                        mFan[i]->printDiagnose(false);
#ifdef BASE_KoDiagnose
                        openknx.console.writeDiagnoseKo("");
#endif
                    }
                return true;
            }
            printStatus();
            return true;
        }

        if (cmd == "kwl grp")
        {
            if (debugKo)
                printGroupsDiagnose();
            else
                printGroups();
            return true;
        }

        // "kwl f1" und "kwl r1". Die Nummern sind die der ETS, also ab 1 - beim
        // Suchen an der Platine zaehlt niemand ab 0.
        if (cmd.length() >= 6 && cmd.compare(0, 5, "kwl f") == 0)
        {
            const int no = atoi(cmd.c_str() + 5);
            if (no < 1 || no > FAN_ChannelCount || mFan[no - 1] == nullptr)
            {
                logInfoP("Luefter %d gibt es nicht (1…%u)", no,
                         (unsigned)FAN_ChannelCount);
                return true;
            }
            if (debugKo)
                mFan[no - 1]->printDiagnose(true);
            else
                mFan[no - 1]->printDetail();
            return true;
        }

        if (cmd.length() >= 6 && cmd.compare(0, 5, "kwl r") == 0)
        {
            const int no = atoi(cmd.c_str() + 5);
            KwlRoom* room = openknxKwlRoomModule.room(static_cast<uint8_t>(no));
            if (room == nullptr)
            {
                logInfoP("Raum %d gibt es nicht (1…%u)", no,
                         (unsigned)ROOM_ChannelCount);
                return true;
            }
            if (debugKo)
                room->printDiagnose(true);
            else
                room->printDetail();
            return true;
        }

        return false;
    }
} // namespace Kwl
