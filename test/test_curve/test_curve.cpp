// Tests der Kennlinienschicht.
//
// Alle Erwartungswerte stammen aus docs/Referenzdesign_Rev4_GP8413.md Abschnitt 3.4
// ("Sollwerttabelle mit DAC-Codes"). Sie sind dort aus Messungen an der
// LUNOS-Steuerung 5/SC-FT v5.14 hergeleitet, nicht aus Datenblaettern - und sie
// sind hier absichtlich als Zahlen ausgeschrieben statt aus derselben Tabelle
// gerechnet, gegen die geprueft wird.

#include <unity.h>

#include "KwlCurve.h"

using namespace Kwl;

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- Volt -> Code

static void test_code_e2_60_kanal_a(void)
{
    TEST_ASSERT_EQUAL_UINT16(16384, Curve::voltToCode(5.00f)); // 0x4000, Stillstand
    TEST_ASSERT_EQUAL_UINT16(13140, Curve::voltToCode(4.01f)); // 0x3354
    TEST_ASSERT_EQUAL_UINT16(10518, Curve::voltToCode(3.21f)); // 0x2916
    TEST_ASSERT_EQUAL_UINT16(6422, Curve::voltToCode(1.96f));  // 0x1916
    TEST_ASSERT_EQUAL_UINT16(33, Curve::voltToCode(0.01f));    // 0x0021
}

static void test_code_e2_60_kanal_b(void)
{
    TEST_ASSERT_EQUAL_UINT16(19693, Curve::voltToCode(6.01f)); // 0x4CED
    TEST_ASSERT_EQUAL_UINT16(22642, Curve::voltToCode(6.91f)); // 0x5872
    TEST_ASSERT_EQUAL_UINT16(26705, Curve::voltToCode(8.15f)); // 0x6851
    TEST_ASSERT_EQUAL_UINT16(32767, Curve::voltToCode(10.00f)); // 0x7FFF
}

static void test_code_ego(void)
{
    TEST_ASSERT_EQUAL_UINT16(10682, Curve::voltToCode(3.26f)); // 0x29BA
    TEST_ASSERT_EQUAL_UINT16(22445, Curve::voltToCode(6.85f)); // 0x57AD
    TEST_ASSERT_EQUAL_UINT16(6848, Curve::voltToCode(2.09f));  // 0x1AC0
    TEST_ASSERT_EQUAL_UINT16(26214, Curve::voltToCode(8.00f)); // 0x6666
    TEST_ASSERT_EQUAL_UINT16(4096, Curve::voltToCode(1.25f));  // 0x1000
    TEST_ASSERT_EQUAL_UINT16(28999, Curve::voltToCode(8.85f)); // 0x7147
    TEST_ASSERT_EQUAL_UINT16(3637, Curve::voltToCode(1.11f));  // 0x0E35, Abluftstoss
}

static void test_code_ra_15_60(void)
{
    TEST_ASSERT_EQUAL_UINT16(33, Curve::voltToCode(0.01f));    // 0x0021
    TEST_ASSERT_EQUAL_UINT16(5177, Curve::voltToCode(1.58f));  // 0x1439
    TEST_ASSERT_EQUAL_UINT16(11698, Curve::voltToCode(3.57f)); // 0x2DB2
    TEST_ASSERT_EQUAL_UINT16(18874, Curve::voltToCode(5.76f)); // 0x49BA
    TEST_ASSERT_EQUAL_UINT16(26705, Curve::voltToCode(8.15f)); // 0x6851
}

static void test_code_geklemmt(void)
{
    // Ein negativer oder zu grosser Zwischenwert ist ein Bug, kein Grenzfall -
    // aber er darf niemals ueberlaufen und dadurch aus Stillstand Vollgas machen.
    TEST_ASSERT_EQUAL_UINT16(0, Curve::voltToCode(0.0f));
    TEST_ASSERT_EQUAL_UINT16(0, Curve::voltToCode(-1.0f));
    TEST_ASSERT_EQUAL_UINT16(0, Curve::voltToCode(-1000.0f));
    TEST_ASSERT_EQUAL_UINT16(kDacMax, Curve::voltToCode(10.0f));
    TEST_ASSERT_EQUAL_UINT16(kDacMax, Curve::voltToCode(10.5f));
    TEST_ASSERT_EQUAL_UINT16(kDacMax, Curve::voltToCode(1000.0f));
}

// ---------------------------------------------------------------- Busformat

static void test_wire_beide_formate(void)
{
    // Rechtsbuendig: der logische Code geht unveraendert auf den Bus.
    TEST_ASSERT_EQUAL_UINT16(16384, Curve::codeToWire(16384, false));
    TEST_ASSERT_EQUAL_UINT16(32767, Curve::codeToWire(32767, false));
    TEST_ASSERT_EQUAL_UINT16(0, Curve::codeToWire(0, false));

    // Linksbuendig: ein Bit nach links. Genau hier entscheidet sich, ob 5,00 V
    // auch 5,00 V werden - die falsche Wahl verdoppelt jede Ausgangsspannung.
    TEST_ASSERT_EQUAL_UINT16(32768, Curve::codeToWire(16384, true));
    TEST_ASSERT_EQUAL_UINT16(65534, Curve::codeToWire(32767, true));
    TEST_ASSERT_EQUAL_UINT16(0, Curve::codeToWire(0, true));
}

// ---------------------------------------------------------------- sicherer Zustand

static void test_sicherer_zustand(void)
{
    TEST_ASSERT_EQUAL_FLOAT(5.0f, Curve::safeVolt(FanType::E2_60));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, Curve::safeVolt(FanType::EGO));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, Curve::safeVolt(FanType::GENERIC_BIPOLAR));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, Curve::safeVolt(FanType::RA_15_60));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, Curve::safeVolt(FanType::GENERIC_UNIPOLAR));

    // Der sichere Zustand eines bipolaren Kanals muss 0x4000 ergeben, nicht 0.
    TEST_ASSERT_EQUAL_UINT16(0x4000, Curve::voltToCode(Curve::safeVolt(FanType::E2_60)));
    TEST_ASSERT_EQUAL_UINT16(0, Curve::voltToCode(Curve::safeVolt(FanType::RA_15_60)));
}

static void test_typmerkmale(void)
{
    TEST_ASSERT_TRUE(Curve::isBipolar(FanType::E2_60));
    TEST_ASSERT_TRUE(Curve::isBipolar(FanType::EGO));
    TEST_ASSERT_TRUE(Curve::isBipolar(FanType::GENERIC_BIPOLAR));
    TEST_ASSERT_FALSE(Curve::isBipolar(FanType::RA_15_60));
    TEST_ASSERT_FALSE(Curve::isBipolar(FanType::GENERIC_UNIPOLAR));

    TEST_ASSERT_EQUAL_UINT8(2, Curve::channelsNeeded(FanType::EGO));
    TEST_ASSERT_EQUAL_UINT8(1, Curve::channelsNeeded(FanType::E2_60));
    TEST_ASSERT_EQUAL_UINT8(1, Curve::channelsNeeded(FanType::RA_15_60));
}

// ---------------------------------------------------------------- Stufe + Richtung

static void test_e2_60_richtung_polaritaet_normal(void)
{
    // below5vIsSupply = true: unter 5 V foerdert der Luefter Zuluft.
    VoltPair v = Curve::stageVolt(FanType::E2_60, 1, Direction::Supply, true);
    TEST_ASSERT_EQUAL_FLOAT(4.01f, v.a);
    v = Curve::stageVolt(FanType::E2_60, 1, Direction::Exhaust, true);
    TEST_ASSERT_EQUAL_FLOAT(6.01f, v.a);

    v = Curve::stageVolt(FanType::E2_60, 4, Direction::Supply, true);
    TEST_ASSERT_EQUAL_FLOAT(0.01f, v.a);
    v = Curve::stageVolt(FanType::E2_60, 4, Direction::Exhaust, true);
    TEST_ASSERT_EQUAL_FLOAT(10.00f, v.a);
}

static void test_e2_60_richtung_polaritaet_vertauscht(void)
{
    // below5vIsSupply = false: dieselbe Stufe, gespiegelte Zuordnung. Faellt
    // Messung M1 anders aus als angenommen, muss sich NUR dieser Schalter aendern.
    VoltPair v = Curve::stageVolt(FanType::E2_60, 1, Direction::Supply, false);
    TEST_ASSERT_EQUAL_FLOAT(6.01f, v.a);
    v = Curve::stageVolt(FanType::E2_60, 1, Direction::Exhaust, false);
    TEST_ASSERT_EQUAL_FLOAT(4.01f, v.a);
}

static void test_stillstand_stufe_0(void)
{
    VoltPair v = Curve::stageVolt(FanType::E2_60, 0, Direction::Supply, true);
    TEST_ASSERT_EQUAL_FLOAT(5.00f, v.a);
    v = Curve::stageVolt(FanType::E2_60, 0, Direction::Exhaust, true);
    TEST_ASSERT_EQUAL_FLOAT(5.00f, v.a);

    v = Curve::stageVolt(FanType::EGO, 0, Direction::Supply, true);
    TEST_ASSERT_EQUAL_FLOAT(5.00f, v.a);
    TEST_ASSERT_EQUAL_FLOAT(5.00f, v.b);

    v = Curve::stageVolt(FanType::RA_15_60, 0, Direction::Supply, true);
    TEST_ASSERT_EQUAL_FLOAT(0.01f, v.a);
}

static void test_ego_motoren_gegenlaeufig(void)
{
    // Stufen 1 bis 3: die beiden Motoren liegen in verschiedenen Haelften, damit
    // das Geraet fuer sich pendelt. Bei Richtungswechsel tauschen sie.
    VoltPair v = Curve::stageVolt(FanType::EGO, 2, Direction::Supply, true);
    TEST_ASSERT_EQUAL_FLOAT(2.09f, v.a);
    TEST_ASSERT_EQUAL_FLOAT(8.00f, v.b);

    v = Curve::stageVolt(FanType::EGO, 2, Direction::Exhaust, true);
    TEST_ASSERT_EQUAL_FLOAT(8.00f, v.a);
    TEST_ASSERT_EQUAL_FLOAT(2.09f, v.b);
}

static void test_ego_abluftstoss_richtungsunabhaengig(void)
{
    // Stufe 4 ist der Abluftstoss: beide Motoren gleichsinnig nach draussen,
    // Richtung ohne Bedeutung. Waeren sie hier gespiegelt, wuerde das Geraet
    // pendeln statt zu foerdern.
    VoltPair s = Curve::stageVolt(FanType::EGO, 4, Direction::Supply, true);
    VoltPair e = Curve::stageVolt(FanType::EGO, 4, Direction::Exhaust, true);
    VoltPair p = Curve::stageVolt(FanType::EGO, 4, Direction::Supply, false);

    TEST_ASSERT_EQUAL_FLOAT(1.11f, s.a);
    TEST_ASSERT_EQUAL_FLOAT(1.11f, s.b);
    TEST_ASSERT_EQUAL_FLOAT(s.a, e.a);
    TEST_ASSERT_EQUAL_FLOAT(s.b, e.b);
    TEST_ASSERT_EQUAL_FLOAT(s.a, p.a);
}

static void test_unipolar_ohne_richtung(void)
{
    for (uint8_t stage = 0; stage <= kStageMax; stage++)
    {
        VoltPair s = Curve::stageVolt(FanType::RA_15_60, stage, Direction::Supply, true);
        VoltPair e = Curve::stageVolt(FanType::RA_15_60, stage, Direction::Exhaust, true);
        VoltPair p = Curve::stageVolt(FanType::RA_15_60, stage, Direction::Supply, false);
        TEST_ASSERT_EQUAL_FLOAT(s.a, e.a);
        TEST_ASSERT_EQUAL_FLOAT(s.a, p.a);
    }
    VoltPair v = Curve::stageVolt(FanType::RA_15_60, 3, Direction::Supply, true);
    TEST_ASSERT_EQUAL_FLOAT(5.76f, v.a);
}

static void test_stufe_wird_geklemmt(void)
{
    // Eine Stufe jenseits von 4 darf nicht in die Tabelle hinauslaufen.
    VoltPair a = Curve::stageVolt(FanType::E2_60, 4, Direction::Supply, true);
    VoltPair b = Curve::stageVolt(FanType::E2_60, 9, Direction::Supply, true);
    VoltPair c = Curve::stageVolt(FanType::E2_60, 255, Direction::Supply, true);
    TEST_ASSERT_EQUAL_FLOAT(a.a, b.a);
    TEST_ASSERT_EQUAL_FLOAT(a.a, c.a);
    TEST_ASSERT_EQUAL_UINT8(60, Curve::nominalFlow(FanType::E2_60, 200));
}

static void test_volumenstrom(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, Curve::nominalFlow(FanType::E2_60, 0));
    TEST_ASSERT_EQUAL_UINT8(5, Curve::nominalFlow(FanType::E2_60, 1));
    TEST_ASSERT_EQUAL_UINT8(20, Curve::nominalFlow(FanType::E2_60, 2));
    TEST_ASSERT_EQUAL_UINT8(40, Curve::nominalFlow(FanType::E2_60, 3));
    TEST_ASSERT_EQUAL_UINT8(60, Curve::nominalFlow(FanType::E2_60, 4));

    TEST_ASSERT_EQUAL_UINT8(45, Curve::nominalFlow(FanType::EGO, 4));
    TEST_ASSERT_EQUAL_UINT8(15, Curve::nominalFlow(FanType::RA_15_60, 1));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_code_e2_60_kanal_a);
    RUN_TEST(test_code_e2_60_kanal_b);
    RUN_TEST(test_code_ego);
    RUN_TEST(test_code_ra_15_60);
    RUN_TEST(test_code_geklemmt);
    RUN_TEST(test_wire_beide_formate);
    RUN_TEST(test_sicherer_zustand);
    RUN_TEST(test_typmerkmale);
    RUN_TEST(test_e2_60_richtung_polaritaet_normal);
    RUN_TEST(test_e2_60_richtung_polaritaet_vertauscht);
    RUN_TEST(test_stillstand_stufe_0);
    RUN_TEST(test_ego_motoren_gegenlaeufig);
    RUN_TEST(test_ego_abluftstoss_richtungsunabhaengig);
    RUN_TEST(test_unipolar_ohne_richtung);
    RUN_TEST(test_stufe_wird_geklemmt);
    RUN_TEST(test_volumenstrom);
    return UNITY_END();
}
