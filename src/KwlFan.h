#pragma once

#include "KwlCurve.h"
#include "KwlOutput.h"
#include "KwlTypes.h"
#include "SendCondition.h"
#include "OpenKNX.h"

namespace Kwl
{
    /// Was der Verbund diesem Luefter aufträgt. Als Struktur, weil die Liste noch
    /// waechst und ein weiterer Parameter sonst jede Aufrufstelle anfasst.
    struct DriveCommand
    {
        uint8_t stage;
        Direction direction;
        bool hrv; ///< Verbund pendelt im kurzen Zyklus
    };

    // ========================================================================
    // Ein Luefter. In der ETS ein Kanal des Moduls FAN.
    //
    // Der Kanal treibt, er entscheidet nicht: Stufe und Richtung kommen vom Raum
    // (ueber StageArbiter) und vom Verbund (ueber KwlGroup). Hier wird daraus eine
    // Spannung - und nur hier steht, welcher DAC-Kanal dazu gehoert.
    //
    // Zwei Korrekturen sitzen zwischen Kennlinie und Ausgabe:
    //   * die **Leitungskompensation** gleicht den Spannungsabfall auf der
    //     gemeinsamen Masse aus - was der Luefter sieht, nicht was die Klemme fuehrt
    //   * der **Kalibrierfaktor** gleicht den Steigungsfehler des DAC-Kanals aus
    // In dieser Reihenfolge: erst was am Luefter ankommen soll, dann was der
    // Wandler dafuer ausgeben muss.
    // ========================================================================
    class KwlFan : public OpenKNX::Channel
    {
      public:
        explicit KwlFan(uint8_t index);

        const std::string name() override { return "KwlFan"; }

        /// Persistenter Teil des Kanalzustands. Das Modul schreibt ihn in den
        /// Flash - die Freigabe ist selbsthaltend und ueberlebt einen
        /// Spannungsausfall, und ein Filterzaehler, der bei jedem Neustart von
        /// vorn begaenne, waere wertlos.
        struct PersistentState
        {
            uint8_t flags;         ///< Bit0 Freigabe empfangen, Bit1 suspendiert
            uint32_t runSeconds;   ///< Betriebssekunden gesamt
            uint32_t filterSeconds; ///< Laufzeit seit dem letzten Filterwechsel
            uint32_t filterVolume; ///< m³ seit dem letzten Filterwechsel
        };
        static constexpr uint8_t kFlagEnabled = 0x01;
        static constexpr uint8_t kFlagSuspended = 0x02;

        void setup();
        void loop();
        void processInputKo(GroupObject& ko);

        PersistentState persistentState() const;
        void restore(const PersistentState& state);

        /// Darf dieser Luefter ueberhaupt foerdern? Fehlt die Freigabe oder ist er
        /// suspendiert, gibt es keine Stufe - das gewinnt gegen jede Bedienung.
        bool blocked() const;

        /// Anliegender Fehlercode, kleinster gewinnt. `groupConflict` kommt vom
        /// Modul, weil nur der Verbund ihn kennt.
        ErrorCode errorCode(bool groupConflict, ErrorCode moduleError) const;

        /// Fehlercode und Stoerungs-KO senden, wenn sie sich geaendert haben.
        void sendFault(bool groupConflict, ErrorCode moduleError);

        /// Verbundzustand auf den Bus geben. Nur der Luefter mit der kleinsten
        /// Nummer eines Verbunds tut das - er traegt dessen KOs.
        void sendGroupState(uint8_t stage, bool tact);

        bool isActive() const { return mActive; }
        FanType type() const { return mType; }
        uint8_t dacChannel() const { return mDacChannel; }
        uint8_t roomNo() const { return mRoomNo; }
        uint8_t groupNo() const { return mGroupNo; }
        uint8_t phase() const { return mPhase; }
        uint8_t share() const { return mShare; }
        uint8_t stage() const { return mStage; }
        Direction direction() const { return mDirection; }

        /// Belegt dieser Luefter zwei DAC-Kanaele? Nur der ego tut das.
        uint8_t channelsNeeded() const { return Curve::channelsNeeded(mType); }

        /// Stufe und Richtung ausgeben. `below5vIsSupply` kommt aus dem
        /// Board-Header (Messung M1) und wird nicht hier entschieden.
        bool drive(KwlOutput& output, const DriveCommand& cmd, bool below5vIsSupply);

        /// Sicherer Zustand dieses Luefters, unabhaengig von allem anderen.
        bool driveSafe(KwlOutput& output);

        /// Eine Zeile fuer "kwl st".
        void printStatusLine();

        /// Ausfuehrliche Einzelansicht fuer "kwl fNN".
        void printDetail();

      private:
        /// Sollspannung am Luefter -> Spannung, die der DAC ausgeben muss.
        float condition(float volt, uint8_t stage) const;

        void sendStatus();

        bool mActive = false;
        FanType mType = FanType::E2_60;
        uint8_t mDacChannel = 0;
        uint8_t mRoomNo = 1;
        uint8_t mGroupNo = 1;
        uint8_t mPhase = 0;
        uint8_t mShare = 100;

        // Leitungskompensation
        bool mCableComp = false;
        float mCableResistance = 0.0f;  ///< Ohm, ein Leiter
        float mCurrent[kStageMax + 1] = {}; ///< A je Stufe

        /// Kalibrierfaktor, 10000 = 1,0000. Aus Phase 1, Messung P7.
        float mCalib = 1.0f;

        /// Nennvolumenstrom je Stufe in m³/h, aus der ETS.
        uint8_t mFlow[kStageMax + 1] = {};

        // --- Freigabe, Suspendierung, Zaehler ---------------------------------
        bool mUseEnable = false;
        bool mSuspendAllowed = false;
        bool mEnableLatched = false;
        bool mSuspended = false;

        uint32_t mRunSeconds = 0;
        uint32_t mFilterSeconds = 0;
        uint32_t mFilterVolume = 0;
        uint32_t mLastTick = 0;

        uint8_t mFilterMode = 0;
        uint32_t mFilterLimitSeconds = 0;
        uint32_t mFilterLimitVolume = 0;
        uint32_t mFilterRemindMs = 0;
        bool mFilterDue = false;
        bool mFilterMuted = false;
        uint32_t mFilterMutedSince = 0;

        ErrorCode mLastError = ErrorCode::None;
        bool mFaultValid = false;

        /// Zykluszeiten aus der ETS, 0 = aus.
        uint32_t mSendCycleStatusMs = 0;
        uint32_t mSendCycleHoursMs = 0;

        Sent<uint16_t> mSentHours;
        Sent<uint8_t> mSentFilterLeft;
        Sent<bool> mSentFilterDue;

        uint8_t mStage = 0;
        Direction mDirection = Direction::Supply;
        bool mHrv = false;
        float mLastVolt = 0.0f;
        bool mStatusValid = false;
    };
} // namespace Kwl
