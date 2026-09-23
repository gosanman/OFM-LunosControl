#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    // ========================================================================
    // Leitungskompensation.
    //
    // Ohne getrennte vierte Ader fliesst der Laststrom des Luefters ueber
    // dieselbe Masse wie das Stellsignal. Der Luefter sieht dann eine um diesen
    // Abfall verschobene Spannung - bei bipolarer Ansteuerung heisst das nicht
    // nur "etwas langsamer", sondern eine Verschiebung des Nullpunkts.
    //
    // Referenzdesign Abschnitt 3.5. Die Korrektur ist IMMER positiv, auch fuer
    // Spannungen unter 5 V: es verschiebt sich der Massebezug, nicht das
    // Vorzeichen.
    // ========================================================================
    class CableComp
    {
      public:
        /// Widerstand eines Leiters: rho * Laenge / Querschnitt.
        /// Laenge ist die einfache Strecke, nicht die Schleife.
        static float resistance(float lengthMeter, float sectionMm2);

        /// Spannungsabfall am Rueckleiter.
        static float deltaVolt(float currentAmp, float resistanceOhm);

        /// Sollspannung plus Abfall, geklemmt auf 0…10,00 V.
        ///
        /// Am oberen Anschlag - beim e²60 ist Stufe 4 in einer Richtung 10,00 V -
        /// ist die Korrektur nicht mehr darstellbar. Die Firmware klemmt dann und
        /// meldet nichts; der Luefter bleibt in dieser Richtung geringfuegig unter
        /// Nennluftleistung. Die Alternative waere, Stufe 4 nominal auf 9,90 V zu
        /// senken. Entscheidung offen bis Messung M2 (PLAN Phase 3, Schritt 4).
        static float apply(float nominalVolt, float deltaVolt);
    };
} // namespace Kwl
