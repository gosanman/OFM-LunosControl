#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    // ========================================================================
    // Grenzwert-Treppe fuer die Fuehrungsgroessen rF, CO2 und VOC.
    //
    // Uebernommen aus Arcus 3.13, von drei auf vier Stufen erweitert: FUENF
    // Grenzwerte fuer VIER Stufen. Grenzwert n schaltet Stufe n ein (n = 1…4),
    // Grenzwert n-1 schaltet sie wieder aus. Die Hysterese ist damit der Abstand
    // der Grenzwerte und braucht keinen eigenen Parameter.
    //
    //   Stufe 4 ─────────────────────────────────────────┐ ein bei GW4
    //   Stufe 3 ──────────────────────────────┐          │ aus bei GW3
    //   Stufe 2 ───────────────────┐          │ ein GW3  │
    //   Stufe 1 ────────┐          │ ein GW2  │ aus GW2  │
    //   Stufe 0         │ ein GW1  │ aus GW1  │          │
    //                   │ aus GW0  │          │          │
    //              GW0  GW1       GW2        GW3        GW4
    //
    // Die Treppe ist einheitenfrei: dieselbe Klasse traegt Prozent, ppm, ppb und
    // den einheitenlosen VOC-Index. Sie ersetzt P-Regler und Zweipunktregler aus
    // frueheren Fassungen - fuer einen Luefter mit vier Stufen ist ein
    // Prozent-Regler mit anschliessender Stufenabbildung ein Umweg.
    //
    // Die Klasse haelt Zustand: die aktuelle Stufe entscheidet mit darueber, was
    // der naechste Messwert bewirkt. Ein Messwert allein bestimmt die Stufe NICHT.
    // ========================================================================
    class StageLadder
    {
      public:
        /// Grenzwerte GW0…GW4 setzen. Die Stufe bleibt dabei unveraendert; erst
        /// der naechste update() bewertet den Messwert an den neuen Grenzwerten.
        ///
        /// Nicht aufsteigende Eingaben werden aufsteigend zurechtgerueckt
        /// (`gw[i] = max(gw[i], gw[i-1])`). Die ETS kann eine unsinnige Reihe
        /// zulassen; eine Treppe mit fallenden Stufen wuerde schwingen.
        void setThresholds(const float thresholds[kStageMax + 1]);

        /// Messwert bewerten und die neue Stufe liefern.
        ///
        /// Steigt der Wert ueber mehrere Grenzwerte auf einmal, springt die Treppe
        /// mehrere Stufen - ein Messwert von 90 % rF fuehrt sofort auf Stufe 4 und
        /// nicht erst nach vier Aufrufen.
        uint8_t update(float value);

        /// Aktuelle Stufe ohne neue Bewertung.
        uint8_t stage() const { return mStage; }

        /// Zurueck auf Stufe 0. Fuer den Fall, dass die Fuehrung abgeschaltet oder
        /// gesperrt wird (Feuchtevergleich, Sperre) - dann darf die Treppe beim
        /// Wiedereinschalten nicht auf ihrer alten Stufe weitermachen.
        void reset() { mStage = 0; }

        /// Grenzwert n, wie er nach dem Zurechtruecken tatsaechlich gilt.
        float threshold(uint8_t index) const;

      private:
        // Vorgabe rF innen [%] aus PLAN Phase 4b. Wird bei jedem Kanal durch die
        // ETS-Werte ersetzt; hier steht sie, damit eine nicht parametrierte Treppe
        // etwas Sinnvolles tut statt bei 0 zu schalten.
        float mThreshold[kStageMax + 1] = {45.0f, 50.0f, 55.0f, 60.0f, 65.0f};
        uint8_t mStage = 0;
    };
} // namespace Kwl
