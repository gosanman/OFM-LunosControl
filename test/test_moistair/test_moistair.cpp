// Tests der Feuchterechnung und des Feuchtevergleichs.
//
// Formeln aus docs/PLAN.md Anhang ("Formeln"):
//   e_s = 6.112 * exp(17.62*T / (243.12+T))            [hPa]
//   e   = rF/100 * e_s
//   p   = 1013.25 * (1 - 2.25577e-5*h)^5.2559          [hPa]
//   r   = 622 * e / (p - e)                            [g/kg]
//
// Die Pruefwerte des Anhangs galten urspruenglich 7,3 / 3,1 / 12,0 g/kg "bei 500 m"
// und passten zu keiner einheitlichen Hoehe; die Partialdruecke stimmten. Der Anhang
// ist am 2026-09-22 auf die nachgerechneten Werte korrigiert worden - 7,69 / 3,20 /
// 12,60 g/kg bei 500 m, 7,24 / 3,02 / 11,86 g/kg auf Meereshoehe. Diese Tests pruefen
// beide Hoehen, damit die Zahlen nicht wieder auseinanderlaufen koennen.

#include <unity.h>

#include <cmath>

#include "MoistAir.h"

using namespace Kwl;

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- Magnus

static void test_saettigungsdampfdruck(void)
{
    // 0 °C ist der Ankerpunkt der Formel: e_s = 6.112 hPa.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.112f, MoistAir::saturationPressure(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.326f, MoistAir::saturationPressure(20.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 31.600f, MoistAir::saturationPressure(25.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 73.675f, MoistAir::saturationPressure(40.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.2597f, MoistAir::saturationPressure(-20.0f));
}

static void test_saettigungsdampfdruck_geklemmt(void)
{
    // Ausserhalb -45…+60 °C driftet die Naeherung weg. Ein Sensor, der -300 °C
    // meldet, darf keinen Fantasiewert erzeugen.
    const float atMin = MoistAir::saturationPressure(-45.0f);
    const float atMax = MoistAir::saturationPressure(60.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1117f, atMin);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 199.93f, atMax);
    TEST_ASSERT_EQUAL_FLOAT(atMin, MoistAir::saturationPressure(-300.0f));
    TEST_ASSERT_EQUAL_FLOAT(atMax, MoistAir::saturationPressure(300.0f));
    TEST_ASSERT_EQUAL_FLOAT(atMin, MoistAir::saturationPressure(std::nanf("")));
}

// ---------------------------------------------------------------- Partialdruck

static void test_partialdruck_pruefwerte(void)
{
    // Pruefwerte aus PLAN Anhang - diese drei stimmen.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 11.66f, MoistAir::vapourPressure(20.0f, 50.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 4.89f, MoistAir::vapourPressure(0.0f, 80.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 18.96f, MoistAir::vapourPressure(25.0f, 60.0f));
}

static void test_partialdruck_randwerte(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0.0f, MoistAir::vapourPressure(20.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, MoistAir::saturationPressure(20.0f),
                             MoistAir::vapourPressure(20.0f, 100.0f));
    // rF ueber 100 % oder negativ kommt vom Bus, nicht aus der Physik.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, MoistAir::vapourPressure(20.0f, 100.0f),
                             MoistAir::vapourPressure(20.0f, 150.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, MoistAir::vapourPressure(20.0f, -10.0f));
}

static void test_kalt_und_feucht_ist_trockener_als_warm_und_mittel(void)
{
    // Der Grund, warum ueber Partialdruck verglichen wird und nicht ueber rF:
    // 80 % bei 0 °C enthalten weniger Wasser als 50 % bei 20 °C.
    const float cold = MoistAir::vapourPressure(0.0f, 80.0f);
    const float warm = MoistAir::vapourPressure(20.0f, 50.0f);
    TEST_ASSERT_TRUE(cold < warm);
}

// ---------------------------------------------------------------- Luftdruck

static void test_luftdruck_ueber_hoehe(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1013.25f, MoistAir::pressureAtAltitude(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 954.61f, MoistAir::pressureAtAltitude(500.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 898.75f, MoistAir::pressureAtAltitude(1000.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 794.95f, MoistAir::pressureAtAltitude(2000.0f));
}

static void test_luftdruck_geklemmt(void)
{
    // ETS-Bereich 0…2000 m.
    TEST_ASSERT_EQUAL_FLOAT(MoistAir::pressureAtAltitude(0.0f),
                            MoistAir::pressureAtAltitude(-100.0f));
    TEST_ASSERT_EQUAL_FLOAT(MoistAir::pressureAtAltitude(2000.0f),
                            MoistAir::pressureAtAltitude(5000.0f));
}

// ---------------------------------------------------------------- g/kg

static void test_mischungsverhaeltnis_bei_500m(void)
{
    // Pruefwerte aus PLAN Anhang, Fassung vom 2026-09-22.
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 7.69f, MoistAir::mixingRatioAt(20.0f, 50.0f, 500.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 3.20f, MoistAir::mixingRatioAt(0.0f, 80.0f, 500.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 12.60f, MoistAir::mixingRatioAt(25.0f, 60.0f, 500.0f));
}

static void test_mischungsverhaeltnis_auf_meereshoehe(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 7.24f, MoistAir::mixingRatioAt(20.0f, 50.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 3.02f, MoistAir::mixingRatioAt(0.0f, 80.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 11.86f, MoistAir::mixingRatioAt(25.0f, 60.0f, 0.0f));
}

static void test_mischungsverhaeltnis_steigt_mit_der_hoehe(void)
{
    // Gleicher Partialdruck, weniger Luft drumherum: mehr Gramm je Kilogramm.
    const float low = MoistAir::mixingRatioAt(20.0f, 50.0f, 0.0f);
    const float high = MoistAir::mixingRatioAt(20.0f, 50.0f, 2000.0f);
    TEST_ASSERT_TRUE(high > low);
}

static void test_mischungsverhaeltnis_randfaelle(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0.0f, MoistAir::mixingRatio(0.0f, 1013.25f));
    // Partialdruck oberhalb des Luftdrucks ist unmoeglich - darf aber nicht durch
    // eine Division nahe null laufen.
    const float r = MoistAir::mixingRatio(2000.0f, 1013.25f);
    TEST_ASSERT_TRUE(r > 0.0f);
    TEST_ASSERT_TRUE(std::isfinite(r));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, MoistAir::mixingRatio(10.0f, 0.0f));
}

// ---------------------------------------------------------------- Feuchtevergleich

static void test_vergleich_startet_gesperrt(void)
{
    // Solange nicht gemessen ist, dass draussen trockenere Luft steht, wird nicht
    // entfeuchtet.
    HumidityCompare c;
    TEST_ASSERT_FALSE(c.allowed());
}

static void test_vergleich_schaltet_bei_1_5_hpa_ein(void)
{
    HumidityCompare c; // Vorgabe 1,5 / 0,5 hPa
    TEST_ASSERT_FALSE(c.update(11.0f, 10.0f)); // 1,0 hPa - zu wenig
    TEST_ASSERT_TRUE(c.update(11.5f, 10.0f));  // 1,5 hPa - genau
    TEST_ASSERT_TRUE(c.update(20.0f, 10.0f));
}

static void test_vergleich_haelt_zwischen_den_schwellen(void)
{
    // Zwischen 0,5 und 1,5 hPa bleibt der Zustand stehen - in beide Richtungen.
    HumidityCompare c;
    TEST_ASSERT_TRUE(c.update(12.0f, 10.0f));
    TEST_ASSERT_TRUE(c.update(11.0f, 10.0f)); // 1,0 hPa: bleibt an
    TEST_ASSERT_TRUE(c.update(10.6f, 10.0f)); // 0,6 hPa: bleibt an
    TEST_ASSERT_FALSE(c.update(10.5f, 10.0f)); // 0,5 hPa: aus
    TEST_ASSERT_FALSE(c.update(11.0f, 10.0f)); // 1,0 hPa: bleibt aus
    TEST_ASSERT_TRUE(c.update(11.5f, 10.0f));  // erst 1,5 hPa schaltet wieder ein
}

static void test_vergleich_sperrt_wenn_aussen_feuchter(void)
{
    HumidityCompare c;
    TEST_ASSERT_TRUE(c.update(15.0f, 10.0f));
    TEST_ASSERT_FALSE(c.update(10.0f, 15.0f)); // draussen feuchter
}

static void test_vergleich_eigene_schwellen(void)
{
    HumidityCompare c;
    c.setThresholds(3.0f, 1.0f);
    TEST_ASSERT_FALSE(c.update(12.0f, 10.0f));
    TEST_ASSERT_TRUE(c.update(13.0f, 10.0f));
    TEST_ASSERT_TRUE(c.update(11.5f, 10.0f));
    TEST_ASSERT_FALSE(c.update(11.0f, 10.0f));
}

static void test_vergleich_verdrehte_schwellen(void)
{
    // Aus-Schwelle ueber der Ein-Schwelle waere keine Hysterese, sondern eine
    // Schwelle mit unklarer Richtung; sie wird zurechtgerueckt.
    HumidityCompare c;
    c.setThresholds(1.0f, 5.0f);
    TEST_ASSERT_TRUE(c.update(11.5f, 10.0f));
    TEST_ASSERT_FALSE(c.update(10.5f, 10.0f));
}

static void test_vergleich_nan_haelt_den_zustand(void)
{
    HumidityCompare c;
    TEST_ASSERT_TRUE(c.update(12.0f, 10.0f));
    TEST_ASSERT_TRUE(c.update(std::nanf(""), 10.0f));
    TEST_ASSERT_TRUE(c.allowed());

    c.reset();
    TEST_ASSERT_FALSE(c.update(12.0f, std::nanf("")));
}

static void test_vergleich_mit_echten_messwerten(void)
{
    // Kellertrocknung im Sommer: innen 18 °C / 75 %, draussen 28 °C / 60 %.
    // Gefuehlt ist draussen "trockener" - tatsaechlich steht dort mehr Wasser in
    // der Luft, und Lueften wuerde den Keller feuchter machen.
    const float inside = MoistAir::vapourPressure(18.0f, 75.0f);
    const float outside = MoistAir::vapourPressure(28.0f, 60.0f);
    HumidityCompare c;
    TEST_ASSERT_FALSE(c.update(inside, outside));

    // Dieselbe Nacht, draussen auf 12 °C abgekuehlt bei 80 %: jetzt lohnt es.
    const float night = MoistAir::vapourPressure(12.0f, 80.0f);
    TEST_ASSERT_TRUE(c.update(inside, night));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_saettigungsdampfdruck);
    RUN_TEST(test_saettigungsdampfdruck_geklemmt);
    RUN_TEST(test_partialdruck_pruefwerte);
    RUN_TEST(test_partialdruck_randwerte);
    RUN_TEST(test_kalt_und_feucht_ist_trockener_als_warm_und_mittel);
    RUN_TEST(test_luftdruck_ueber_hoehe);
    RUN_TEST(test_luftdruck_geklemmt);
    RUN_TEST(test_mischungsverhaeltnis_bei_500m);
    RUN_TEST(test_mischungsverhaeltnis_auf_meereshoehe);
    RUN_TEST(test_mischungsverhaeltnis_steigt_mit_der_hoehe);
    RUN_TEST(test_mischungsverhaeltnis_randfaelle);
    RUN_TEST(test_vergleich_startet_gesperrt);
    RUN_TEST(test_vergleich_schaltet_bei_1_5_hpa_ein);
    RUN_TEST(test_vergleich_haelt_zwischen_den_schwellen);
    RUN_TEST(test_vergleich_sperrt_wenn_aussen_feuchter);
    RUN_TEST(test_vergleich_eigene_schwellen);
    RUN_TEST(test_vergleich_verdrehte_schwellen);
    RUN_TEST(test_vergleich_nan_haelt_den_zustand);
    RUN_TEST(test_vergleich_mit_echten_messwerten);
    return UNITY_END();
}
