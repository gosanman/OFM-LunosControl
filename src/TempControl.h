#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    /// Was die Temperaturfuehrung gerade vorhat.
    enum class TempMode : uint8_t
    {
        None = 0,          ///< kein Eingriff, die Betriebsart entscheidet
        FreeCooling = 1,   ///< innen zu warm, draussen kuehler: freie Kuehlung
        HeatRetention = 2, ///< innen zu kalt, draussen kaelter: Waerme halten
        UseWarmAir = 3     ///< innen zu kalt, draussen waermer: Warmluft nutzen
    };

    /// Ergebnis eines Durchlaufs.
    struct TempResult
    {
        /// Ti unter dem Frostschutzwert. Rang 2 (Schutz) in effectiveStage();
        /// WELCHE Stufe daraus wird, entscheidet dort - hier steht nur die Lage.
        bool frostProtection;

        /// Ti ueber dem Hitzeschutzwert UND draussen noch waermer. Ebenfalls Rang 2.
        bool heatProtection;

        TempMode mode;
        uint8_t stageRequest; ///< 0 = keine Anforderung
        CycleRule cycle;
    };

    // ========================================================================
    // Temperaturfuehrung nach PLAN Phase 4b (Arcus 3.10).
    //
    // | Fall            | Bedingung                              | Stufe     | Takt      |
    // |-----------------|----------------------------------------|-----------|-----------|
    // | Freie Kuehlung  | Ti > Tsoll + 1 K  und  Ta < Ti - Abst.  | "Kuehlung"| Sommer    |
    // | Waermeerhalt    | Ti < Tsoll        und  Ta < Ti - Abst.  | keine     | WRG       |
    // | Warmluft nutzen | Ti < Tsoll - 1 K  und  Ta > Ti + Abst.  | "Heizung" | Sommer    |
    // | sonst           |                                        | keine     | Betriebsart|
    //
    // Der **Temperaturabstand** (0…10 K, Vorgabe 3) ist kein Feinschliff: erst wenn
    // |Ta - Ti| groesser ist, greift die Fuehrung ueberhaupt ein. Ein groesserer
    // Abstand haelt sie der Heizungsregelung aus dem Weg.
    //
    // AUSLEGUNG - das eine Kelvin: die Tabelle nennt fuer die freie Kuehlung
    // Tsoll + 1 K, fuer den Waermeerhalt aber nur Tsoll. Zwischen beiden liegt also
    // ein Kelvin, in dem keine Zeile zutrifft. Waere das "sonst", wuerde der
    // Verbund beim Durchfahren dieses Kelvins zwischen Sommer- und WRG-Takt hin und
    // her springen - genau das, was das Kelvin verhindern soll. Deshalb HAELT die
    // Fuehrung in diesem Band ihren letzten Zustand, solange die Aussenbedingung
    // (Ta gegen Ti) weiter gilt. Der Plan sagt "alle vier Faelle mit Hysterese";
    // dies ist die Hysterese. Ebenso fuer die Warmluftnutzung: ein bei Tsoll - 1 K,
    // aus erst bei Tsoll.
    // ========================================================================
    class TempControl
    {
      public:
        /// Frostschutz [°C], ETS 5…16, Vorgabe 8. Rueckkehr erst bei +2 K.
        void setFrostLimit(float celsius);

        /// Hitzeschutz [°C], Vorgabe 30. Wirkt nur, wenn draussen noch waermer ist.
        void setHeatLimit(float celsius);

        /// Temperaturabstand [K], ETS 0…10, Vorgabe 3.
        void setDistance(float kelvin);

        /// Stufen fuer "Kuehlung" (Vorgabe 3) und "Heizung" (Vorgabe 2).
        void setStages(uint8_t cooling, uint8_t heating);

        /// Innen-, Aussentemperatur und Solltemperatur bewerten.
        /// Ein fehlender Messwert (NaN) laesst alles stehen, wie es war - weder
        /// wird Schutz ausgeloest noch ein laufender Schutz aufgehoben.
        TempResult update(float tempInside, float tempOutside, float tempSetpoint);

        /// Letztes Ergebnis ohne neue Bewertung.
        TempResult result() const;

        /// Zurueck in den Anfangszustand: kein Eingriff, kein Schutz.
        void reset();

      private:
        float mFrostLimit = 8.0f;
        float mHeatLimit = 30.0f;
        float mDistance = 3.0f;
        uint8_t mStageCooling = 3;
        uint8_t mStageHeating = 2;

        bool mFrost = false;
        bool mHeat = false;
        TempMode mMode = TempMode::None;
    };
} // namespace Kwl
