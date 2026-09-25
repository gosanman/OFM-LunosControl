#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    // ========================================================================
    // Wann ein Wert auf den Bus gehoert.
    //
    // GroupObject::value() markiert das Objekt IMMER zum Senden - auch wenn sich
    // nichts geaendert hat. Wer es in loop() aufruft, flutet den Bus. Deshalb
    // entscheidet diese Schicht vorher, und zwar nach zwei Regeln:
    //
    //   * **geaendert** - diskrete Werte (Stufe, Schaltzustand) gehen hinaus,
    //     wenn sie sich unterscheiden, sonst nicht
    //   * **Totband** - analoge Werte (g/kg, Volt, Volumenstrom) brauchen einen
    //     Mindestabstand, sonst sendet das letzte Bit Messrauschen
    //
    // Dazu kommt das zyklische Senden: ein Wert, der sich nie aendert, soll
    // trotzdem hin und wieder bestaetigt werden.
    // ========================================================================
    class SendCondition
    {
      public:
        /// Totband, relativ ODER absolut.
        ///
        /// Uebernommen aus der Vorlage OFM-FanControl (`FanChannel::passesDeadband`,
        /// GPL-3.0) und auf 32-Bit-Absolutwerte erweitert. Der absolute Sockel
        /// wirkt dort, wo die relative Schwelle in der Naehe von 0 beliebig klein
        /// wuerde - ohne ihn sendet ein Wert um den Nullpunkt herum unablaessig.
        ///
        /// Sind beide Schwellen 0, gilt jede Aenderung als sendenswert.
        static bool passesDeadband(int32_t value, int32_t lastSent,
                                   uint8_t percentBand, uint32_t absBand);

        /// Ist die Zykluszeit abgelaufen? `cycleMs` 0 heisst: kein zyklisches
        /// Senden. Ueberlaufsicher gegen den millis()-Umlauf.
        static bool cycleElapsed(uint32_t now, uint32_t lastSent, uint32_t cycleMs);
    };

    /// Ein gesendeter Wert samt Merker, ob ueberhaupt schon gesendet wurde.
    /// Der Unterschied zaehlt: ein Wert, der zufaellig 0 ist, ist nicht dasselbe
    /// wie einer, den noch niemand bekommen hat.
    template <typename T>
    struct Sent
    {
        T value{};
        bool valid = false;
        uint32_t stamp = 0;

        /// Muss gesendet werden, weil sich der Wert geaendert hat oder die
        /// Zykluszeit abgelaufen ist?
        bool due(const T& candidate, uint32_t now, uint32_t cycleMs) const
        {
            if (!valid)
                return true;
            if (candidate != value)
                return true;
            return SendCondition::cycleElapsed(now, stamp, cycleMs);
        }

        void mark(const T& candidate, uint32_t now)
        {
            value = candidate;
            valid = true;
            stamp = now;
        }
    };
} // namespace Kwl
