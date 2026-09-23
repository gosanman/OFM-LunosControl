// Tests der Vorfahrt - der wichtigste Testtag des Plans.
//
// Jede Zeile der Rangtabelle aus docs/PLAN.md Phase 4 ist ein Testfall, dazu die
// Ruecksetz-Semantik nach Arcus 3.14 und beide dort genannten Bedienbeispiele.
//
//   1 Sperre · 2 Schutz · 3 Zu-/Abluftanforderung · 4 Verbund · 5 Handstufe ·
//   6 Automatikstufe · 7 Zwangsobjekte · 8 Nacht · 9 Zwangsbetriebsart ·
//   10 Betriebsart · 11 Standard
//
// Vorgaben: Standard Standby {Grund 1, Max 2} · Eco {1,1} · Komfort {1,4} ·
// Stosslueften {4,4} · Ruhe {0,0} · Zwangsobjekt 1 Stosslueften 30 min,
// 2 Ruhe unendlich, 3 Komfort 30 min.

#include <unity.h>

#include "StageArbiter.h"

using namespace Kwl;

namespace
{
    constexpr uint32_t kMinute = 60000u;
} // namespace

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- Rang 6 zuerst

static void test_grundstufe_laeuft_immer(void)
{
    // Ohne jede Anforderung laeuft die Grundstufe der Standard-Betriebsart.
    StageArbiter a;
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL_UINT8(1, r.stage);
    TEST_ASSERT_EQUAL(StageSource::Automatic, r.source);
    TEST_ASSERT_EQUAL(OperatingMode::Standby, r.mode);
    TEST_ASSERT_EQUAL_UINT8(11, r.modeRank);
}

static void test_fuehrung_hebt_an_maximalstufe_deckelt(void)
{
    StageArbiter a;
    a.setGuidanceStage(2);
    TEST_ASSERT_EQUAL_UINT8(2, a.update(0).stage);

    // Standby deckelt bei 2 - die Fuehrung kommt nicht darueber.
    a.setGuidanceStage(4);
    TEST_ASSERT_EQUAL_UINT8(2, a.update(0).stage);

    // In Komfort (Deckel 4) dieselbe Fuehrung voll.
    a.setMode(OperatingMode::Comfort, 0);
    TEST_ASSERT_EQUAL_UINT8(4, a.update(0).stage);
}

static void test_nachtdeckel_ist_der_zweck_des_projekts(void)
{
    // Eco deckelt bei Stufe 1. Eine CO2-Treppe auf Stufe 4 darf im Schlafzimmer
    // nicht durchschlagen.
    StageArbiter a;
    a.setGuidanceStage(4);
    a.setNight(true, 0);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL(OperatingMode::Eco, r.mode);
    TEST_ASSERT_EQUAL_UINT8(1, r.stage);
}

static void test_grundstufe_ueber_maximalstufe_wird_gedeckelt(void)
{
    // Fehlparametrierung: Grundstufe 4 bei Deckel 2. Gedeckelt, nicht durchgelassen.
    StageArbiter a;
    a.setModeParams(OperatingMode::Standby, {4, 2});
    TEST_ASSERT_EQUAL_UINT8(2, a.update(0).stage);
}

// ---------------------------------------------------------------- Raenge 1 bis 5

static void test_rang5_hand_ersetzt_automatik(void)
{
    StageArbiter a;
    a.setGuidanceStage(2);
    a.setManualStage(0, 0);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL_UINT8(0, r.stage);
    TEST_ASSERT_EQUAL(StageSource::Manual, r.source);
}

static void test_hand_wird_nicht_von_der_maximalstufe_gedeckelt(void)
{
    // Auslegung, siehe StageArbiter.h: der Deckel schuetzt vor der Automatik,
    // nicht vor dem Bewohner. Wer nachts bewusst Stufe 3 drueckt, bekommt sie.
    StageArbiter a;
    a.setNight(true, 0); // Eco, Deckel 1
    a.setManualStage(3, 0);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL_UINT8(3, r.stage);
    TEST_ASSERT_EQUAL(StageSource::Manual, r.source);
    TEST_ASSERT_EQUAL(OperatingMode::Eco, r.mode); // die Betriebsart bleibt Nacht
}

static void test_rang4_verbund_schlaegt_hand(void)
{
    StageArbiter a;
    a.setManualStage(1, 0);
    a.setGroupStage(true, 3, Direction::Exhaust);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL_UINT8(3, r.stage);
    TEST_ASSERT_EQUAL(StageSource::Group, r.source);
    TEST_ASSERT_TRUE(r.directionForced);
    TEST_ASSERT_EQUAL(Direction::Exhaust, r.direction);
}

static void test_rang3_anforderung_schlaegt_verbund_und_erzwingt_richtung(void)
{
    StageArbiter a;
    a.setManualStage(1, 0);
    a.setGroupStage(true, 3, Direction::Exhaust);
    a.setAirDemand(true, Direction::Supply, 2);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL_UINT8(2, r.stage);
    TEST_ASSERT_EQUAL(StageSource::AirDemand, r.source);
    TEST_ASSERT_TRUE(r.directionForced);
    TEST_ASSERT_EQUAL(Direction::Supply, r.direction);
}

static void test_rang2_schutz_schlaegt_alles_darunter(void)
{
    // Abweichung von Arcus: ein durchfrierender Raum ist kein Bedienfall.
    StageArbiter a;
    a.setManualStage(4, 0);
    a.setGroupStage(true, 3, Direction::Exhaust);
    a.setAirDemand(true, Direction::Supply, 2);
    a.setProtection(true);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL_UINT8(0, r.stage); // Schutzstufe, Vorgabe 0
    TEST_ASSERT_EQUAL(StageSource::Protection, r.source);
    TEST_ASSERT_FALSE(r.directionForced);
}

static void test_schutzstufe_ist_parametrierbar(void)
{
    StageArbiter a;
    a.setProtectionStage(1);
    a.setProtection(true);
    TEST_ASSERT_EQUAL_UINT8(1, a.update(0).stage);
}

static void test_rang1_sperre_gewinnt_gegen_alles(void)
{
    StageArbiter a;
    a.setManualStage(4, 0);
    a.setGroupStage(true, 4, Direction::Exhaust);
    a.setAirDemand(true, Direction::Supply, 4);
    a.setProtection(true);
    a.setProtectionStage(4);
    a.setLock(true);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL_UINT8(0, r.stage);
    TEST_ASSERT_EQUAL(StageSource::Lock, r.source);
    TEST_ASSERT_FALSE(r.directionForced);
}

static void test_sperre_faellt_weg_und_die_ebene_darunter_uebernimmt(void)
{
    StageArbiter a;
    a.setGuidanceStage(2);
    a.setLock(true);
    TEST_ASSERT_EQUAL_UINT8(0, a.update(0).stage);
    a.setLock(false);
    TEST_ASSERT_EQUAL_UINT8(2, a.update(0).stage);
}

// ---------------------------------------------------------------- Betriebsarten

static void test_rangfolge_der_betriebsartebenen(void)
{
    StageArbiter a;

    // 11: Standard
    TEST_ASSERT_EQUAL_UINT8(11, a.update(0).modeRank);

    // 10: Betriebsart
    a.setMode(OperatingMode::Comfort, 0);
    TEST_ASSERT_EQUAL_UINT8(10, a.update(0).modeRank);
    TEST_ASSERT_EQUAL(OperatingMode::Comfort, a.result().mode);

    // 9: Zwangsbetriebsart
    a.setForcedMode(OperatingMode::Reduction, 0);
    TEST_ASSERT_EQUAL_UINT8(9, a.update(0).modeRank);
    TEST_ASSERT_EQUAL(OperatingMode::Reduction, a.result().mode);

    // 8: Nacht
    a.setNight(true, 0);
    TEST_ASSERT_EQUAL_UINT8(8, a.update(0).modeRank);
    TEST_ASSERT_EQUAL(OperatingMode::Eco, a.result().mode);

    // 7: Zwangsobjekt
    a.setForcedObjectState(1, true, 0);
    TEST_ASSERT_EQUAL_UINT8(7, a.update(0).modeRank);
    TEST_ASSERT_EQUAL(OperatingMode::Boost, a.result().mode);
    TEST_ASSERT_EQUAL_UINT8(4, a.result().stage); // Stosslueften {4,4}
}

static void test_auto_faellt_auf_die_standard_betriebsart(void)
{
    StageArbiter a;
    a.setStandardMode(OperatingMode::Comfort);
    a.setMode(OperatingMode::Quiet, 0);
    TEST_ASSERT_EQUAL_UINT8(0, a.update(0).stage); // Ruhe {0,0}

    a.setMode(OperatingMode::Auto, 0);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL(OperatingMode::Comfort, r.mode);
    TEST_ASSERT_EQUAL_UINT8(11, r.modeRank);
}

// ---------------------------------------------------------------- Arcus-Beispiele

static void test_arcus_beispiel_1_auto_nacht_zwangsobjekt(void)
{
    // "Auto -> Nacht -> Zwangsobjekt 1: Zwangsobjekt 1 aktiv; nach Ablauf wieder
    // Nacht." Das Zwangsobjekt steht ueber der Nacht, loescht sie aber nicht.
    StageArbiter a;
    a.setNight(true, 0);
    TEST_ASSERT_EQUAL(OperatingMode::Eco, a.update(0).mode);

    a.setForcedObjectState(1, true, 1000);
    TEST_ASSERT_EQUAL(OperatingMode::Boost, a.update(1000).mode);

    // Laufzeit 30 min: eine Millisekunde vorher noch Stosslueften.
    TEST_ASSERT_EQUAL(OperatingMode::Boost, a.update(1000 + 30 * kMinute - 1).mode);

    // Nach Ablauf faellt er auf die naechste noch aktive Ebene - die Nacht.
    const StageResult r = a.update(1000 + 30 * kMinute);
    TEST_ASSERT_EQUAL(OperatingMode::Eco, r.mode);
    TEST_ASSERT_EQUAL_UINT8(8, r.modeRank);
}

static void test_arcus_beispiel_2_auto_zwangsobjekt_nacht(void)
{
    // "Auto -> Zwangsobjekt 1 -> Nacht: Nacht loescht Zwangsobjekt 1; nach
    // Ruecknahme Auto." Die Nacht liegt tiefer im Rang, ihre Bedienung setzt aber
    // die hoeheren Ebenen zurueck - sonst bliebe die Taste wirkungslos.
    StageArbiter a;
    a.setForcedObjectState(1, true, 0);
    TEST_ASSERT_EQUAL(OperatingMode::Boost, a.update(0).mode);

    a.setNight(true, 1000);
    const StageResult r = a.update(1000);
    TEST_ASSERT_EQUAL(OperatingMode::Eco, r.mode);
    TEST_ASSERT_EQUAL_UINT8(8, r.modeRank);

    // Nach Ruecknahme der Nacht bleibt nichts uebrig: zurueck auf Auto/Standard.
    a.setNight(false, 2000);
    const StageResult s = a.update(2000);
    TEST_ASSERT_EQUAL(OperatingMode::Standby, s.mode);
    TEST_ASSERT_EQUAL_UINT8(11, s.modeRank);
}

// ---------------------------------------------------------------- Ruecksetzen

static void test_bedienung_setzt_hoehere_ebenen_zurueck(void)
{
    // Rang 10 aendert sich: alles von 5 bis 9 faellt.
    StageArbiter a;
    a.setManualStage(4, 0);
    a.setForcedObjectState(2, true, 0);
    a.setNight(true, 0);
    a.setForcedMode(OperatingMode::Boost, 0);

    a.setMode(OperatingMode::Comfort, 1000);
    const StageResult r = a.update(1000);
    TEST_ASSERT_EQUAL(OperatingMode::Comfort, r.mode);
    TEST_ASSERT_EQUAL_UINT8(10, r.modeRank);
    TEST_ASSERT_FALSE(a.manualActive());
    TEST_ASSERT_EQUAL(StageSource::Automatic, r.source);
}

static void test_sperre_laesst_sich_nicht_wegbedienen(void)
{
    // Raenge 1 bis 4 sind vom Ruecksetzen ausgenommen.
    StageArbiter a;
    a.setLock(true);
    a.setMode(OperatingMode::Boost, 0);
    a.setManualStage(4, 0);
    a.setNight(true, 0);
    const StageResult r = a.update(0);
    TEST_ASSERT_EQUAL(StageSource::Lock, r.source);
    TEST_ASSERT_EQUAL_UINT8(0, r.stage);
}

static void test_gleicher_wert_setzt_nichts_zurueck(void)
{
    // Eine Wiederholung desselben Objektwerts ist keine Aenderung - sonst wuerde
    // ein zyklisch sendender Schaltaktor den Handbetrieb dauernd loeschen.
    StageArbiter a;
    a.setNight(true, 0);
    a.setManualStage(3, 0);
    TEST_ASSERT_TRUE(a.manualActive());

    a.setNight(true, 1000); // derselbe Wert
    TEST_ASSERT_TRUE(a.manualActive());

    a.setNight(false, 2000); // jetzt eine Aenderung
    TEST_ASSERT_FALSE(a.manualActive());
}

// ---------------------------------------------------------------- Zwangsobjekte

static void test_zwangsobjekte_gleichrangig_letztes_gewinnt(void)
{
    StageArbiter a; // Vorgabe: gleichrangig
    a.setForcedObjectState(1, true, 0); // Stosslueften
    TEST_ASSERT_EQUAL(OperatingMode::Boost, a.update(0).mode);

    a.setForcedObjectState(3, true, 1000); // Komfort
    TEST_ASSERT_EQUAL(OperatingMode::Comfort, a.update(1000).mode);

    // Das zuletzt eingeschaltete faellt weg - das noch anstehende uebernimmt,
    // nicht die naechste Ebene.
    a.setForcedObjectState(3, false, 2000);
    TEST_ASSERT_EQUAL(OperatingMode::Boost, a.update(2000).mode);
}

static void test_zwangsobjekte_hierarchisch(void)
{
    StageArbiter a;
    a.setForcedObjectHierarchical(true);
    a.setForcedObject(2, OperatingMode::Quiet, 0);

    a.setForcedObjectState(3, true, 0);  // Komfort
    a.setForcedObjectState(1, true, 100); // Stosslueften, spaeter, aber niedriger
    TEST_ASSERT_EQUAL(OperatingMode::Comfort, a.update(100).mode);

    a.setForcedObjectState(3, false, 200);
    TEST_ASSERT_EQUAL(OperatingMode::Boost, a.update(200).mode);
}

static void test_zwangsobjekt_ohne_laufzeit_bleibt(void)
{
    // Zwangsobjekt 2 ist per Vorgabe Ruhe ohne Ablauf - fuer den Fensterkontakt.
    StageArbiter a;
    a.setForcedObjectState(2, true, 0);
    TEST_ASSERT_EQUAL(OperatingMode::Quiet, a.update(0).mode);
    TEST_ASSERT_EQUAL(OperatingMode::Quiet, a.update(48u * 60u * kMinute).mode);
    TEST_ASSERT_EQUAL_UINT8(0, a.result().stage);
}

static void test_zwangsobjekt_belegung_parametrierbar(void)
{
    StageArbiter a;
    a.setForcedObject(1, OperatingMode::Quiet, 5 * kMinute);
    a.setForcedObjectState(1, true, 0);
    TEST_ASSERT_EQUAL(OperatingMode::Quiet, a.update(0).mode);
    TEST_ASSERT_EQUAL(OperatingMode::Quiet, a.update(5 * kMinute - 1).mode);
    TEST_ASSERT_EQUAL(OperatingMode::Standby, a.update(5 * kMinute).mode);
}

// ---------------------------------------------------------------- Laufzeiten

static void test_handstufe_laeuft_ab(void)
{
    StageArbiter a; // Vorgabe 60 min
    a.setGuidanceStage(2);
    a.setManualStage(0, 0);
    TEST_ASSERT_EQUAL_UINT8(0, a.update(0).stage);
    TEST_ASSERT_EQUAL_UINT8(0, a.update(60 * kMinute - 1).stage);

    const StageResult r = a.update(60 * kMinute);
    TEST_ASSERT_EQUAL_UINT8(2, r.stage);
    TEST_ASSERT_EQUAL(StageSource::Automatic, r.source);
}

static void test_handstufe_dauerhaft(void)
{
    StageArbiter a;
    a.setManualRuntime(0);
    a.setGuidanceStage(2);
    a.setManualStage(0, 0);
    TEST_ASSERT_EQUAL_UINT8(0, a.update(24u * 60u * kMinute).stage);
}

static void test_handbetrieb_per_null_beenden(void)
{
    // KO "Handbetrieb aktiv" ist Ein- und Ausgang; eine 0 vom Bus beendet sofort.
    StageArbiter a;
    a.setGuidanceStage(2);
    a.setManualStage(4, 0);
    TEST_ASSERT_EQUAL_UINT8(4, a.update(0).stage);

    a.setManualActive(false, 1000);
    TEST_ASSERT_EQUAL_UINT8(2, a.update(1000).stage);
}

static void test_zwangsbetriebsart_laeuft_ab(void)
{
    StageArbiter a;
    a.setForcedModeRuntime(120 * kMinute);
    a.setMode(OperatingMode::Comfort, 0);
    a.setForcedMode(OperatingMode::Quiet, 1000);
    TEST_ASSERT_EQUAL(OperatingMode::Quiet, a.update(1000).mode);

    const StageResult r = a.update(1000 + 120 * kMinute);
    TEST_ASSERT_EQUAL(OperatingMode::Comfort, r.mode);
    TEST_ASSERT_EQUAL_UINT8(10, r.modeRank);
}

static void test_laufzeit_ueber_den_millis_ueberlauf(void)
{
    // millis() laeuft nach 49,7 Tagen ueber. Eine Laufzeit darf dabei weder sofort
    // ablaufen noch haengenbleiben.
    StageArbiter a;
    a.setManualRuntime(10 * kMinute);
    const uint32_t start = 0xFFFFFF00u;
    a.setManualStage(4, start);

    TEST_ASSERT_EQUAL_UINT8(4, a.update(start + 10 * kMinute - 1).stage);
    TEST_ASSERT_EQUAL(StageSource::Automatic,
                      a.update(start + 10 * kMinute).source);
}

// ---------------------------------------------------------------- Taster

static void test_schritt_hoch_und_runter(void)
{
    StageArbiter a;
    a.setGuidanceStage(1); // wirksam ist Stufe 1
    TEST_ASSERT_EQUAL_UINT8(1, a.update(0).stage);

    // Der erste Schritt geht von der sichtbaren Stufe aus, nicht von 0.
    a.stepManual(+1, 0);
    TEST_ASSERT_EQUAL_UINT8(2, a.update(0).stage);
    a.stepManual(+1, 0);
    TEST_ASSERT_EQUAL_UINT8(3, a.update(0).stage);
    a.stepManual(-1, 0);
    TEST_ASSERT_EQUAL_UINT8(2, a.update(0).stage);
}

static void test_schritt_klemmt_an_den_enden(void)
{
    StageArbiter a;
    a.setManualStage(4, 0);
    a.stepManual(+1, 0);
    TEST_ASSERT_EQUAL_UINT8(4, a.update(0).stage);

    a.setManualStage(0, 0);
    a.stepManual(-1, 0);
    TEST_ASSERT_EQUAL_UINT8(0, a.update(0).stage);
}

// ---------------------------------------------------------------- Randfaelle

static void test_stufen_werden_geklemmt(void)
{
    StageArbiter a;
    a.setManualStage(9, 0);
    TEST_ASSERT_EQUAL_UINT8(kStageMax, a.update(0).stage);

    a.setManualActive(false, 0);
    a.setAirDemand(true, Direction::Supply, 200);
    TEST_ASSERT_EQUAL_UINT8(kStageMax, a.update(0).stage);
}

static void test_reset(void)
{
    StageArbiter a;
    a.setLock(true);
    a.setManualStage(4, 0);
    a.setNight(true, 0);
    a.reset();
    const StageResult r = a.result();
    TEST_ASSERT_EQUAL_UINT8(1, r.stage); // Standby-Grundstufe
    TEST_ASSERT_EQUAL(StageSource::Automatic, r.source);
    TEST_ASSERT_EQUAL(OperatingMode::Standby, r.mode);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_grundstufe_laeuft_immer);
    RUN_TEST(test_fuehrung_hebt_an_maximalstufe_deckelt);
    RUN_TEST(test_nachtdeckel_ist_der_zweck_des_projekts);
    RUN_TEST(test_grundstufe_ueber_maximalstufe_wird_gedeckelt);
    RUN_TEST(test_rang5_hand_ersetzt_automatik);
    RUN_TEST(test_hand_wird_nicht_von_der_maximalstufe_gedeckelt);
    RUN_TEST(test_rang4_verbund_schlaegt_hand);
    RUN_TEST(test_rang3_anforderung_schlaegt_verbund_und_erzwingt_richtung);
    RUN_TEST(test_rang2_schutz_schlaegt_alles_darunter);
    RUN_TEST(test_schutzstufe_ist_parametrierbar);
    RUN_TEST(test_rang1_sperre_gewinnt_gegen_alles);
    RUN_TEST(test_sperre_faellt_weg_und_die_ebene_darunter_uebernimmt);
    RUN_TEST(test_rangfolge_der_betriebsartebenen);
    RUN_TEST(test_auto_faellt_auf_die_standard_betriebsart);
    RUN_TEST(test_arcus_beispiel_1_auto_nacht_zwangsobjekt);
    RUN_TEST(test_arcus_beispiel_2_auto_zwangsobjekt_nacht);
    RUN_TEST(test_bedienung_setzt_hoehere_ebenen_zurueck);
    RUN_TEST(test_sperre_laesst_sich_nicht_wegbedienen);
    RUN_TEST(test_gleicher_wert_setzt_nichts_zurueck);
    RUN_TEST(test_zwangsobjekte_gleichrangig_letztes_gewinnt);
    RUN_TEST(test_zwangsobjekte_hierarchisch);
    RUN_TEST(test_zwangsobjekt_ohne_laufzeit_bleibt);
    RUN_TEST(test_zwangsobjekt_belegung_parametrierbar);
    RUN_TEST(test_handstufe_laeuft_ab);
    RUN_TEST(test_handstufe_dauerhaft);
    RUN_TEST(test_handbetrieb_per_null_beenden);
    RUN_TEST(test_zwangsbetriebsart_laeuft_ab);
    RUN_TEST(test_laufzeit_ueber_den_millis_ueberlauf);
    RUN_TEST(test_schritt_hoch_und_runter);
    RUN_TEST(test_schritt_klemmt_an_den_enden);
    RUN_TEST(test_stufen_werden_geklemmt);
    RUN_TEST(test_reset);
    return UNITY_END();
}
