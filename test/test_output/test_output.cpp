// Tests der Ausgabeschicht.
//
// Hier werden die Sicherheitsinvarianten aus CLAUDE.md geprueft, die sich ohne
// Hardware pruefen lassen:
//
//   3. Startup schreibt zuerst den Bereich, dann den sicheren Zustand.
//   6. Der ego wird immer als Paar geschrieben.
//   8. Vor Neustart sicherer Zustand.
//  10. Ausfall wird gemeldet, nicht geraten.
//
// Der Mock zeichnet die REIHENFOLGE der Buszugriffe auf. Ein Test, der nur den
// Endwert je Kanal prueft, koennte eine falsche Reihenfolge nicht sehen - und
// genau die ist hier der Fehler, der wehtut.

#include <unity.h>

#include "Drive/MockDacDrive.h"
#include "KwlOutput.h"

using namespace Kwl;

using Op = MockDacDrive::Op;

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- Startsequenz

static void test_startsequenz_bereich_vor_sollwert(void)
{
    // Sicherheitsinvariante 3. Steht der Bereich falsch, bedeutet derselbe Code
    // eine andere Spannung - auch der des sicheren Zustands.
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    TEST_ASSERT_TRUE(out.begin());

    const int16_t probe = drive.firstOf(Op::Probe);
    const int16_t configure = drive.firstOf(Op::Configure);
    const int16_t write = drive.firstOf(Op::Write);

    TEST_ASSERT_TRUE(probe >= 0);
    TEST_ASSERT_TRUE(configure > probe);
    TEST_ASSERT_TRUE(write > configure);
}

static void test_startsequenz_schreibt_jeden_kanal_sicher(void)
{
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    out.setChannelType(0, FanType::E2_60);
    out.setChannelType(1, FanType::EGO);
    out.setChannelType(2, FanType::RA_15_60);
    out.setChannelType(3, FanType::GENERIC_UNIPOLAR);
    TEST_ASSERT_TRUE(out.begin());

    TEST_ASSERT_EQUAL_UINT8(4, drive.countOf(Op::Write));

    // Bipolar: 5,00 V = 0x4000. Unipolar: 0 V = 0.
    TEST_ASSERT_EQUAL_UINT16(0x4000, drive.lastWire(0));
    TEST_ASSERT_EQUAL_UINT16(0x4000, drive.lastWire(1));
    TEST_ASSERT_EQUAL_UINT16(0, drive.lastWire(2));
    TEST_ASSERT_EQUAL_UINT16(0, drive.lastWire(3));
}

static void test_kein_sollwert_vor_begin(void)
{
    // Der Kern der Invariante: die Klasse nimmt vorher nichts an und schreibt
    // deshalb auch nichts.
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);

    TEST_ASSERT_FALSE(out.setVolt(0, 8.0f));
    TEST_ASSERT_FALSE(out.setEgo(0, {3.26f, 6.85f}));
    TEST_ASSERT_EQUAL_UINT8(0, drive.count());
    TEST_ASSERT_FALSE(out.ready());
}

static void test_ohne_treiber_kein_start(void)
{
    KwlOutput out;
    TEST_ASSERT_FALSE(out.begin());
    TEST_ASSERT_FALSE(out.ready());
    TEST_ASSERT_EQUAL(ErrorCode::Configuration, out.error());
}

// ---------------------------------------------------------------- Ausfall

static void test_dac_antwortet_nicht(void)
{
    // Sicherheitsinvariante 10: kein stilles Weiterrechnen.
    MockDacDrive drive(4);
    drive.setPresent(false);
    KwlOutput out;
    out.attach(&drive, false);

    TEST_ASSERT_FALSE(out.begin());
    TEST_ASSERT_FALSE(out.ready());
    TEST_ASSERT_EQUAL(ErrorCode::DacUnreachable, out.error());
    TEST_ASSERT_TRUE(isAlarm(out.error()));

    // Und es wurde nichts geschrieben - weder Bereich noch Sollwert.
    TEST_ASSERT_EQUAL_UINT8(0, drive.countOf(Op::Configure));
    TEST_ASSERT_EQUAL_UINT8(0, drive.countOf(Op::Write));
}

static void test_bus_bricht_waehrend_des_betriebs_ab(void)
{
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    TEST_ASSERT_TRUE(out.begin());

    drive.setWritable(false);
    TEST_ASSERT_FALSE(out.setVolt(0, 8.0f));
    TEST_ASSERT_FALSE(out.ready());
    TEST_ASSERT_EQUAL(ErrorCode::DacUnreachable, out.error());

    // Danach wird nichts mehr angenommen, bis begin() erneut laeuft.
    drive.clearLog();
    TEST_ASSERT_FALSE(out.setVolt(0, 8.0f));
    TEST_ASSERT_EQUAL_UINT8(0, drive.count());
}

// ---------------------------------------------------------------- Sollwerte

static void test_sollwert_wird_in_das_busformat_gerechnet(void)
{
    MockDacDrive drive(4);
    KwlOutput rightAligned;
    rightAligned.attach(&drive, false);
    TEST_ASSERT_TRUE(rightAligned.begin());
    TEST_ASSERT_TRUE(rightAligned.setVolt(0, 8.15f));
    TEST_ASSERT_EQUAL_UINT16(26705, drive.lastWire(0));

    // Dieselbe Spannung, anderes Busformat: ein Bit nach links.
    MockDacDrive other(4);
    KwlOutput leftAligned;
    leftAligned.attach(&other, true);
    TEST_ASSERT_TRUE(leftAligned.begin());
    TEST_ASSERT_TRUE(leftAligned.setVolt(0, 8.15f));
    TEST_ASSERT_EQUAL_UINT16(26705 * 2, other.lastWire(0));

    // Und der sichere Zustand ebenso - hier haengt der Unterschied zwischen
    // Stillstand und Vollgas an einem Bit.
    TEST_ASSERT_EQUAL_UINT16(0x8000, other.at(2).wireA);
}

static void test_sollwert_ausserhalb_der_kanaele(void)
{
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    TEST_ASSERT_TRUE(out.begin());
    drive.clearLog();

    TEST_ASSERT_FALSE(out.setVolt(4, 5.0f));
    TEST_ASSERT_FALSE(out.setVolt(200, 5.0f));
    TEST_ASSERT_EQUAL_UINT8(0, drive.count());
    TEST_ASSERT_TRUE(out.ready()); // das ist kein Ausfall
}

static void test_letzte_spannung_wird_gemerkt(void)
{
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    out.setChannelType(2, FanType::RA_15_60);
    TEST_ASSERT_TRUE(out.begin());

    TEST_ASSERT_EQUAL_FLOAT(5.0f, out.lastVolt(0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out.lastVolt(2));

    out.setVolt(0, 6.91f);
    TEST_ASSERT_EQUAL_FLOAT(6.91f, out.lastVolt(0));
}

// ---------------------------------------------------------------- ego

static void test_ego_wird_als_paar_geschrieben(void)
{
    // Sicherheitsinvariante 6: EIN Frame, zwei verschiedene Werte. Zwei getrennte
    // Schreibvorgaenge liessen die Motoren beim Richtungswechsel auseinanderlaufen.
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    out.setChannelType(0, FanType::EGO);
    out.setChannelType(1, FanType::EGO);
    TEST_ASSERT_TRUE(out.begin());
    drive.clearLog();

    TEST_ASSERT_TRUE(out.setEgo(0, {2.09f, 8.00f}));
    TEST_ASSERT_EQUAL_UINT8(1, drive.count());
    TEST_ASSERT_EQUAL_UINT8(0, drive.countOf(Op::Write));
    TEST_ASSERT_EQUAL(Op::WritePair, drive.at(0).op);
    TEST_ASSERT_EQUAL_UINT16(6848, drive.at(0).wireA);
    TEST_ASSERT_EQUAL_UINT16(26214, drive.at(0).wireB);
}

static void test_ego_muss_auf_einem_chip_liegen(void)
{
    // Kanal 1 ist der ZWEITE Ausgang des ersten Chips; ein Paar ab dort laege auf
    // zwei Chips. Das ist ein Parametrierfehler - und er darf nicht in zwei
    // Einzelschreibvorgaenge ausweichen.
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    TEST_ASSERT_TRUE(out.begin());
    drive.clearLog();

    TEST_ASSERT_FALSE(out.setEgo(1, {2.09f, 8.00f}));
    TEST_ASSERT_EQUAL_UINT8(0, drive.count());
    TEST_ASSERT_EQUAL(ErrorCode::Configuration, out.error());

    TEST_ASSERT_TRUE(out.isPairStart(0));
    TEST_ASSERT_FALSE(out.isPairStart(1));
    TEST_ASSERT_TRUE(out.isPairStart(2));
    TEST_ASSERT_FALSE(out.isPairStart(3)); // dahinter kommt kein zweiter Kanal mehr
}

static void test_ego_abluftstoss_beide_motoren_gleich(void)
{
    // Stufe 4 des ego: beide Motoren auf 1,11 V, gleichsinnig nach draussen.
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    TEST_ASSERT_TRUE(out.begin());
    drive.clearLog();

    const VoltPair v = Curve::stageVolt(FanType::EGO, 4, Direction::Exhaust, true);
    TEST_ASSERT_TRUE(out.setEgo(2, v));
    TEST_ASSERT_EQUAL(Op::WritePair, drive.at(0).op);
    TEST_ASSERT_EQUAL_UINT8(2, drive.at(0).channel);
    TEST_ASSERT_EQUAL_UINT16(3637, drive.at(0).wireA);
    TEST_ASSERT_EQUAL_UINT16(3637, drive.at(0).wireB);
}

// ---------------------------------------------------------------- Neustart

static void test_sicherer_zustand_vor_neustart(void)
{
    // Sicherheitsinvariante 8: eine ETS-Neuprogrammierung darf keinen
    // Vollgas-Moment erzeugen.
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    out.setChannelType(2, FanType::RA_15_60);
    TEST_ASSERT_TRUE(out.begin());

    out.setVolt(0, 10.0f); // Vollgas
    out.setVolt(2, 8.15f);
    drive.clearLog();

    TEST_ASSERT_TRUE(out.safeAll());
    TEST_ASSERT_EQUAL_UINT16(0x4000, drive.lastWire(0));
    TEST_ASSERT_EQUAL_UINT16(0x4000, drive.lastWire(1));
    TEST_ASSERT_EQUAL_UINT16(0, drive.lastWire(2)); // unipolar
    TEST_ASSERT_EQUAL_UINT16(0x4000, drive.lastWire(3));
}

static void test_sicherer_zustand_auch_nach_einem_fehler(void)
{
    // Ein Neustart soll nicht daran scheitern, dass vorher etwas schiefging.
    MockDacDrive drive(4);
    KwlOutput out;
    out.attach(&drive, false);
    TEST_ASSERT_TRUE(out.begin());

    drive.setWritable(false);
    out.setVolt(0, 10.0f);
    TEST_ASSERT_FALSE(out.ready());

    drive.setWritable(true);
    drive.clearLog();
    TEST_ASSERT_TRUE(out.safeAll());
    TEST_ASSERT_EQUAL_UINT8(4, drive.countOf(Op::Write));
}

static void test_sicherer_zustand_ohne_treiber(void)
{
    KwlOutput out;
    TEST_ASSERT_FALSE(out.safeAll());
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_startsequenz_bereich_vor_sollwert);
    RUN_TEST(test_startsequenz_schreibt_jeden_kanal_sicher);
    RUN_TEST(test_kein_sollwert_vor_begin);
    RUN_TEST(test_ohne_treiber_kein_start);
    RUN_TEST(test_dac_antwortet_nicht);
    RUN_TEST(test_bus_bricht_waehrend_des_betriebs_ab);
    RUN_TEST(test_sollwert_wird_in_das_busformat_gerechnet);
    RUN_TEST(test_sollwert_ausserhalb_der_kanaele);
    RUN_TEST(test_letzte_spannung_wird_gemerkt);
    RUN_TEST(test_ego_wird_als_paar_geschrieben);
    RUN_TEST(test_ego_muss_auf_einem_chip_liegen);
    RUN_TEST(test_ego_abluftstoss_beide_motoren_gleich);
    RUN_TEST(test_sicherer_zustand_vor_neustart);
    RUN_TEST(test_sicherer_zustand_auch_nach_einem_fehler);
    RUN_TEST(test_sicherer_zustand_ohne_treiber);
    return UNITY_END();
}
