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
            uint8_t stageRule;
            uint8_t cycleConflict;
            bool active;
            uint8_t leadRoom;
            uint8_t deadTime;
            uint16_t cycle[kStageMax + 1]; // Index 1…4
            uint16_t summer;
        };

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

        for (uint8_t g = 0; g < kGroupsMax; g++)
        {
            if (!mGroupActive[g])
                continue;
            driveGroup(g, now);
        }
    }

    void KwlFanModule::driveGroup(uint8_t group, uint32_t now)
    {
        KwlGroup& grp = mGroup[group];

        // Mitglieder in jedem Durchlauf neu einsammeln: Stufe, Deckel und
        // Richtungsforderung eines Raums aendern sich staendig, und der Verbund
        // soll auf dem Stand rechnen, der jetzt gilt.
        grp.clearMembers();
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

        if (grp.memberCount() == 0)
            return;

        const GroupResult r = grp.update(now);

        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            KwlFan* fan = mFan[i];
            if (fan == nullptr || !fan->isActive() || fan->groupNo() != group + 1)
                continue;

            if (r.inDeadTime)
            {
                // Sicherheitsinvariante 5: waehrend des Wechsels steht alles auf
                // 5,00 V, beim ego beide Motoren gleichzeitig.
                fan->driveSafe(mOutput);
                continue;
            }

            const uint8_t stage = KwlGroup::stageForShare(r.stage, fan->share());
            const Direction dir = KwlGroup::directionFor(r, fan->phase());
            fan->drive(mOutput, stage, dir, mBelow5vIsSupply);
        }

        if (!mOutput.ready())
        {
            mError = mOutput.error();
            logErrorP("Ausgabe fehlgeschlagen - Fehlercode %u", (unsigned)mError);
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

        return false;
    }
} // namespace Kwl
