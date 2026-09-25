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
        void processBeforeRestart() override;

        void showHelp() override;
        bool processCommand(const std::string cmd, bool debugKo) override;

        const std::string name() override { return "KwlControl"; }
        const std::string version() override { return "0.1"; }

        /// Ausgabeschicht, damit der Raum-Teil beim Start mitlesen kann.
        KwlOutput& output() { return mOutput; }

        ErrorCode error() const { return mError; }

      private:
        bool checkHardware();
        void setupChannels();
        void setupGroups();
        void driveGroup(uint8_t group, uint32_t now);
        void printStatus();
        void printGroups();

        KwlOutput mOutput;
        KwlFan* mFan[FAN_ChannelCount] = {};
        KwlGroup mGroup[kGroupsMax];
        bool mGroupActive[kGroupsMax] = {};

        ErrorCode mError = ErrorCode::None;
        bool mBelow5vIsSupply = true;
    };
} // namespace Kwl

extern Kwl::KwlFanModule openknxKwlFanModule;
