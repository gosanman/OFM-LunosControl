#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    // ========================================================================
    // Prozent -> Stufe mit Hysterese.
    //
    // Prozent ist Eingabeformat, Stufe ist Wahrheit: ein Prozentwert vom Bus wird
    // AM RAND in eine Stufe uebersetzt und nie weiter nach innen gereicht. Intern
    // rechnet alles in Stufen 0…4 und Volt.
    //
    // Abbildung (PLAN Anhang):
    //   0 -> 0 · 1…25 -> 1 · 26…50 -> 2 · 51…75 -> 3 · 76…100 -> 4
    //
    // Hysterese +/- 3 %: ein Dimmer, der mit 25, 26, 25, 26 sendet, darf den
    // Luefter nicht im Sekundentakt zwischen zwei Stufen springen lassen. Eine
    // Stufe wird erst 3 % oberhalb ihrer Grenze betreten und erst 3 % unterhalb
    // wieder verlassen.
    //
    // Ausnahme ist die Null: 0 % ist kein Randwert, sondern eine Ansage. Sie fuehrt
    // ohne Hysterese auf Stufe 0, und jeder Wert ab 1 % fuehrt mindestens auf
    // Stufe 1 - sonst waere ein eingeschalteter Luefter bei 2 % stillgestanden.
    // ========================================================================
    class StageMap
    {
      public:
        /// Prozentwert bewerten und die neue Stufe liefern.
        /// Werte ueber 100 werden auf 100 geklemmt.
        uint8_t update(uint8_t percent);

        /// Aktuelle Stufe ohne neue Bewertung.
        uint8_t stage() const { return mStage; }

        /// Stufe setzen, ohne einen Prozentwert zu bewerten - damit die Hysterese
        /// von der tatsaechlich wirksamen Stufe aus rechnet.
        void setStage(uint8_t stage);

        /// Stufe -> Prozent fuer die Rueckmeldung: die Mitte des jeweiligen
        /// Bereichs, damit ein Rueckgelesener Wert dieselbe Stufe wieder ergibt.
        static uint8_t stageToPercent(uint8_t stage);

      private:
        uint8_t mStage = 0;
    };
} // namespace Kwl
