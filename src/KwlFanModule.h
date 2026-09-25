#pragma once

#include "KwlFan.h"
#include "KwlGroup.h"
#include "KwlOutput.h"
#include "OpenKNX.h"
#include "knxprod.h"

namespace Kwl
{
    /// Compile-Zeit-Obergrenze der Verbuende (PLAN Phase 2).
    constexpr uint8_t kGroupsMax = 8;

    // ========================================================================
    // OpenKNX-Modul der Luefter. Haelt Ausgabeschicht, Luefterkanaele und Verbuende.
    //
    // Kompiliert wird das Maximum, sichtbar ist die Auswahl: 12 Luefter und 8
    // Verbuende stehen immer im Speicher, laufen aber nur, wenn die ETS sie
    // aktiviert hat.
    // ========================================================================
    class KwlFanModule : public OpenKNX::Module
    {
      public:
        void setup(bool configured) override;
        void loop(bool configured) override;
        void processInputKo(GroupObject& ko) override;
        void processBeforeRestart() override;

        uint16_t flashSize() override;
        void writeFlash() override;
        void readFlash(const uint8_t* data, const uint16_t size) override;

        void showHelp() override;
        bool processCommand(const std::string cmd, bool debugKo) override;

        const std::string name() override { return "KwlControl"; }
        const std::string version() override { return "0.1"; }

        /// Ausgabeschicht, damit der Raum-Teil beim Start mitlesen kann.
        KwlOutput& output() { return mOutput; }

        ErrorCode error() const { return mError; }

      private:
        static constexpr uint8_t kFlashVersion = 1;

        bool checkHardware();
        void setupChannels();
        void setupGroups();
        void driveGroup(uint8_t group, uint32_t now);
        void collectMembers(uint8_t group);
        void sendGroup(uint8_t group, const GroupResult& r, uint32_t now);
        int8_t groupOf(GroupObject& ko) const;
        void printStatus();
        void printGroups();

        /// Rolle eines Verbunds: 0 intern, 1 Master (sendet), 2 Slave (folgt).
        enum class GroupRole : uint8_t
        {
            Internal = 0,
            Master = 1,
            Slave = 2
        };

        KwlOutput mOutput;
        KwlFan* mFan[FAN_ChannelCount] = {};
        KwlGroup mGroup[kGroupsMax];
        bool mGroupActive[kGroupsMax] = {};
        GroupRole mGroupRole[kGroupsMax] = {};

        /// Luefter, an dem die Gruppen-KOs dieses Verbunds haengen: der Kanal mit
        /// der kleinsten Nummer. Der Plan gibt den Verbuenden keinen eigenen
        /// KO-Block; sie liegen beim Luefter mit der Rolle.
        int8_t mGroupSpeaker[kGroupsMax] = {};

        uint8_t mGroupLeadRoom[kGroupsMax] = {};
        uint8_t mGroupFollowSupply[kGroupsMax] = {};
        uint32_t mGroupHeartbeatMs[kGroupsMax] = {};
        uint32_t mGroupTimeoutMs[kGroupsMax] = {};

        // --- Master sendet ----------------------------------------------------
        uint32_t mGroupLastSent[kGroupsMax] = {};
        uint8_t mGroupSentStage[kGroupsMax] = {};
        bool mGroupSentTact[kGroupsMax] = {};

        // --- Slave empfaengt --------------------------------------------------
        uint8_t mGroupRxStage[kGroupsMax] = {};
        bool mGroupRxTact[kGroupsMax] = {};
        uint32_t mGroupLastAlive[kGroupsMax] = {};
        bool mGroupTimedOut[kGroupsMax] = {};

        ErrorCode mError = ErrorCode::None;
        bool mBelow5vIsSupply = true;
    };
} // namespace Kwl

extern Kwl::KwlFanModule openknxKwlFanModule;
