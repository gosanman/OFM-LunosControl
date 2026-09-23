#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    /// Hoechste Zahl Mitglieder eines Verbunds. Ein Verbund ist normalerweise ein
    /// Paar; mehr gibt es fuer unpaarige Aufbauten (Arcus: drei e²60, einer voll,
    /// zwei mit halbem Anteil).
    constexpr uint8_t kGroupMembersMax = 6;

    /// Wie die Gruppenstufe aus den Raumwuenschen entsteht (ETS Grp%n%StageRule).
    enum class GroupStageRule : uint8_t
    {
        MaxCapped = 0, ///< Maximum, begrenzt durch den kleinsten Raumdeckel
        Min = 1,       ///< Minimum
        LeadRoom = 2   ///< ein Raum fuehrt
    };

    /// Was bei unterschiedlichem Zykluswunsch gilt (ETS Grp%n%CycleRule).
    enum class GroupCycleConflict : uint8_t
    {
        ShortestWins = 0, ///< Waermerueckgewinnung gewinnt (kuerzester Zyklus)
        LeadRoomWins = 1  ///< der fuehrende Raum entscheidet
    };

    /// Ein Luefter im Verbund, so wie ihn der Verbund sieht.
    struct GroupMember
    {
        uint8_t roomNo;        ///< 1…8, entscheidet bei Gleichstand
        uint8_t stage;         ///< Stufenwunsch aus effectiveStage()
        uint8_t maxStage;      ///< Deckel der dort aktiven Betriebsart
        CycleRule cycleWish;   ///< Zykluswunsch der Betriebsart
        bool directionDemanded; ///< Rang 3 oder feste Betriebsweise
        Direction direction;   ///< die geforderte Richtung
        uint8_t phase;         ///< 0 oder 1 - die beiden Haelften des Pendels
        uint8_t share;         ///< Anteil 25…100 %
    };

    struct GroupResult
    {
        uint8_t stage;             ///< Stufe des Verbunds vor dem Anteil
        bool inDeadTime;           ///< alle Kanaele auf 5,00 V, Wechsel laeuft
        Direction phase0Direction; ///< Phase 1 ist immer die Gegenrichtung
        bool directionFixed;       ///< kein Wechsel: eine Anforderung haelt sie
        bool conflict;             ///< zwei Forderungen schliessen sich aus
        uint8_t conflictRoom;      ///< der unterlegene Raum, 0 = keiner
        uint16_t cycleSeconds;     ///< aktuell wirksame Zykluszeit
    };

    // ========================================================================
    // Der Verbund: zwei (oder mehr) Luefter, die gegenlaeufig arbeiten und im Takt
    // die Richtung tauschen.
    //
    // Die Pendelbewegung IST die Waermerueckgewinnung - beim e² gibt es dafuer
    // keinen Schalter. Kurzer Zyklus (Vorgabe 70 s) heisst WRG; langer Zyklus
    // (Vorgabe 1 h, Sommer) heisst praktisch keine WRG, aber weiter wechselnde
    // Richtung, damit kein dauerhafter Ueber- oder Unterdruck entsteht.
    //
    // Zwei Luefter eines Verbunds koennen NIE in dieselbe Richtung laufen: was in
    // den einen Raum hineingedrueckt wird, muss aus dem anderen heraus. Deshalb
    // gibt es nur EINE Richtung je Verbund - die der Phase 0. Phase 1 ist immer die
    // Gegenrichtung.
    //
    // RICHTUNGSKONFLIKT: fordern zwei Raeume Richtungen, die sich ausschliessen,
    // gewinnt die hoehere Stufe; bei Gleichstand die niedrigere Raumnummer. Der
    // unterlegene Raum steht in `conflictRoom` und meldet
    // ErrorCode::DirectionConflict ohne Alarm. Der Konflikt wird gemeldet, nicht
    // versteckt - aufloesen muss ihn der Mensch, etwa durch eine andere
    // Verbundzuordnung.
    //
    // Jeder Richtungswechsel laeuft ueber die Totzeit bei 5,00 V
    // (Sicherheitsinvariante 5), auch der Wechsel in eine erzwungene Richtung.
    // ========================================================================
    class KwlGroup
    {
      public:
        KwlGroup();

        // --- Parametrierung -------------------------------------------------
        void setStageRule(GroupStageRule rule);
        void setCycleConflict(GroupCycleConflict rule);

        /// Fuehrender Raum (1…8) fuer die Regeln "ein Raum fuehrt" und
        /// "der fuehrende Raum entscheidet".
        void setLeadRoom(uint8_t roomNo);

        /// Totzeit vor jedem Richtungswechsel, Vorgabe 2 s.
        void setDeadTime(uint16_t seconds);

        /// Zykluszeit je Stufe (1…4), Vorgabe 70 s.
        void setCycleTime(uint8_t stage, uint16_t seconds);

        /// Zykluszeit im Sommerbetrieb, Vorgabe 3600 s.
        void setSummerCycleTime(uint16_t seconds);

        // --- Mitglieder -----------------------------------------------------
        void clearMembers();
        bool addMember(const GroupMember& member);
        uint8_t memberCount() const { return mCount; }

        // --- Auswertung -----------------------------------------------------
        GroupResult update(uint32_t now);
        GroupResult result() const { return mResult; }

        /// Richtung eines Mitglieds aus der Verbundrichtung.
        static Direction directionFor(const GroupResult& r, uint8_t phase);

        /// Stufe eines Mitglieds nach seinem Anteil. Ein Anteil unter 100 % darf
        /// einen laufenden Luefter nicht anhalten - deshalb mindestens Stufe 1,
        /// solange der Verbund laeuft.
        static uint8_t stageForShare(uint8_t groupStage, uint8_t share);

        /// Alles auf Anfang.
        void reset();

      private:
        uint8_t computeStage() const;
        uint16_t computeCycleSeconds(uint8_t stage) const;
        void resolveDemands(uint8_t stage);
        static Direction opposite(Direction d);

        /// Welche Phase-0-Richtung die Forderung dieses Mitglieds bedeutet.
        static Direction implied(const GroupMember& m);

        GroupStageRule mStageRule = GroupStageRule::MaxCapped;
        GroupCycleConflict mCycleConflict = GroupCycleConflict::ShortestWins;
        uint8_t mLeadRoom = 1;
        uint16_t mDeadTime = 2;
        uint16_t mCycleTime[kStageMax + 1] = {70, 70, 70, 70, 70};
        uint16_t mSummerCycle = 3600;

        GroupMember mMember[kGroupMembersMax];
        uint8_t mCount = 0;

        bool mFixed = false;
        Direction mDemandDirection = Direction::Supply;
        bool mConflict = false;
        uint8_t mConflictRoom = 0;

        Direction mCurrent = Direction::Supply;
        Direction mPending = Direction::Supply;
        bool mInDeadTime = false;
        bool mStarted = false;
        uint32_t mSince = 0;

        GroupResult mResult;
    };
} // namespace Kwl
