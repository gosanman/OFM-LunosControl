// Tests der Temperaturfuehrung.
//
// Tabelle und Vorgaben aus docs/PLAN.md Phase 4b (Arcus 3.10):
//   Freie Kuehlung   Ti > Tsoll + 1 K  und  Ta < Ti - Abstand -> Stufe "Kuehlung", Sommer
//   Waermeerhalt     Ti < Tsoll        und  Ta < Ti - Abstand -> keine Stufe,     WRG
//   Warmluft nutzen  Ti < Tsoll - 1 K  und  Ta > Ti + Abstand -> Stufe "Heizung", Sommer
//   sonst                                                     -> keine Stufe,     Betriebsart
//   Frostschutz 8 °C, Rueckkehr bei +2 K · Hitzeschutz 30 °C mit Ta > Ti
//   Temperaturabstand 3 K · Stufe Kuehlung 3 · Stufe Heizung 2
//
// Das eine Kelvin zwischen "ein bei Tsoll + 1 K" und "aus bei Tsoll" ist die
// Hysterese: in diesem Band haelt die Fuehrung ihren Zustand. Diese Auslegung
// steht in TempControl.h und wird hier festgehalten.

#include <unity.h>

#include <cmath>

#include "TempControl.h"

using namespace Kwl;

namespace
{
    constexpr float kSet = 22.0f; // Solltemperatur in allen Faellen
} // namespace

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- vier Faelle

static void test_freie_kuehlung(void)
{
    // Drinnen 24, Sollwert 22, draussen 18: kuehlen, und zwar mit langem Zyklus -
    // ein kurzer wuerde die Waerme zurueckholen, die gerade hinaus soll.
    TempControl t;
    const TempResult r = t.update(24.0f, 18.0f, kSet);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, r.mode);
    TEST_ASSERT_EQUAL_UINT8(3, r.stageRequest);
    TEST_ASSERT_EQUAL(CycleRule::Summer, r.cycle);
    TEST_ASSERT_FALSE(r.frostProtection);
    TEST_ASSERT_FALSE(r.heatProtection);
}

static void test_waermeerhalt(void)
{
    // Drinnen 20 unter Sollwert, draussen 10: nicht mehr foerdern als noetig, aber
    // kurz takten, damit die Waerme im Haus bleibt. KEINE Stufenanforderung.
    TempControl t;
    const TempResult r = t.update(20.0f, 10.0f, kSet);
    TEST_ASSERT_EQUAL(TempMode::HeatRetention, r.mode);
    TEST_ASSERT_EQUAL_UINT8(0, r.stageRequest);
    TEST_ASSERT_EQUAL(CycleRule::Wrg, r.cycle);
}

static void test_warmluft_nutzen(void)
{
    // Drinnen 18, draussen 25: die warme Luft hereinholen, langer Zyklus.
    TempControl t;
    const TempResult r = t.update(18.0f, 25.0f, kSet);
    TEST_ASSERT_EQUAL(TempMode::UseWarmAir, r.mode);
    TEST_ASSERT_EQUAL_UINT8(2, r.stageRequest);
    TEST_ASSERT_EQUAL(CycleRule::Summer, r.cycle);
}

static void test_sonst_keine_fuehrung(void)
{
    // Innen und aussen liegen 1 K auseinander, der Abstand ist 3 K: die Fuehrung
    // haelt sich heraus und ueberlaesst den Takt der Betriebsart.
    TempControl t;
    const TempResult r = t.update(22.0f, 21.0f, kSet);
    TEST_ASSERT_EQUAL(TempMode::None, r.mode);
    TEST_ASSERT_EQUAL_UINT8(0, r.stageRequest);
    TEST_ASSERT_EQUAL(CycleRule::FromOperatingMode, r.cycle);
}

// ---------------------------------------------------------------- Hysterese

static void test_band_zwischen_kuehlung_und_waermeerhalt(void)
{
    // Das Kelvin zwischen Tsoll und Tsoll + 1 K: hier wird nicht umgeschaltet.
    // Ohne dieses Halten wuerde der Verbund beim Durchfahren des Bandes zwischen
    // Sommer- und WRG-Takt springen.
    TempControl t;
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(24.0f, 18.0f, kSet).mode);

    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(22.5f, 18.0f, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(22.0f, 18.0f, kSet).mode);

    // Erst unter dem Sollwert kippt sie in den Waermeerhalt.
    TEST_ASSERT_EQUAL(TempMode::HeatRetention, t.update(21.9f, 18.0f, kSet).mode);

    // Und auf dem Rueckweg haelt das Band ebenso.
    TEST_ASSERT_EQUAL(TempMode::HeatRetention, t.update(22.5f, 18.0f, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::HeatRetention, t.update(23.0f, 18.0f, kSet).mode);

    // Erst ueber Tsoll + 1 K wieder kuehlen.
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(23.1f, 18.0f, kSet).mode);
}

static void test_band_der_warmluftnutzung(void)
{
    // Ein bei Tsoll - 1 K, aus erst bei Tsoll.
    TempControl t;
    TEST_ASSERT_EQUAL(TempMode::UseWarmAir, t.update(20.0f, 26.0f, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::UseWarmAir, t.update(21.5f, 26.0f, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::None, t.update(22.0f, 26.0f, kSet).mode);

    // Aus dem Band heraus wird nicht wieder eingeschaltet - dafuer muss der Wert
    // erst unter Tsoll - 1 K.
    TEST_ASSERT_EQUAL(TempMode::None, t.update(21.5f, 26.0f, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::UseWarmAir, t.update(20.5f, 26.0f, kSet).mode);
}

static void test_band_wird_verlassen_wenn_die_aussenbedingung_faellt(void)
{
    // Im Band gehalten wird nur, solange draussen weiter kuehler ist. Gleicht sich
    // die Aussentemperatur an, hoert die Fuehrung auf - sie haelt keinen Zustand
    // fest, dessen Voraussetzung weg ist.
    TempControl t;
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(24.0f, 18.0f, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::None, t.update(22.5f, 22.0f, kSet).mode);
}

// ---------------------------------------------------------------- Abstand

static void test_abstand_sperrt_die_fuehrung(void)
{
    // 2 K Unterschied bei 3 K Abstand: kein Eingriff, obwohl es drinnen zu warm ist.
    TempControl t;
    TEST_ASSERT_EQUAL(TempMode::None, t.update(24.0f, 22.0f, kSet).mode);

    // Mit Abstand 0 greift dieselbe Lage sofort.
    TempControl u;
    u.setDistance(0.0f);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, u.update(24.0f, 22.0f, kSet).mode);
}

static void test_grosser_abstand_haelt_sich_heraus(void)
{
    // Ein grosser Abstand soll Konflikte mit der Heizungsregelung vermeiden.
    TempControl t;
    t.setDistance(10.0f);
    TEST_ASSERT_EQUAL(TempMode::None, t.update(24.0f, 18.0f, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(24.0f, 13.0f, kSet).mode);
}

static void test_negativer_abstand_wird_abgefangen(void)
{
    // Ein negativer Abstand wuerde die Bedingung umdrehen: die Fuehrung wuerde
    // eingreifen, WEIL draussen aehnlich warm ist. Er wird auf 0 geklemmt und
    // verhaelt sich damit wie "kein Mindestabstand".
    TempControl t;
    t.setDistance(-5.0f);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(24.0f, 22.0f, kSet).mode);

    // Und gleich warme Luft bleibt folgenlos - das ist der Unterschied zur
    // umgedrehten Bedingung.
    TempControl u;
    u.setDistance(-5.0f);
    TEST_ASSERT_EQUAL(TempMode::None, u.update(24.0f, 24.0f, kSet).mode);
}

// ---------------------------------------------------------------- Frostschutz

static void test_frostschutz_mit_rueckkehr_bei_2K(void)
{
    TempControl t; // Grenzwert 8 °C
    TEST_ASSERT_FALSE(t.update(9.0f, 0.0f, 20.0f).frostProtection);

    TEST_ASSERT_TRUE(t.update(7.9f, 0.0f, 20.0f).frostProtection);
    TEST_ASSERT_TRUE(t.update(8.0f, 0.0f, 20.0f).frostProtection);  // noch nicht
    TEST_ASSERT_TRUE(t.update(9.9f, 0.0f, 20.0f).frostProtection);  // immer noch
    TEST_ASSERT_FALSE(t.update(10.0f, 0.0f, 20.0f).frostProtection); // +2 K: zurueck
}

static void test_frostschutz_schlaegt_die_fuehrung(void)
{
    // Bei 7 °C innen und 0 °C aussen waere es formal "Waermeerhalt". Solange der
    // Frostschutz steht, gibt die Fuehrung nichts aus.
    TempControl t;
    const TempResult r = t.update(7.0f, 0.0f, 20.0f);
    TEST_ASSERT_TRUE(r.frostProtection);
    TEST_ASSERT_EQUAL(TempMode::None, r.mode);
    TEST_ASSERT_EQUAL_UINT8(0, r.stageRequest);
    TEST_ASSERT_EQUAL(CycleRule::FromOperatingMode, r.cycle);

    // Nach der Rueckkehr wird neu bewertet, nicht fortgesetzt.
    TEST_ASSERT_EQUAL(TempMode::HeatRetention, t.update(10.0f, 0.0f, 20.0f).mode);
}

static void test_eigener_frostgrenzwert(void)
{
    TempControl t;
    t.setFrostLimit(16.0f); // oberes Ende des ETS-Bereichs
    TEST_ASSERT_TRUE(t.update(15.0f, 5.0f, 20.0f).frostProtection);
    TEST_ASSERT_TRUE(t.update(17.0f, 5.0f, 20.0f).frostProtection);
    TEST_ASSERT_FALSE(t.update(18.0f, 5.0f, 20.0f).frostProtection);
}

// ---------------------------------------------------------------- Hitzeschutz

static void test_hitzeschutz_nur_wenn_draussen_waermer(void)
{
    TempControl t; // Grenzwert 30 °C
    TEST_ASSERT_TRUE(t.update(31.0f, 33.0f, 24.0f).heatProtection);

    // Gleich warm oder kuehler draussen: lueften hilft, also kein Schutz.
    TEST_ASSERT_FALSE(t.update(31.0f, 31.0f, 24.0f).heatProtection);
    TEST_ASSERT_FALSE(t.update(31.0f, 25.0f, 24.0f).heatProtection);

    // Unter dem Grenzwert kein Schutz, egal wie warm es draussen ist.
    TEST_ASSERT_FALSE(t.update(29.0f, 40.0f, 24.0f).heatProtection);
}

static void test_hitzeschutz_schlaegt_die_fuehrung(void)
{
    TempControl t;
    const TempResult r = t.update(31.0f, 33.0f, 24.0f);
    TEST_ASSERT_TRUE(r.heatProtection);
    TEST_ASSERT_EQUAL(TempMode::None, r.mode);
    TEST_ASSERT_EQUAL_UINT8(0, r.stageRequest);
}

static void test_hitze_ohne_schutz_ist_freie_kuehlung(void)
{
    // Dieselben 31 °C innen, aber draussen 25: genau dann soll gekuehlt werden.
    TempControl t;
    const TempResult r = t.update(31.0f, 25.0f, 24.0f);
    TEST_ASSERT_FALSE(r.heatProtection);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, r.mode);
    TEST_ASSERT_EQUAL_UINT8(3, r.stageRequest);
}

// ---------------------------------------------------------------- Parameter

static void test_eigene_stufen(void)
{
    TempControl t;
    t.setStages(4, 1);
    TEST_ASSERT_EQUAL_UINT8(4, t.update(24.0f, 18.0f, kSet).stageRequest);
    TEST_ASSERT_EQUAL_UINT8(1, t.update(18.0f, 25.0f, kSet).stageRequest);
}

static void test_stufen_werden_geklemmt(void)
{
    TempControl t;
    t.setStages(9, 200);
    TEST_ASSERT_EQUAL_UINT8(kStageMax, t.update(24.0f, 18.0f, kSet).stageRequest);
    TEST_ASSERT_EQUAL_UINT8(kStageMax, t.update(18.0f, 25.0f, kSet).stageRequest);
}

// ---------------------------------------------------------------- Randfaelle

static void test_fehlender_messwert_laesst_alles_stehen(void)
{
    TempControl t;
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(24.0f, 18.0f, kSet).mode);

    const float nan = std::nanf("");
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(nan, 18.0f, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(24.0f, nan, kSet).mode);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.update(24.0f, 18.0f, nan).mode);

    // Ein fehlender Wert loest auch keinen Schutz aus und hebt keinen auf.
    TempControl u;
    TEST_ASSERT_TRUE(u.update(7.0f, 0.0f, 20.0f).frostProtection);
    TEST_ASSERT_TRUE(u.update(nan, 0.0f, 20.0f).frostProtection);
}

static void test_reset(void)
{
    TempControl t;
    TEST_ASSERT_TRUE(t.update(7.0f, 0.0f, 20.0f).frostProtection);
    t.reset();
    const TempResult r = t.result();
    TEST_ASSERT_FALSE(r.frostProtection);
    TEST_ASSERT_FALSE(r.heatProtection);
    TEST_ASSERT_EQUAL(TempMode::None, r.mode);
}

static void test_result_wiederholt_ohne_neue_bewertung(void)
{
    TempControl t;
    t.update(24.0f, 18.0f, kSet);
    TEST_ASSERT_EQUAL(TempMode::FreeCooling, t.result().mode);
    TEST_ASSERT_EQUAL_UINT8(3, t.result().stageRequest);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_freie_kuehlung);
    RUN_TEST(test_waermeerhalt);
    RUN_TEST(test_warmluft_nutzen);
    RUN_TEST(test_sonst_keine_fuehrung);
    RUN_TEST(test_band_zwischen_kuehlung_und_waermeerhalt);
    RUN_TEST(test_band_der_warmluftnutzung);
    RUN_TEST(test_band_wird_verlassen_wenn_die_aussenbedingung_faellt);
    RUN_TEST(test_abstand_sperrt_die_fuehrung);
    RUN_TEST(test_grosser_abstand_haelt_sich_heraus);
    RUN_TEST(test_negativer_abstand_wird_abgefangen);
    RUN_TEST(test_frostschutz_mit_rueckkehr_bei_2K);
    RUN_TEST(test_frostschutz_schlaegt_die_fuehrung);
    RUN_TEST(test_eigener_frostgrenzwert);
    RUN_TEST(test_hitzeschutz_nur_wenn_draussen_waermer);
    RUN_TEST(test_hitzeschutz_schlaegt_die_fuehrung);
    RUN_TEST(test_hitze_ohne_schutz_ist_freie_kuehlung);
    RUN_TEST(test_eigene_stufen);
    RUN_TEST(test_stufen_werden_geklemmt);
    RUN_TEST(test_fehlender_messwert_laesst_alles_stehen);
    RUN_TEST(test_reset);
    RUN_TEST(test_result_wiederholt_ohne_neue_bewertung);
    return UNITY_END();
}
