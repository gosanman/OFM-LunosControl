// Tests der Grenzwert-Treppe.
//
// Vorgaben aus docs/PLAN.md Phase 4b:
//   rF innen [%]  45 / 50 / 55 / 60 / 65
//   CO2 [ppm]    700 / 850 / 1000 / 1300 / 1700
//   VOC [ppb]    300 / 500 / 750 / 1000 / 1500
//   VOC-Index    100 / 150 / 200 / 300 / 400
//
// Geprueft wird vor allem das, was eine reine Schwellwertabfrage NICHT leistet:
// dass ein Messwert zwischen Ein- und Ausschaltgrenzwert die Stufe stehen laesst.

#include <unity.h>

#include <cmath>

#include "StageLadder.h"

using namespace Kwl;

namespace
{
    constexpr float kRh[5] = {45.0f, 50.0f, 55.0f, 60.0f, 65.0f};
    constexpr float kCo2[5] = {700.0f, 850.0f, 1000.0f, 1300.0f, 1700.0f};
    constexpr float kVocPpb[5] = {300.0f, 500.0f, 750.0f, 1000.0f, 1500.0f};
    constexpr float kVocIndex[5] = {100.0f, 150.0f, 200.0f, 300.0f, 400.0f};
} // namespace

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- Aufwaerts

static void test_aufwaerts_stufe_fuer_stufe(void)
{
    StageLadder l;
    l.setThresholds(kRh);

    TEST_ASSERT_EQUAL_UINT8(0, l.update(40.0f));
    TEST_ASSERT_EQUAL_UINT8(0, l.update(49.9f)); // GW1 noch nicht erreicht
    TEST_ASSERT_EQUAL_UINT8(1, l.update(50.0f)); // GW1 genau erreicht: ein
    TEST_ASSERT_EQUAL_UINT8(2, l.update(55.0f));
    TEST_ASSERT_EQUAL_UINT8(3, l.update(60.0f));
    TEST_ASSERT_EQUAL_UINT8(4, l.update(65.0f));
    TEST_ASSERT_EQUAL_UINT8(4, l.update(90.0f)); // hoeher geht nicht
}

static void test_sprung_ueber_mehrere_stufen(void)
{
    // Ein Schwall feuchter Luft darf nicht vier Aufrufe brauchen, bis die Anlage
    // oben ist.
    StageLadder l;
    l.setThresholds(kRh);
    TEST_ASSERT_EQUAL_UINT8(4, l.update(80.0f));

    StageLadder m;
    m.setThresholds(kCo2);
    TEST_ASSERT_EQUAL_UINT8(3, m.update(1300.0f));
}

// ---------------------------------------------------------------- Abwaerts

static void test_abwaerts_stufe_fuer_stufe(void)
{
    StageLadder l;
    l.setThresholds(kRh);
    TEST_ASSERT_EQUAL_UINT8(4, l.update(70.0f));

    TEST_ASSERT_EQUAL_UINT8(4, l.update(60.0f)); // GW3 genau: noch nicht aus
    TEST_ASSERT_EQUAL_UINT8(3, l.update(59.9f)); // unter GW3: aus
    TEST_ASSERT_EQUAL_UINT8(2, l.update(54.9f));
    TEST_ASSERT_EQUAL_UINT8(1, l.update(49.9f));
    TEST_ASSERT_EQUAL_UINT8(0, l.update(44.9f));
    TEST_ASSERT_EQUAL_UINT8(0, l.update(0.0f));
}

static void test_sprung_abwaerts(void)
{
    StageLadder l;
    l.setThresholds(kCo2);
    TEST_ASSERT_EQUAL_UINT8(4, l.update(2000.0f));
    TEST_ASSERT_EQUAL_UINT8(0, l.update(400.0f));
}

// ---------------------------------------------------------------- Hysterese

static void test_hysterese_haelt_die_stufe(void)
{
    // Der Kern der Treppe: zwischen Ein- und Ausschaltgrenzwert passiert nichts.
    // Stufe 2 ist ein ab 55 und aus erst unter 50 - dazwischen bleibt sie.
    StageLadder l;
    l.setThresholds(kRh);
    TEST_ASSERT_EQUAL_UINT8(2, l.update(55.0f));

    TEST_ASSERT_EQUAL_UINT8(2, l.update(54.0f));
    TEST_ASSERT_EQUAL_UINT8(2, l.update(52.0f));
    TEST_ASSERT_EQUAL_UINT8(2, l.update(50.0f));
    TEST_ASSERT_EQUAL_UINT8(2, l.update(54.9f));
    TEST_ASSERT_EQUAL_UINT8(1, l.update(49.9f)); // erst hier faellt sie
}

static void test_niedrigerer_grenzwert_aendert_nichts(void)
{
    // "Ist eine hoehere Stufe aktiv, aendert das UEBERSCHREITEN eines niedrigeren
    // Grenzwerts nichts." Gemeint ist die Fahrt nach oben: wer schon auf Stufe 3
    // steht, bekommt durch erneutes Ueberschreiten von GW1 oder GW2 keine Stufe
    // dazu. Nach unten gilt das nicht - dort ist GW2 der Ausschaltpunkt von
    // Stufe 3, und darunter faellt sie sehr wohl.
    StageLadder l;
    l.setThresholds(kRh);
    TEST_ASSERT_EQUAL_UINT8(3, l.update(61.0f));

    TEST_ASSERT_EQUAL_UINT8(3, l.update(56.0f)); // ueber GW2, unter GW3
    TEST_ASSERT_EQUAL_UINT8(3, l.update(62.0f)); // ueber GW3 - keine Stufe dazu
    TEST_ASSERT_EQUAL_UINT8(3, l.update(55.0f)); // genau GW2: haelt noch

    // Erst unter GW2 faellt sie, und zwar um genau eine Stufe.
    TEST_ASSERT_EQUAL_UINT8(2, l.update(54.9f));
}

static void test_dauerhaftes_pendeln_um_einen_grenzwert(void)
{
    // Messrauschen um GW2 herum darf die Stufe nicht im Takt der Messwerte
    // umschalten. Nach dem Einschalten von Stufe 2 liegt der Ausschaltpunkt 5 %
    // tiefer; ein Rauschen von +/- 0,5 % bleibt folgenlos.
    StageLadder l;
    l.setThresholds(kRh);
    l.update(55.0f);
    for (int i = 0; i < 20; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(2, l.update(54.6f));
        TEST_ASSERT_EQUAL_UINT8(2, l.update(55.4f));
    }
}

// ---------------------------------------------------------------- Einheitenfrei

static void test_co2_treppe(void)
{
    StageLadder l;
    l.setThresholds(kCo2);
    TEST_ASSERT_EQUAL_UINT8(0, l.update(600.0f));
    TEST_ASSERT_EQUAL_UINT8(1, l.update(850.0f));
    TEST_ASSERT_EQUAL_UINT8(1, l.update(700.0f)); // GW0: noch an
    TEST_ASSERT_EQUAL_UINT8(0, l.update(699.0f));
    TEST_ASSERT_EQUAL_UINT8(4, l.update(1700.0f));
}

static void test_voc_ppb_und_index_gleiche_treppe(void)
{
    // VOC ist einheitenfrei - ppb und Index unterscheiden sich nur in den Zahlen.
    StageLadder ppb;
    ppb.setThresholds(kVocPpb);
    TEST_ASSERT_EQUAL_UINT8(2, ppb.update(750.0f));
    TEST_ASSERT_EQUAL_UINT8(2, ppb.update(500.0f));
    TEST_ASSERT_EQUAL_UINT8(1, ppb.update(499.0f));

    StageLadder idx;
    idx.setThresholds(kVocIndex);
    TEST_ASSERT_EQUAL_UINT8(2, idx.update(200.0f));
    TEST_ASSERT_EQUAL_UINT8(2, idx.update(150.0f));
    TEST_ASSERT_EQUAL_UINT8(1, idx.update(149.0f));
}

// ---------------------------------------------------------------- Randfaelle

static void test_vorgabe_ohne_parametrierung(void)
{
    // Eine nicht parametrierte Treppe traegt die rF-Vorgabe, nicht lauter Nullen -
    // sonst stuende sie beim ersten Messwert sofort auf Stufe 4.
    StageLadder l;
    TEST_ASSERT_EQUAL_FLOAT(45.0f, l.threshold(0));
    TEST_ASSERT_EQUAL_FLOAT(65.0f, l.threshold(4));
    TEST_ASSERT_EQUAL_UINT8(0, l.update(30.0f));
    TEST_ASSERT_EQUAL_UINT8(2, l.update(56.0f));
}

static void test_fallende_grenzwerte_werden_zurechtgerueckt(void)
{
    // Die ETS kann eine unsinnige Reihe zulassen. Eine fallende Treppe wuerde
    // bedeuten, dass derselbe Messwert eine Stufe gleichzeitig ein- und
    // ausschaltet; sie wird aufsteigend zurechtgerueckt.
    const float bad[5] = {60.0f, 50.0f, 55.0f, 40.0f, 65.0f};
    StageLadder l;
    l.setThresholds(bad);

    TEST_ASSERT_EQUAL_FLOAT(60.0f, l.threshold(0));
    TEST_ASSERT_EQUAL_FLOAT(60.0f, l.threshold(1));
    TEST_ASSERT_EQUAL_FLOAT(60.0f, l.threshold(2));
    TEST_ASSERT_EQUAL_FLOAT(60.0f, l.threshold(3));
    TEST_ASSERT_EQUAL_FLOAT(65.0f, l.threshold(4));

    // Sie muss trotzdem terminieren und darf nicht schwingen.
    TEST_ASSERT_EQUAL_UINT8(0, l.update(59.0f));
    TEST_ASSERT_EQUAL_UINT8(3, l.update(60.0f));
    TEST_ASSERT_EQUAL_UINT8(4, l.update(65.0f));
    TEST_ASSERT_EQUAL_UINT8(4, l.update(60.0f));
    TEST_ASSERT_EQUAL_UINT8(0, l.update(59.0f));
}

static void test_alle_grenzwerte_gleich(void)
{
    // Entartete Treppe ohne Hysterese: erlaubt, aber sie muss sauber zwischen 0
    // und 4 schalten statt haengenzubleiben.
    const float flat[5] = {50.0f, 50.0f, 50.0f, 50.0f, 50.0f};
    StageLadder l;
    l.setThresholds(flat);
    TEST_ASSERT_EQUAL_UINT8(4, l.update(50.0f));
    TEST_ASSERT_EQUAL_UINT8(0, l.update(49.9f));
    TEST_ASSERT_EQUAL_UINT8(4, l.update(80.0f));
}

static void test_nan_laesst_die_stufe_stehen(void)
{
    // Ein ausgefallener Sensor ist kein Grund, die Lueftung zu verstellen.
    StageLadder l;
    l.setThresholds(kRh);
    TEST_ASSERT_EQUAL_UINT8(2, l.update(56.0f));

    TEST_ASSERT_EQUAL_UINT8(2, l.update(std::nanf("")));
    TEST_ASSERT_EQUAL_UINT8(2, l.stage());
    TEST_ASSERT_EQUAL_UINT8(0, l.update(40.0f)); // danach wieder normal
}

static void test_reset(void)
{
    // Wird die Fuehrung gesperrt (Feuchtevergleich) und spaeter wieder erlaubt,
    // darf die Treppe nicht auf ihrer alten Stufe weitermachen.
    StageLadder l;
    l.setThresholds(kRh);
    TEST_ASSERT_EQUAL_UINT8(4, l.update(70.0f));
    l.reset();
    TEST_ASSERT_EQUAL_UINT8(0, l.stage());
    TEST_ASSERT_EQUAL_UINT8(2, l.update(56.0f)); // neu bewertet, nicht fortgesetzt
}

static void test_neue_grenzwerte_aendern_die_stufe_nicht_sofort(void)
{
    // setThresholds kommt aus der Parametrierung, nicht aus einem Messwert - es
    // darf fuer sich genommen keine Stufe verstellen.
    StageLadder l;
    l.setThresholds(kRh);
    TEST_ASSERT_EQUAL_UINT8(3, l.update(61.0f));
    l.setThresholds(kCo2);
    TEST_ASSERT_EQUAL_UINT8(3, l.stage());
    TEST_ASSERT_EQUAL_UINT8(0, l.update(600.0f)); // erst der Messwert wirkt
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_aufwaerts_stufe_fuer_stufe);
    RUN_TEST(test_sprung_ueber_mehrere_stufen);
    RUN_TEST(test_abwaerts_stufe_fuer_stufe);
    RUN_TEST(test_sprung_abwaerts);
    RUN_TEST(test_hysterese_haelt_die_stufe);
    RUN_TEST(test_niedrigerer_grenzwert_aendert_nichts);
    RUN_TEST(test_dauerhaftes_pendeln_um_einen_grenzwert);
    RUN_TEST(test_co2_treppe);
    RUN_TEST(test_voc_ppb_und_index_gleiche_treppe);
    RUN_TEST(test_vorgabe_ohne_parametrierung);
    RUN_TEST(test_fallende_grenzwerte_werden_zurechtgerueckt);
    RUN_TEST(test_alle_grenzwerte_gleich);
    RUN_TEST(test_nan_laesst_die_stufe_stehen);
    RUN_TEST(test_reset);
    RUN_TEST(test_neue_grenzwerte_aendern_die_stufe_nicht_sofort);
    return UNITY_END();
}
