#pragma once

#include <stdint.h>

// ============================================================================
// Gemeinsame Typen und Konstanten der Lueftersteuerung.
//
// Diese Datei ist bewusst frei von Arduino, KNX und Board-Headern: sie muss sich
// auch in der nativen Testumgebung uebersetzen lassen.
// ============================================================================

namespace Kwl
{
    /// Geraetetyp des angeschlossenen Luefters. Die Werte entsprechen dem
    /// ETS-Parameter FAN_Type und duerfen deshalb nicht umsortiert werden.
    enum class FanType : uint8_t
    {
        E2_60 = 0,           ///< LUNOS e²60, bipolar, ein Kanal
        EGO = 1,             ///< LUNOS ego, bipolar, ZWEI Kanaele (zwei Motoren)
        RA_15_60 = 2,        ///< LUNOS RA 15-60, unipolar, ein Kanal
        GENERIC_BIPOLAR = 3, ///< beliebiger Luefter mit Stellsignal um 5 V
        GENERIC_UNIPOLAR = 4 ///< beliebiger Luefter mit 0-10 V
    };

    /// Foerderrichtung. Der Modulcode rechnet ausschliesslich in Zuluft und
    /// Abluft; welche Spannungshaelfte das jeweils ist, uebersetzt allein
    /// KwlCurve anhand der Polaritaet aus dem Board-Header (Messung M1).
    enum class Direction : uint8_t
    {
        Supply = 0,  ///< Zuluft, foerdert ins Gebaeude
        Exhaust = 1  ///< Abluft, foerdert nach draussen
    };

    /// Welcher Pendeltakt gilt. Die Zykluszeit selbst steht am Verbund; hier steht
    /// nur, WER sie bestimmt.
    ///
    /// WRG ist die Pendelbewegung: der kurze Zyklus (70 s) gewinnt Waerme zurueck,
    /// der lange (1 h, Sommer) tut das praktisch nicht mehr - er wechselt aber
    /// weiter die Richtung. "Sommer" heisst also nicht "feste Richtung".
    enum class CycleRule : uint8_t
    {
        FromOperatingMode = 0, ///< keine Vorgabe, die Betriebsart entscheidet
        Wrg = 1,               ///< kurzer Zyklus, Waermerueckgewinnung
        Summer = 2             ///< langer Zyklus, Sommerbetrieb
    };

    /// Hoechste Luefterstufe. Stufe 0 ist Stillstand.
    constexpr uint8_t kStageMax = 4;

    /// Groesster logischer DAC-Code (15 Bit).
    constexpr uint16_t kDacMax = 0x7FFF;

    /// Codes je Volt im 0-10-V-Modus des GP8413, Referenzdesign Abschnitt 3.4.
    constexpr double kCodePerVolt = 3276.7;

    /// Hoechste darstellbare Ausgangsspannung.
    constexpr float kVoltMax = 10.0f;

    /// Sicherer Zustand eines bipolaren Kanals: Stillstand.
    /// Ein DAC-Code 0 ist hier KEIN "aus", sondern Volllast in eine Richtung.
    constexpr float kVoltStandstillBipolar = 5.0f;

    /// Sicherer Zustand eines unipolaren Kanals.
    constexpr float kVoltStandstillUnipolar = 0.0f;

    /// Spezifischer Widerstand von Kupfer in Ohm*mm²/m.
    constexpr float kRhoCopper = 0.0175f;
} // namespace Kwl
