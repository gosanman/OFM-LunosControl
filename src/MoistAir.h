#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    // ========================================================================
    // Feuchte Luft: Saettigungsdampfdruck, Partialdruck, Mischungsverhaeltnis.
    //
    // Grundlage des Feuchtevergleichs (PLAN Phase 4b). Lueften trocknet nur, wenn
    // die Aussenluft weniger Wasser enthaelt - verglichen wird der
    // Wasserdampf-PARTIALDRUCK, nicht die relative Feuchte und nicht g/m³:
    //
    //   - rF allein taeuscht: 80 % bei 0 °C sind trockener als 50 % bei 20 °C.
    //   - g/m³ haengt am Volumen und damit an der Temperatur.
    //   - g/kg (Arcus) braucht den Luftdruck und ist deshalb nur die Anzeige.
    //
    // Der Partialdruck ist luftdruckunabhaengig und vergleicht innen/aussen auch
    // bei verschiedenen Temperaturen richtig. Nur fuer die Anzeige in g/kg (KO,
    // DPT 9.029) wird der Luftdruck aus der Hoehe ueber Meer geschaetzt.
    //
    // Formeln aus PLAN Anhang; Magnus ueber Wasser, gueltig -45…+60 °C.
    // ========================================================================
    class MoistAir
    {
      public:
        /// Saettigungsdampfdruck in hPa (Magnus ueber Wasser).
        /// `e_s = 6.112 * exp(17.62*T / (243.12+T))`
        /// Ausserhalb -45…+60 °C wird die Temperatur geklemmt: die Naeherung
        /// driftet dort weg, und ein Sensorfehler soll keine Fantasiewerte liefern.
        static float saturationPressure(float tempC);

        /// Wasserdampf-Partialdruck in hPa. `e = rF/100 * e_s(T)`
        /// Die relative Feuchte wird auf 0…100 % geklemmt.
        static float vapourPressure(float tempC, float relHumidity);

        /// Luftdruck in hPa aus der Hoehe ueber Meer [m], barometrische Hoehenformel
        /// der Normatmosphaere: `p = 1013.25 * (1 - 2.25577e-5*h)^5.2559`.
        /// Nur fuer die Anzeige. Hoehe wird auf 0…2000 m geklemmt (ETS-Bereich).
        static float pressureAtAltitude(float altitudeMeter);

        /// Mischungsverhaeltnis in g/kg: `r = 622 * e / (p - e)`.
        /// Reine Anzeigegroesse - der Vergleich innen/aussen laeuft ueber `e`.
        static float mixingRatio(float vapourPressureHpa, float airPressureHpa);

        /// Bequemlichkeit: g/kg direkt aus Temperatur, rF und Hoehe.
        static float mixingRatioAt(float tempC, float relHumidity, float altitudeMeter);
    };

    // ========================================================================
    // Feuchtevergleich mit Schalthysterese.
    //
    // `e_innen - e_aussen >= dEin`  -> Entfeuchtung wirkt, rF-Treppe erlaubt.
    // `e_innen - e_aussen <= dAus`  -> gesperrt, Grundstufe bleibt, Fehlercode 7
    //                                  (ohne Alarm), Status-KO.
    // Dazwischen bleibt der letzte Zustand stehen. Ohne diese Hysterese wuerde die
    // Anlage an einem Tag mit fast gleicher Feuchte innen und aussen im Takt der
    // Messwerte ein- und ausschalten.
    //
    // Der Anfangszustand ist GESPERRT: solange nicht gemessen ist, dass draussen
    // trockenere Luft steht, wird nicht entfeuchtet.
    // ========================================================================
    class HumidityCompare
    {
      public:
        /// Schaltschwellen in hPa. Vorgabe 1,5 / 0,5 hPa (PLAN Phase 4b);
        /// 1,5 hPa entsprechen etwa 1 g/kg.
        void setThresholds(float onHpa, float offHpa);

        /// Partialdruecke innen und aussen bewerten.
        /// @return true, wenn Entfeuchtung wirkt und die rF-Treppe laufen darf.
        bool update(float vapourInside, float vapourOutside);

        /// Letztes Ergebnis ohne neue Bewertung.
        bool allowed() const { return mAllowed; }

        /// Zurueck in den sicheren Anfangszustand (gesperrt).
        void reset() { mAllowed = false; }

      private:
        float mOnHpa = 1.5f;
        float mOffHpa = 0.5f;
        bool mAllowed = false;
    };
} // namespace Kwl
