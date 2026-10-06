// Tests des Totzeit-Waechters je Kanal (Befund C, 2026-10-06).
//
// Sicherheitsinvariante 5: zwischen zwei Richtungen liegt immer ein Aufenthalt
// bei 5,00 V von mindestens der Totzeit - egal, warum die Spannung die Seite
// wechselt.

#include <unity.h>

#include "DirectionGuard.h"

using namespace Kwl;

void setUp(void) {}
void tearDown(void) {}

static constexpr uint32_t kDead = 2000;

static void test_seite_einer_spannung(void)
{
    TEST_ASSERT_EQUAL_INT8(-1, DirectionGuard::sideOf(3.21f));
    TEST_ASSERT_EQUAL_INT8(-1, DirectionGuard::sideOf(0.0f));
    TEST_ASSERT_EQUAL_INT8(0, DirectionGuard::sideOf(5.0f));
    TEST_ASSERT_EQUAL_INT8(0, DirectionGuard::sideOf(4.99f));   // Kalibrierfehler
    TEST_ASSERT_EQUAL_INT8(0, DirectionGuard::sideOf(5.013f));
    TEST_ASSERT_EQUAL_INT8(1, DirectionGuard::sideOf(6.91f));
}

static void test_dieselbe_seite_laeuft_durch(void)
{
    DirectionGuard g;
    g.restart(0);
    g.written(-1, kDead);                       // nach der Starttotzeit
    TEST_ASSERT_TRUE(g.allow(-1, kDead + 10, kDead));
    TEST_ASSERT_TRUE(g.allow(0, kDead + 10, kDead));
}

static void test_seitenwechsel_ohne_aufenthalt_wird_gehalten(void)
{
    // Der Fall Abluftstoss des ego: Motor 1 von 6,85 V direkt auf 1,11 V.
    DirectionGuard g;
    g.restart(0);
    g.written(1, kDead);
    TEST_ASSERT_FALSE(g.allow(-1, kDead + 100, kDead));
}

static void test_seitenwechsel_nach_totzeit_erlaubt(void)
{
    DirectionGuard g;
    g.restart(0);
    g.written(1, kDead);
    g.written(0, 10000);                        // 5,00 V ab t = 10 s
    TEST_ASSERT_FALSE(g.allow(-1, 11999, kDead));
    TEST_ASSERT_TRUE(g.allow(-1, 12000, kDead));
}

static void test_aufenthalt_zaehlt_ab_dem_ersten_mal_fuenf_volt(void)
{
    // Wiederholtes Schreiben von 5,00 V verschiebt den Beginn nicht.
    DirectionGuard g;
    g.restart(0);
    g.written(1, kDead);
    g.written(0, 10000);
    g.written(0, 11000);
    TEST_ASSERT_TRUE(g.allow(-1, 12000, kDead));
}

static void test_rueckkehr_auf_dieselbe_seite_braucht_keine_totzeit(void)
{
    // Stufe 0 und wieder Stufe 1 in derselben Richtung ist kein Wechsel.
    DirectionGuard g;
    g.restart(0);
    g.written(-1, kDead);
    g.written(0, 10000);
    TEST_ASSERT_TRUE(g.allow(-1, 10010, kDead));
}

static void test_nach_restart_braucht_jede_seite_die_totzeit(void)
{
    // 12-V-Wiederkehr und Start: vorher kann 0 V = Volllast angelegen haben.
    DirectionGuard g;
    g.restart(0);
    g.written(-1, kDead);
    g.restart(50000);
    TEST_ASSERT_FALSE(g.allow(-1, 50010, kDead));   // auch dieselbe Seite
    TEST_ASSERT_FALSE(g.allow(1, 51999, kDead));
    TEST_ASSERT_TRUE(g.allow(1, 52000, kDead));
    TEST_ASSERT_TRUE(g.allow(-1, 52000, kDead));
}

static void test_ohne_restart_gilt_der_start(void)
{
    // Ein frisch angelegter Waechter verhaelt sich wie nach restart(0).
    DirectionGuard g;
    TEST_ASSERT_FALSE(g.allow(1, 1999, kDead));
    TEST_ASSERT_TRUE(g.allow(1, 2000, kDead));
}

static void test_uhrueberlauf(void)
{
    // millis() laeuft nach 49 Tagen ueber; die Differenz bleibt richtig.
    DirectionGuard g;
    g.restart(0xFFFFFF00u);
    TEST_ASSERT_FALSE(g.allow(1, 0xFFFFFF00u + 100, kDead));
    TEST_ASSERT_TRUE(g.allow(1, 0xFFFFFF00u + kDead, kDead));
}

static void test_totzeit_null(void)
{
    DirectionGuard g;
    g.restart(0);
    g.written(1, 0);
    g.written(0, 100);
    TEST_ASSERT_TRUE(g.allow(-1, 100, 0));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_seite_einer_spannung);
    RUN_TEST(test_dieselbe_seite_laeuft_durch);
    RUN_TEST(test_seitenwechsel_ohne_aufenthalt_wird_gehalten);
    RUN_TEST(test_seitenwechsel_nach_totzeit_erlaubt);
    RUN_TEST(test_aufenthalt_zaehlt_ab_dem_ersten_mal_fuenf_volt);
    RUN_TEST(test_rueckkehr_auf_dieselbe_seite_braucht_keine_totzeit);
    RUN_TEST(test_nach_restart_braucht_jede_seite_die_totzeit);
    RUN_TEST(test_ohne_restart_gilt_der_start);
    RUN_TEST(test_uhrueberlauf);
    RUN_TEST(test_totzeit_null);
    return UNITY_END();
}
