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

    /// Betriebsart. Die Werte 1…4 sind DPT 20.102 (KNX-Standard), 5…7 sind die
    /// drei erweiterten Arten und liegen ausserhalb des Standardbereichs.
    /// Der Wert 0 auf dem KO heisst "Auto" und waehlt die Standard-Betriebsart.
    enum class OperatingMode : uint8_t
    {
        Auto = 0,       ///< nur als KO-Wert: Standard-Betriebsart gilt
        Comfort = 1,    ///< Komfort
        Standby = 2,    ///< Standby
        Eco = 3,        ///< Eco / Nacht
        Protection = 4, ///< Frost-/Gebaeudeschutz
        Boost = 5,      ///< Stosslueften
        Reduction = 6,  ///< Temperatur-Absenkung
        Quiet = 7       ///< Ruhe (Aus)
    };

    /// Zahl der Betriebsarten ohne "Auto".
    constexpr uint8_t kModeCount = 7;

    /// Fehlercodes (PLAN Anhang). Die REIHENFOLGE IST DIE PRIORITAET: liegen
    /// mehrere an, gewinnt der kleinste Wert groesser 0. Neue Codes gehoeren
    /// deshalb ans Ende, sonst verschiebt sich die Rangfolge der bestehenden.
    enum class ErrorCode : uint8_t
    {
        None = 0,
        NoRelease = 1,        ///< Freigabe fehlt
        MasterTimeout = 2,    ///< Lebenszeichen des Masters bleibt aus
        Configuration = 3,    ///< Kanalzahl oder Kennlinie passt nicht zur Platine
        DacUnreachable = 4,   ///< DAC antwortet nicht - Alarm
        BadValue = 5,         ///< ungueltiger Empfangswert
        MonitoringPaused = 6, ///< Ueberwachung ausgesetzt
        HumidityBlocks = 7,   ///< Feuchtevergleich sperrt, kein Alarm
        SensorsMissing = 8,   ///< Sensorwerte fehlen, kein Alarm
        ProtectionActive = 9, ///< Schutzbetrieb aktiv, kein Alarm
        FilterDue = 10,       ///< Filterwechsel faellig, kein Alarm
        DirectionConflict = 11 ///< Richtungskonflikt im Verbund, kein Alarm
    };

    /// Loest dieser Code den Stoerungsausgang (DPT 1.005) aus?
    /// Nur die Codes, bei denen der Knoten nicht mehr das tut, was er soll.
    constexpr bool isAlarm(ErrorCode code)
    {
        return code == ErrorCode::NoRelease || code == ErrorCode::MasterTimeout ||
               code == ErrorCode::Configuration || code == ErrorCode::DacUnreachable ||
               code == ErrorCode::BadValue;
    }

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
