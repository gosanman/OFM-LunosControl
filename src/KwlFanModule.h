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

    /// Takt-Plaetze: die 8 Verbuende der ETS und dahinter ein Platz je Luefter
    /// fuer "eigenstaendig (eigener Takt)". Ein eigenstaendiger Luefter ist ein
    /// Verbund mit genau einem Mitglied - derselbe Antriebsweg, dieselbe Totzeit,
    /// derselbe Takt, nur ohne ETS-Parameter (Vorgaben aus KwlGroup: 70 s WRG,
    /// 3600 s Sommer, 2 s Totzeit). Bis 2026-09-27 gab es diesen Weg nicht, und ein
    /// Luefter auf Verbund 0 blieb dauerhaft auf 5,00 V stehen.
    constexpr uint8_t kSlotsMax = kGroupsMax + FAN_ChannelCount;

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
        void savePower() override;
        bool restorePower() override;

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

        /// Info-LED-Funktionen (PT-SLEDFunc in Fan.share.xml): 110 zeigt Betrieb
        /// und Stoerungen, 111 nur Stoerungen. Die ETS ordnet die eine LED der
        /// Platine einer davon zu - oder einer Basisfunktion, dann tun wir nichts.
        static constexpr uint32_t kLedFunctionAll = 110;
        static constexpr uint32_t kLedFunctionFaultsOnly = 111;

        /// Wirkung einer Geste der Funktionstaste (PT-FanButtonAction).
        enum class ButtonAction : uint8_t
        {
            None = 0,
            Boost = 1,     ///< Stosslueften ein/aus, alle Raeume
            Quiet = 2,     ///< Ruhe ein/aus, alle Raeume
            FilterAck = 3  ///< Filterwechsel quittieren, faellige Filter
        };

      private:
        static constexpr uint8_t kFlashVersion = 1;

        bool checkHardware();
        void setupChannels();
        void setupGroups();
        /// Gehoert Luefter `fan` zum Takt-Platz `slot`? Platz < kGroupsMax ist
        /// Verbund slot+1, dahinter der eigenstaendige Platz genau eines Luefters.
        bool inSlot(uint8_t fan, uint8_t slot) const;
        void driveGroup(uint8_t group, uint32_t now);
        bool tryRecoverOutput(uint32_t now);
        void holdSafeUnconfigured(uint32_t now);
        void collectMembers(uint8_t group);
        void sendGroup(uint8_t group, const GroupResult& r, uint32_t now);
        int8_t groupOf(GroupObject& ko) const;
        void printStatus();
        void printGroups();
        void printGroupsDiagnose();

        // --- Geraete-LED und Funktionstaste -----------------------------------
        void setupButton();
        void handleButton(ButtonAction action, uint32_t now);
        void toggleForcedMode(OperatingMode mode, uint32_t now);
        void updateLed();
        void flashLed();

        OpenKNX::Led::FunctionGroup* mLedAll = nullptr;
        OpenKNX::Led::FunctionGroup* mLedFaults = nullptr;
        uint8_t mLedShown = 0xFF;

        /// Die Rueckrufe der Taste laufen im Timer-Interrupt. Sie setzen nur ein
        /// Flag; die Wirkung passiert in loop(), wo KNX und Raeume erlaubt sind.
        volatile uint8_t mButtonPending = 0; ///< 1 kurz, 2 lang, 3 doppelt
        ButtonAction mButtonShort = ButtonAction::None;
        ButtonAction mButtonLong = ButtonAction::None;
        ButtonAction mButtonDouble = ButtonAction::None;

        /// Rolle eines Verbunds: 0 intern, 1 Master (sendet), 2 Slave (folgt).
        enum class GroupRole : uint8_t
        {
            Internal = 0,
            Master = 1,
            Slave = 2
        };

        KwlOutput mOutput;
        KwlFan* mFan[FAN_ChannelCount] = {};
        // Alle Verbundfelder sind je Takt-Platz angelegt (kSlotsMax), nicht je
        // ETS-Verbund. Rolle, Lebenszeichen und Gruppen-KOs gibt es nur fuer die
        // ersten kGroupsMax; ein eigenstaendiger Platz ist immer "intern".
        KwlGroup mGroup[kSlotsMax];
        bool mGroupActive[kSlotsMax] = {};
        GroupRole mGroupRole[kSlotsMax] = {};

        /// Luefter, an dem die Gruppen-KOs dieses Verbunds haengen: der Kanal mit
        /// der kleinsten Nummer. Der Plan gibt den Verbuenden keinen eigenen
        /// KO-Block; sie liegen beim Luefter mit der Rolle.
        int8_t mGroupSpeaker[kSlotsMax] = {};

        uint8_t mGroupLeadRoom[kSlotsMax] = {};
        uint8_t mGroupFollowSupply[kSlotsMax] = {};
        uint32_t mGroupHeartbeatMs[kSlotsMax] = {};
        uint32_t mGroupTimeoutMs[kSlotsMax] = {};

        // --- Master sendet ----------------------------------------------------
        uint32_t mGroupLastSent[kSlotsMax] = {};
        uint8_t mGroupSentStage[kSlotsMax] = {};
        bool mGroupSentTact[kSlotsMax] = {};

        // --- Slave empfaengt --------------------------------------------------
        uint8_t mGroupRxStage[kSlotsMax] = {};
        bool mGroupRxTact[kSlotsMax] = {};
        uint32_t mGroupLastAlive[kSlotsMax] = {};
        bool mGroupTimedOut[kSlotsMax] = {};

        ErrorCode mError = ErrorCode::None;
        bool mBelow5vIsSupply = true;

        /// true, sobald loop() einmal mit gueltiger Parametrierung lief. Faellt
        /// knx.configured() danach auf false (ETS-Download laeuft), schreibt loop()
        /// EINMAL den sicheren Zustand - sonst stuenden die Ausgaenge fuer die Dauer
        /// des Downloads auf der letzten Stufe, ohne dass jemand sie fuehrt.
        bool mWasConfigured = false;

        /// true zwischen savePower() und restorePower(): der KNX-Bus ist weg, die
        /// 12 V der Luefter nicht. Die DACs halten jeden Wert, solange sie
        /// Strom haben - also muss vor dem Abschalten des RP2040 5,00 V stehen,
        /// und loop() darf ihn bis zur Rueckkehr nicht wieder ueberschreiben.
        bool mPowerSaved = false;

        /// Letzter Versuch, die DACs nach einem Ausfall wieder aufzunehmen
        /// (tryRecoverOutput). Nur fuer DacUnreachable - ein Konfigurationsfehler
        /// heilt nicht von selbst.
        uint32_t mRecoverLastMs = 0;
        static constexpr uint32_t kRecoverIntervalMs = 250;
    };
} // namespace Kwl

extern Kwl::KwlFanModule openknxKwlFanModule;
