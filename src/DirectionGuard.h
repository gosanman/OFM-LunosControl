#pragma once

#include <stdint.h>

namespace Kwl
{
    // ========================================================================
    // Totzeit je Kanal (Sicherheitsinvariante 5), unabhaengig vom Verbund.
    //
    // Der Verbund (KwlGroup) legt die Totzeit ein, wenn sich seine RICHTUNG
    // aendert. Der Simulator hat drei Wege gefunden, auf denen die Spannung am
    // Kanal trotzdem ohne Aufenthalt bei 5,00 V die Seite wechselt (Befund C,
    // 2026-10-06):
    //   - nach 12-V-Wiederkehr: der DAC startet mit 0 V, danach sofort die
    //     Gegenrichtung;
    //   - beim Abluftstoss des ego: Stufe 4 legt beide Motoren auf 1,11 V,
    //     Motor 1 kommt aus der oberen Haelfte - die Verbundrichtung bleibt;
    //   - beim Start: vorher lag der Werkswert 0 V an.
    // Dieser Waechter sitzt deshalb direkt vor dem Schreiben, je Motor, und
    // fragt nicht nach dem Grund.
    //
    // Regel: Ein Sollwert auf der anderen Seite von 5 V als der zuletzt
    // gefahrene darf erst hinaus, wenn der Kanal die Totzeit lang bei 5,00 V
    // stand. Nach restart() ist die Seite unbekannt: dann braucht JEDE Seite
    // die volle Totzeit ab dem restart.
    //
    // Nur fuer bipolare Kanaele. Reine Logik ohne Arduino - nativ getestet.
    // ========================================================================
    class DirectionGuard
    {
      public:
        /// Seite einer Spannung: -1 unter, 0 bei, +1 ueber 5 V. Toleranz 50 mV
        /// fuer Kalibrierung und Rundung.
        static int8_t sideOf(float volt)
        {
            if (volt < 4.95f)
                return -1;
            if (volt > 5.05f)
                return 1;
            return 0;
        }

        /// Seite unbekannt machen (Start, 12-V-Wiederkehr): ab jetzt die volle
        /// Totzeit bei 5,00 V, bevor irgendeine Seite hinaus darf.
        void restart(uint32_t now)
        {
            mKnown = false;
            mAtFive = true;
            mFiveSince = now;
        }

        /// Darf ein Sollwert dieser Seite jetzt geschrieben werden? false heisst:
        /// der Aufrufer schreibt stattdessen 5,00 V.
        bool allow(int8_t side, uint32_t now, uint32_t deadTimeMs) const
        {
            if (side == 0)
                return true;
            if (mKnown && side == mSide)
                return true;                       // dieselbe Seite, kein Wechsel
            if (!mAtFive)
                return false;                      // erst auf 5,00 V
            return (now - mFiveSince) >= deadTimeMs;
        }

        /// Was tatsaechlich geschrieben wurde.
        void written(int8_t side, uint32_t now)
        {
            if (side == 0)
            {
                if (!mAtFive)
                {
                    mAtFive = true;
                    mFiveSince = now;
                }
                return;
            }
            mAtFive = false;
            mSide = side;
            mKnown = true;
        }

        bool atFive() const { return mAtFive; }

      private:
        int8_t mSide = 0;        ///< zuletzt gefahrene Seite (nur gueltig, wenn mKnown)
        bool mKnown = false;     ///< false bis zum ersten Sollwert nach restart()
        bool mAtFive = true;
        uint32_t mFiveSince = 0;
    };
} // namespace Kwl
