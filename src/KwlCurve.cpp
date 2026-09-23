#include "KwlCurve.h"

#include <cmath>

namespace Kwl
{
    namespace
    {
        // Sollspannungen aus Referenzdesign Abschnitt 3.4. Nominalwerte, gemessen an der
        // LUNOS-Steuerung 5/SC-FT v5.14 - keine Datenblattwerte.
        //
        // `low`  ist die Spannungshaelfte unter 5,00 V, `high` die darueber. Welche davon
        // Zuluft ist, entscheidet erst die Polaritaet des Boards.
        struct Table
        {
            float low[kStageMax + 1];
            float high[kStageMax + 1];
            uint8_t flow[kStageMax + 1];
        };

        constexpr Table kE2_60 = {
            {5.00f, 4.01f, 3.21f, 1.96f, 0.01f},
            {5.00f, 6.01f, 6.91f, 8.15f, 10.00f},
            {0, 5, 20, 40, 60}};

        // Stufe 4 ist beim ego der Abluftstoss: BEIDE Motoren auf 1,11 V, gleichsinnig
        // nach draussen. Die Eintraege sind deshalb absichtlich gleich und nicht um 5 V
        // gespiegelt; stageVolt behandelt diese Stufe gesondert.
        constexpr Table kEgo = {
            {5.00f, 3.26f, 2.09f, 1.25f, 1.11f},
            {5.00f, 6.85f, 8.00f, 8.85f, 1.11f},
            {0, 5, 10, 20, 45}};

        // Unipolar: 0 V ist aus, kein Pendelbetrieb. Beide Haelften gleich, damit die
        // Richtung rechnerisch folgenlos bleibt.
        constexpr Table kRa = {
            {0.01f, 1.58f, 3.57f, 5.76f, 8.15f},
            {0.01f, 1.58f, 3.57f, 5.76f, 8.15f},
            {0, 15, 30, 45, 60}};

        // Generisch bipolar: dieselben Vorgaben wie in der ETS, damit Applikation und
        // Firmware dasselbe sagen, solange niemand etwas eintraegt.
        constexpr Table kGenericBipolar = {
            {5.00f, 4.00f, 3.00f, 2.00f, 1.00f},
            {5.00f, 6.00f, 7.00f, 8.00f, 9.00f},
            {0, 10, 20, 30, 40}};

        // Generisch unipolar: aufsteigend von 0 V. Diese Tabelle ist ein Rueckfallwert -
        // bei generischen Typen zeigt die ETS die Kennlinienfelder immer an, ihre Werte
        // gewinnen also ohnehin.
        constexpr Table kGenericUnipolar = {
            {0.00f, 2.50f, 5.00f, 7.50f, 10.00f},
            {0.00f, 2.50f, 5.00f, 7.50f, 10.00f},
            {0, 10, 20, 30, 40}};

        const Table& tableFor(FanType type)
        {
            switch (type)
            {
                case FanType::EGO:
                    return kEgo;
                case FanType::RA_15_60:
                    return kRa;
                case FanType::GENERIC_BIPOLAR:
                    return kGenericBipolar;
                case FanType::GENERIC_UNIPOLAR:
                    return kGenericUnipolar;
                case FanType::E2_60:
                default:
                    return kE2_60;
            }
        }
    } // namespace

    uint8_t Curve::clampStage(uint8_t stage)
    {
        return stage > kStageMax ? kStageMax : stage;
    }

    uint16_t Curve::voltToCode(float volt)
    {
        // Die Bedingung ist absichtlich so herum formuliert: NaN erfuellt sie nicht und
        // faellt damit auf 0, statt in lround() ein undefiniertes Ergebnis zu erzeugen.
        if (!(volt > 0.0f))
            return 0;

        const long code = std::lround(static_cast<double>(volt) * kCodePerVolt);
        if (code <= 0)
            return 0;
        if (code >= static_cast<long>(kDacMax))
            return kDacMax;
        return static_cast<uint16_t>(code);
    }

    uint16_t Curve::codeToWire(uint16_t code, bool leftAligned)
    {
        if (code > kDacMax)
            code = kDacMax;
        return leftAligned ? static_cast<uint16_t>(code << 1) : code;
    }

    bool Curve::isBipolar(FanType type)
    {
        return type == FanType::E2_60 || type == FanType::EGO ||
               type == FanType::GENERIC_BIPOLAR;
    }

    uint8_t Curve::channelsNeeded(FanType type)
    {
        return type == FanType::EGO ? 2 : 1;
    }

    float Curve::safeVolt(FanType type)
    {
        return isBipolar(type) ? kVoltStandstillBipolar : kVoltStandstillUnipolar;
    }

    uint8_t Curve::nominalFlow(FanType type, uint8_t stage)
    {
        return tableFor(type).flow[clampStage(stage)];
    }

    VoltPair Curve::stageVolt(FanType type, uint8_t stage, Direction dir,
                              bool below5vIsSupply)
    {
        const uint8_t s = clampStage(stage);
        const Table& t = tableFor(type);

        // Unipolar kennt keine Richtung.
        if (!isBipolar(type))
            return {t.low[s], t.low[s]};

        // Abluftstoss des ego: beide Motoren gleichsinnig, Richtung ohne Bedeutung.
        if (type == FanType::EGO && s == kStageMax)
            return {t.low[s], t.high[s]};

        // Welche Haelfte foerdert in die verlangte Richtung?
        const bool useLow = (dir == Direction::Supply) == below5vIsSupply;
        const float primary = useLow ? t.low[s] : t.high[s];
        const float secondary = useLow ? t.high[s] : t.low[s];

        // Einkanalig: nur `a` zaehlt. Beim ego liegt Motor 2 in der Gegenhaelfte, damit
        // das Geraet fuer sich pendelt; bei Richtungswechsel tauschen beide.
        if (channelsNeeded(type) == 1)
            return {primary, primary};
        return {primary, secondary};
    }
} // namespace Kwl
