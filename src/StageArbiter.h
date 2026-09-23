#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    /// Woher die wirksame Stufe kommt. Die Zahl ist der Rang aus der
    /// Vorfahrtstabelle (PLAN Phase 4) und wird so auch im Status-KO gemeldet.
    enum class StageSource : uint8_t
    {
        Lock = 1,       ///< Sperre, fehlende Freigabe, Master-Timeout
        Protection = 2, ///< Frost oder Hitze
        AirDemand = 3,  ///< Zu-/Abluftanforderung
        Group = 4,      ///< Verbund, nur beim Slave
        Manual = 5,     ///< Handstufe
        Automatic = 6   ///< Fuehrungen und Betriebsart
    };

    /// Parametersatz einer Betriebsart, soweit er die Stufe bestimmt.
    /// Die uebrigen Felder des Satzes (Fuehrungen an/aus, Zyklusregel, Nachlauf)
    /// gehoeren in den Raum, nicht in die Vorfahrt.
    struct ModeParams
    {
        uint8_t baseStage; ///< Grundstufe: laeuft immer, auch ohne Anforderung
        uint8_t maxStage;  ///< Maximalstufe: Deckel fuer alle Fuehrungen
    };

    struct StageResult
    {
        uint8_t stage;
        StageSource source;
        bool directionForced; ///< true: `direction` gilt, der Takt ist ausgesetzt
        Direction direction;
        OperatingMode mode;   ///< welche Betriebsart Rang 6 gerade bestimmt
        uint8_t modeRank;     ///< 7…11: welche Ebene diese Betriebsart gesetzt hat
    };

    // ========================================================================
    // Die Vorfahrt. EINE Stelle, an der eine Stufe entsteht (Sicherheitsinvariante 9).
    //
    // | Rang | Ebene                        |
    // |------|------------------------------|
    // |  1   | Sperre                       | immer Stufe 0 -> 5,00 V
    // |  2   | Schutz (Frost/Hitze)         | Schutzstufe
    // |  3   | Zu-/Abluftanforderung        | erzwingt Richtung und Stufe
    // |  4   | Verbund (nur Slave)          | folgt Gruppenstufe und -richtung
    // |  5   | Handstufe                    | ersetzt die Automatikstufe
    // |  6   | Automatikstufe               | Fuehrungen, gedeckelt von der Betriebsart
    // |  7   | Zwangsobjekt 1 / 2 / 3       | bestimmen, WELCHER Parametersatz
    // |  8   | Nacht                        | in Rang 6 gilt
    // |  9   | Zwangsbetriebsart            |
    // | 10   | Betriebsart                  |
    // | 11   | Standard-Betriebsart         |
    //
    // Ruecksetz-Semantik (Arcus 3.14): aendert sich ein Objekt auf Rang r mit
    // 5 <= r <= 11, werden alle Objekte hoeheren Rangs bis einschliesslich 5
    // zurueckgesetzt - jede Nutzeraktion bekommt eine sichtbare Reaktion. Die Raenge
    // 1 bis 4 sind ausgenommen: eine Sperre laesst sich nicht wegdruecken.
    //
    // Zwei Auslegungen, die der Plan offen laesst und die hier festgelegt sind:
    //
    //  * Die **Maximalstufe deckelt die Handstufe nicht.** Der Plan nennt sie
    //    "Deckel fuer alle Fuehrungen", und die Handstufe steht als Rang 5 ueber der
    //    Automatikstufe. Wer nachts bewusst Stufe 3 drueckt, bekommt Stufe 3; der
    //    Deckel schuetzt vor der Automatik, nicht vor dem Bewohner.
    //  * Bei **gleichrangigen Zwangsobjekten gewinnt das zuletzt eingeschaltete.**
    //    Wird dieses zurueckgenommen, faellt der Knoten auf ein noch anstehendes
    //    Zwangsobjekt zurueck, nicht gleich auf die naechste Ebene.
    // ========================================================================
    class StageArbiter
    {
      public:
        StageArbiter();

        // --- Parametrierung -------------------------------------------------
        void setModeParams(OperatingMode mode, ModeParams params);
        ModeParams modeParams(OperatingMode mode) const;

        /// Standard-Betriebsart (Rang 11), Vorgabe Standby.
        void setStandardMode(OperatingMode mode);

        /// Stufe im Schutzfall (Rang 2), Vorgabe 0 - Frost heisst Lueftung aus.
        void setProtectionStage(uint8_t stage);

        /// Belegung und Laufzeit eines Zwangsobjekts, `index` 1…3.
        /// `runtimeMs` 0 = unendlich. Vorgaben: 1 Stosslueften 30 min,
        /// 2 Ruhe unendlich, 3 Komfort 30 min.
        void setForcedObject(uint8_t index, OperatingMode mode, uint32_t runtimeMs);

        /// false: gleichrangig, das zuletzt eingeschaltete gewinnt.
        /// true: hierarchisch, 3 schlaegt 2 schlaegt 1.
        void setForcedObjectHierarchical(bool hierarchical);

        /// Laufzeit der Zwangsbetriebsart, 0 = unendlich.
        void setForcedModeRuntime(uint32_t runtimeMs);

        /// Laufzeit der Handstufe, 0 = dauerhaft. Vorgabe 60 min.
        void setManualRuntime(uint32_t runtimeMs);

        // --- Raenge 1 bis 4 -------------------------------------------------
        void setLock(bool active);
        void setProtection(bool active);
        void setAirDemand(bool active, Direction direction, uint8_t stage);
        void setGroupStage(bool active, uint8_t stage, Direction direction);

        // --- Rang 5, Handstufe ----------------------------------------------
        void setManualStage(uint8_t stage, uint32_t now);

        /// Schritt hoch oder runter, ausgehend von der zuletzt wirksamen Stufe.
        void stepManual(int8_t delta, uint32_t now);

        /// KO "Handbetrieb aktiv" als Eingang. Eine 0 beendet den Handbetrieb
        /// sofort; eine 1 nimmt die zuletzt gesetzte Handstufe wieder auf.
        void setManualActive(bool active, uint32_t now);

        bool manualActive() const { return mManualActive; }

        // --- Rang 6 ---------------------------------------------------------
        /// Stufenwunsch der Fuehrungen (Treppen, Temperatur). Grundstufe und
        /// Maximalstufe der aktiven Betriebsart kommen hier obendrauf.
        void setGuidanceStage(uint8_t stage);

        // --- Raenge 7 bis 11 ------------------------------------------------
        void setForcedObjectState(uint8_t index, bool active, uint32_t now);
        void setNight(bool active, uint32_t now);
        void setForcedMode(OperatingMode mode, uint32_t now);
        void clearForcedMode(uint32_t now);

        /// KO Betriebsart (DPT 20.102). `OperatingMode::Auto` schaltet auf die
        /// Standard-Betriebsart zurueck.
        void setMode(OperatingMode mode, uint32_t now);

        // --- Auswertung -----------------------------------------------------
        /// Laufzeiten ablaufen lassen und die wirksame Stufe bilden.
        StageResult update(uint32_t now);

        /// Letztes Ergebnis ohne neue Bewertung.
        StageResult result() const { return mResult; }

        /// Alles auf Anfang: keine Sperre, kein Schutz, keine Ueberlagerung.
        void reset();

      private:
        /// Raenge unterhalb von `rank` (also hoeheren Vorrangs) bis 5 zuruecksetzen.
        void resetAbove(uint8_t rank);

        /// Ist die Laufzeit seit `since` abgelaufen? `runtime` 0 = nie.
        static bool expired(uint32_t now, uint32_t since, uint32_t runtime);

        void expireRuntimes(uint32_t now);
        void evaluateMode();
        void evaluateStage();

        static uint8_t clampStage(uint8_t stage);

        // Parametrierung
        ModeParams mParams[kModeCount + 1];
        OperatingMode mStandardMode = OperatingMode::Standby;
        uint8_t mProtectionStage = 0;
        OperatingMode mForcedObjectMode[4] = {OperatingMode::Auto, OperatingMode::Boost,
                                              OperatingMode::Quiet, OperatingMode::Comfort};
        uint32_t mForcedObjectRuntime[4] = {0, 30u * 60000u, 0, 30u * 60000u};
        bool mForcedObjectHierarchical = false;
        uint32_t mForcedModeRuntime = 0;
        uint32_t mManualRuntime = 60u * 60000u;

        // Raenge 1…4
        bool mLock = false;
        bool mProtection = false;
        bool mAirDemand = false;
        Direction mAirDemandDirection = Direction::Exhaust;
        uint8_t mAirDemandStage = 0;
        bool mGroup = false;
        uint8_t mGroupStage = 0;
        Direction mGroupDirection = Direction::Supply;

        // Rang 5
        bool mManualActive = false;
        uint8_t mManualStage = 0;
        uint32_t mManualSince = 0;

        // Rang 6
        uint8_t mGuidanceStage = 0;

        // Raenge 7…11
        bool mForcedObjectOn[4] = {false, false, false, false};
        uint32_t mForcedObjectSince[4] = {0, 0, 0, 0};
        uint8_t mForcedObjectLast = 0; ///< zuletzt eingeschaltet, fuer "gleichrangig"
        bool mNight = false;
        bool mForcedModeOn = false;
        OperatingMode mForcedMode = OperatingMode::Comfort;
        uint32_t mForcedModeSince = 0;
        OperatingMode mMode = OperatingMode::Auto;

        OperatingMode mActiveMode = OperatingMode::Standby;
        uint8_t mActiveModeRank = 11;
        StageResult mResult;
    };
} // namespace Kwl
