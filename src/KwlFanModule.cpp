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

        if (!checkHardware())
        {
            mOutput.begin();
            mOutput.safeAll();
            return;
        }

        setupChannels();
        setupGroups();

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
        if (!configured || mError != ErrorCode::None || !mOutput.ready())
            return;

        const uint32_t now = millis();

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
            mError = mOutput.error();
            logErrorP("Ausgabe fehlgeschlagen - Fehlercode %u", (unsigned)mError);
        }

        const ErrorCode groupError =
            mGroupTimedOut[group] ? ErrorCode::MasterTimeout : mError;
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
            if (mFan[i] != nullptr && mFan[i]->groupNo() == group + 1)
                mFan[i]->sendFault(r.conflict && r.conflictRoom == mFan[i]->roomNo(),
                                   groupError);
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
        openknx.console.printHelpLine("kwl", "Lueftersteuerung anzeigen");
    }

    bool KwlFanModule::processCommand(const std::string cmd, bool debugKo)
    {
        (void)debugKo;

        if (cmd.rfind("kwl", 0) != 0)
            return false;

        if (cmd == "kwl")
        {
            openknx.console.printHelpLine("kwl st", "Alle Luefterkanaele je eine Zeile");
            openknx.console.printHelpLine("kwl grp", "Verbuende mit Takt und Richtung");
            openknx.console.printHelpLine("kwl fNN", "Luefter NN ausfuehrlich, z.B. kwl f1");
            openknx.console.printHelpLine("kwl rNN", "Raum NN ausfuehrlich, z.B. kwl r1");
            return true;
        }

        if (cmd == "kwl st")
        {
            printStatus();
            return true;
        }

        if (cmd == "kwl grp")
        {
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
            room->printDetail();
            return true;
        }

        return false;
    }
} // namespace Kwl
