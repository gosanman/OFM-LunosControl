#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    /// Zwei Stellspannungen. Bei einkanaligen Luftern traegt nur `a` einen Wert;
    /// beim ego ist `a` Motor 1 und `b` Motor 2.
    struct VoltPair
    {
        float a;
        float b;
    };

    // ========================================================================
    // Kennlinien und Umrechnung Volt -> DAC.
    //
    // Reine Logik ohne Hardwarebezug: die Polaritaet der Spannungshaelften und
    // das Busformat werden als Argument hereingereicht, nicht aus dem
    // Board-Header gelesen. Nur so ist die Schicht ohne Geraet pruefbar - und
    // genau sie entscheidet ueber Stillstand oder Vollgas.
    // ========================================================================
    class Curve
    {
      public:
        /// Volt -> logischer 15-Bit-Code, `round(v * 3276.7)`, geklemmt auf
        /// 0…0x7FFF. Negative Werte und NaN ergeben 0, Werte ueber 10,00 V den
        /// Hoechstwert. Ein Ueberlauf ist ein Fehler, kein Grenzfall.
        static uint16_t voltToCode(float volt);

        /// Logischer Code -> Wert auf dem Bus. `leftAligned` kommt aus
        /// FANDRV_DAC_LEFT_ALIGNED und damit aus Messung P6. Die falsche Wahl
        /// verdoppelt jede Ausgangsspannung.
        static uint16_t codeToWire(uint16_t code, bool leftAligned);

        /// Sollspannung(en) fuer Typ, Stufe und Richtung.
        /// `below5vIsSupply` kommt aus FANDRV_BELOW_5V_IS_SUPPLY (Messung M1).
        /// Stufen ueber kStageMax werden auf kStageMax geklemmt.
        static VoltPair stageVolt(FanType type, uint8_t stage, Direction dir,
                                  bool below5vIsSupply);

        /// Nennvolumenstrom in m³/h fuer Typ und Stufe.
        static uint8_t nominalFlow(FanType type, uint8_t stage);

        /// Arbeitet dieser Typ bipolar um 5 V?
        static bool isBipolar(FanType type);

        /// Zahl der DAC-Kanaele, die dieser Typ belegt. Nur der ego braucht zwei.
        static uint8_t channelsNeeded(FanType type);

        /// Sicherer Zustand: Stillstand. Bipolar 5,00 V, unipolar 0,00 V.
        static float safeVolt(FanType type);

      private:
        static uint8_t clampStage(uint8_t stage);
    };
} // namespace Kwl
