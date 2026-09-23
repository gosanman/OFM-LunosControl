// Tests der Leitungskompensation.
//
// Alle Erwartungswerte stammen aus docs/Referenzdesign_Rev4_GP8413.md Abschnitt 3.5
// ("Leitungskompensation"), Tabelle fuer 0,7 mm² Kupfer. Die dortigen Zahlen sind
// hier als Millivolt und Code-Differenz ausgeschrieben, nicht nachgerechnet.
//
// Stroeme: e²60 Stufe 4 = 0,275 A, ego Stufe 4 = 0,41 A, e²60 Stufe 1 = 0,033 A.

#include <unity.h>

#include "CableComp.h"
#include "KwlCurve.h"

using namespace Kwl;

namespace
{
    constexpr float kSection = 0.7f; // mm², Referenzdesign 3.5
    constexpr float kI_e2_st4 = 0.275f;
    constexpr float kI_ego_st4 = 0.41f;
    constexpr float kI_e2_st1 = 0.033f;

    // Die Tabelle im Referenzdesign ist auf ganze Millivolt gerundet (0,41 A an
    // 0,25 Ohm sind 102,5 mV und stehen dort als 103 mV). Feiner als 1 mV laesst
    // sie sich nicht vergleichen - und feiner muss sie auch nicht sein: 1 mV sind
    // drei DAC-Codes von 32767.
    constexpr float kMv = 0.001f;
} // namespace

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- Widerstand

static void test_widerstand_07mm2(void)
{
    // 0,0175 Ohm*mm²/m / 0,7 mm² = 0,025 Ohm je Meter.
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.125f, CableComp::resistance(5.0f, kSection));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.250f, CableComp::resistance(10.0f, kSection));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.375f, CableComp::resistance(15.0f, kSection));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.500f, CableComp::resistance(20.0f, kSection));
}

static void test_widerstand_andere_querschnitte(void)
{
    // Doppelter Querschnitt, halber Widerstand - die Formel darf nicht auf 0,7
    // festgenagelt sein, die ETS laesst den Querschnitt eingeben.
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.0875f, CableComp::resistance(5.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.0583f, CableComp::resistance(5.0f, 1.5f));
}

static void test_widerstand_unsinnige_eingaben(void)
{
    // Laenge oder Querschnitt 0 heisst "keine Kompensation", nicht "Division".
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::resistance(0.0f, kSection));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::resistance(10.0f, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::resistance(-10.0f, kSection));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::resistance(10.0f, -0.7f));
}

// ---------------------------------------------------------------- Spannungsabfall

static void test_abfall_e2_60_stufe4(void)
{
    // Referenzdesign 3.5: 34 / 69 / 103 / 138 mV bei 5 / 10 / 15 / 20 m.
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.034f,
                             CableComp::deltaVolt(kI_e2_st4, CableComp::resistance(5.0f, kSection)));
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.069f,
                             CableComp::deltaVolt(kI_e2_st4, CableComp::resistance(10.0f, kSection)));
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.103f,
                             CableComp::deltaVolt(kI_e2_st4, CableComp::resistance(15.0f, kSection)));
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.138f,
                             CableComp::deltaVolt(kI_e2_st4, CableComp::resistance(20.0f, kSection)));
}

static void test_abfall_ego_stufe4(void)
{
    // Referenzdesign 3.5: 51 / 103 / 154 / 205 mV. Der ego zieht mehr Strom,
    // deshalb ist bei ihm die Kompensation zuerst spuerbar.
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.051f,
                             CableComp::deltaVolt(kI_ego_st4, CableComp::resistance(5.0f, kSection)));
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.103f,
                             CableComp::deltaVolt(kI_ego_st4, CableComp::resistance(10.0f, kSection)));
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.154f,
                             CableComp::deltaVolt(kI_ego_st4, CableComp::resistance(15.0f, kSection)));
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.205f,
                             CableComp::deltaVolt(kI_ego_st4, CableComp::resistance(20.0f, kSection)));
}

static void test_abfall_kleine_stufe_bleibt_klein(void)
{
    // e²60 Stufe 1 bei 15 m: 12 mV. In den unteren Stufen ist die Kompensation
    // praktisch bedeutungslos - das muss sie auch bleiben.
    TEST_ASSERT_FLOAT_WITHIN(kMv, 0.012f,
                             CableComp::deltaVolt(kI_e2_st1, CableComp::resistance(15.0f, kSection)));
}

static void test_abfall_ohne_strom(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::deltaVolt(0.0f, 0.375f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::deltaVolt(0.275f, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::deltaVolt(-0.275f, 0.375f));
}

// ---------------------------------------------------------------- Code-Differenz

static void test_codedifferenz_aus_referenzdesign(void)
{
    // Die Tabelle nennt neben den Millivolt auch die Code-Differenz. Sie ist die
    // eigentlich interessante Groesse: unter etwa 100 Codes liegt die Korrektur im
    // Rauschen der DAC-Aufloesung.
    struct Case
    {
        float current;
        float length;
        uint16_t expected;
    };
    const Case cases[] = {
        {kI_e2_st4, 5.0f, 113},
        {kI_e2_st4, 10.0f, 225},
        {kI_e2_st4, 15.0f, 338},
        {kI_e2_st4, 20.0f, 451},
        {kI_ego_st4, 5.0f, 168},
        {kI_ego_st4, 10.0f, 336},
        {kI_ego_st4, 15.0f, 504},
        {kI_ego_st4, 20.0f, 672}};

    for (const Case& c : cases)
    {
        const float r = CableComp::resistance(c.length, kSection);
        const uint16_t code = Curve::voltToCode(CableComp::deltaVolt(c.current, r));
        TEST_ASSERT_UINT16_WITHIN(1, c.expected, code);
    }
}

// ---------------------------------------------------------------- Anwendung

static void test_korrektur_ist_immer_additiv(void)
{
    // Es verschiebt sich der Massebezug, nicht das Vorzeichen: auch unterhalb von
    // 5 V wird addiert. Ein Abziehen in der unteren Haelfte wuerde den Fehler
    // verdoppeln statt ihn aufzuheben.
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 8.253f, CableComp::apply(8.15f, 0.103f));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 2.063f, CableComp::apply(1.96f, 0.103f));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.113f, CableComp::apply(0.01f, 0.103f));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 5.103f, CableComp::apply(5.00f, 0.103f));
}

static void test_oberer_anschlag_klemmt(void)
{
    // e²60 Stufe 4 in der oberen Haelfte liegt nominal auf 10,00 V. Dort ist die
    // Korrektur nicht mehr darstellbar; sie wird geklemmt, der Luefter bleibt
    // geringfuegig unter Nennleistung. Entscheidung offen bis Messung M2.
    TEST_ASSERT_EQUAL_FLOAT(kVoltMax, CableComp::apply(10.00f, 0.138f));
    TEST_ASSERT_EQUAL_FLOAT(kVoltMax, CableComp::apply(9.95f, 0.205f));
    TEST_ASSERT_EQUAL_UINT16(kDacMax, Curve::voltToCode(CableComp::apply(10.00f, 0.138f)));
}

static void test_unterer_anschlag_klemmt(void)
{
    // Negative Sollspannungen entstehen nicht, aber sie duerfen auch nicht
    // durchrutschen - ein negativer Zwischenwert wuerde im DAC ueberlaufen.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::apply(-1.0f, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CableComp::apply(0.0f, 0.0f));
}

static void test_ohne_kompensation_unveraendert(void)
{
    // Laenge 0 in der ETS heisst "keine Kompensation": die Sollspannung muss
    // Bit fuer Bit dieselbe bleiben, sonst wandert der Stillstandspunkt.
    const float delta = CableComp::deltaVolt(kI_e2_st4, CableComp::resistance(0.0f, kSection));
    TEST_ASSERT_EQUAL_FLOAT(5.00f, CableComp::apply(5.00f, delta));
    TEST_ASSERT_EQUAL_UINT16(0x4000, Curve::voltToCode(CableComp::apply(5.00f, delta)));
}

static void test_stillstand_bleibt_stillstand(void)
{
    // Der sichere Zustand wird NIE kompensiert: bei Stillstand fliesst kein
    // Laststrom, und 5,00 V muss 5,00 V bleiben. Dieser Test haelt die Regel fest,
    // damit spaeterer Antriebscode die Kompensation nicht auf Stufe 0 anwendet.
    const float deltaAtStandstill =
        CableComp::deltaVolt(0.0f, CableComp::resistance(20.0f, kSection));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, deltaAtStandstill);
    TEST_ASSERT_EQUAL_FLOAT(5.00f,
                            CableComp::apply(Curve::safeVolt(FanType::E2_60), deltaAtStandstill));
    TEST_ASSERT_EQUAL_FLOAT(0.00f,
                            CableComp::apply(Curve::safeVolt(FanType::RA_15_60), deltaAtStandstill));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_widerstand_07mm2);
    RUN_TEST(test_widerstand_andere_querschnitte);
    RUN_TEST(test_widerstand_unsinnige_eingaben);
    RUN_TEST(test_abfall_e2_60_stufe4);
    RUN_TEST(test_abfall_ego_stufe4);
    RUN_TEST(test_abfall_kleine_stufe_bleibt_klein);
    RUN_TEST(test_abfall_ohne_strom);
    RUN_TEST(test_codedifferenz_aus_referenzdesign);
    RUN_TEST(test_korrektur_ist_immer_additiv);
    RUN_TEST(test_oberer_anschlag_klemmt);
    RUN_TEST(test_unterer_anschlag_klemmt);
    RUN_TEST(test_ohne_kompensation_unveraendert);
    RUN_TEST(test_stillstand_bleibt_stillstand);
    return UNITY_END();
}
