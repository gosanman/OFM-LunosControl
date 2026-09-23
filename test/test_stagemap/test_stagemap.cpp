// Tests der Prozent-zu-Stufe-Abbildung.
//
// docs/PLAN.md Anhang: 0 -> 0 · 1…25 -> 1 · 26…50 -> 2 · 51…75 -> 3 · 76…100 -> 4,
// Hysterese +/- 3 %.
//
// Prozent ist Eingabeformat, Stufe ist Wahrheit: diese Uebersetzung findet am Rand
// statt, und der Prozentwert geht nicht weiter nach innen.

#include <unity.h>

#include "StageMap.h"

using namespace Kwl;

void setUp(void) {}
void tearDown(void) {}

static void test_nullpunkt_ohne_hysterese(void)
{
    // 0 % ist kein Randwert, sondern eine Ansage: aus heisst aus, sofort.
    StageMap m;
    m.setStage(4);
    TEST_ASSERT_EQUAL_UINT8(0, m.update(0));

    m.setStage(1);
    TEST_ASSERT_EQUAL_UINT8(0, m.update(0));
}

static void test_jeder_wert_ab_eins_ist_mindestens_stufe_1(void)
{
    // Ein eingeschalteter Luefter darf bei 2 % nicht stillstehen.
    StageMap m;
    TEST_ASSERT_EQUAL_UINT8(1, m.update(1));

    StageMap n;
    TEST_ASSERT_EQUAL_UINT8(1, n.update(2));

    StageMap o;
    TEST_ASSERT_EQUAL_UINT8(1, o.update(25));
}

static void test_aufwaerts_mit_hysterese(void)
{
    // Betreten erst 3 % ueber der Grenze.
    StageMap m;
    TEST_ASSERT_EQUAL_UINT8(1, m.update(26)); // nominale Grenze, noch nicht
    TEST_ASSERT_EQUAL_UINT8(1, m.update(28));
    TEST_ASSERT_EQUAL_UINT8(2, m.update(29));
    TEST_ASSERT_EQUAL_UINT8(2, m.update(53));
    TEST_ASSERT_EQUAL_UINT8(3, m.update(54));
    TEST_ASSERT_EQUAL_UINT8(3, m.update(78));
    TEST_ASSERT_EQUAL_UINT8(4, m.update(79));
    TEST_ASSERT_EQUAL_UINT8(4, m.update(100));
}

static void test_abwaerts_mit_hysterese(void)
{
    // Verlassen erst 3 % unter der Grenze.
    StageMap m;
    TEST_ASSERT_EQUAL_UINT8(4, m.update(100));
    TEST_ASSERT_EQUAL_UINT8(4, m.update(76)); // nominale Grenze, bleibt
    TEST_ASSERT_EQUAL_UINT8(4, m.update(74));
    TEST_ASSERT_EQUAL_UINT8(3, m.update(73));
    TEST_ASSERT_EQUAL_UINT8(3, m.update(49));
    TEST_ASSERT_EQUAL_UINT8(2, m.update(48));
    TEST_ASSERT_EQUAL_UINT8(2, m.update(24));
    TEST_ASSERT_EQUAL_UINT8(1, m.update(23));
    TEST_ASSERT_EQUAL_UINT8(1, m.update(1)); // tiefer geht es nur ueber die Null
}

static void test_zitternder_dimmer_bleibt_folgenlos(void)
{
    // Der Grund fuer die Hysterese: ein Dimmer, der um die Grenze pendelt, darf
    // den Luefter nicht im Takt der Telegramme umschalten.
    StageMap m;
    TEST_ASSERT_EQUAL_UINT8(2, m.update(29));
    for (int i = 0; i < 20; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(2, m.update(26));
        TEST_ASSERT_EQUAL_UINT8(2, m.update(28));
        TEST_ASSERT_EQUAL_UINT8(2, m.update(25));
    }
}

static void test_sprung_ueber_mehrere_stufen(void)
{
    StageMap m;
    TEST_ASSERT_EQUAL_UINT8(4, m.update(100));
    TEST_ASSERT_EQUAL_UINT8(1, m.update(5));
    TEST_ASSERT_EQUAL_UINT8(4, m.update(90));
}

static void test_werte_ueber_100_werden_geklemmt(void)
{
    StageMap m;
    TEST_ASSERT_EQUAL_UINT8(4, m.update(200));
    TEST_ASSERT_EQUAL_UINT8(4, m.update(255));
}

static void test_stufe_setzen_verschiebt_den_bezugspunkt(void)
{
    // Wird die Stufe anderswo gesetzt (Handstufe per DPT 5.100), muss die
    // Hysterese von dort aus weiterrechnen und nicht vom alten Prozentwert.
    StageMap m;
    m.setStage(3);
    TEST_ASSERT_EQUAL_UINT8(3, m.update(60));
    TEST_ASSERT_EQUAL_UINT8(2, m.update(48));

    m.setStage(9); // geklemmt
    TEST_ASSERT_EQUAL_UINT8(kStageMax, m.stage());
}

static void test_rueckmeldung_ist_rundreisefest(void)
{
    // Stufe -> Prozent -> Stufe muss dieselbe Stufe ergeben, sonst wandert eine
    // zurueckgelesene Anzeige.
    for (uint8_t s = 0; s <= kStageMax; s++)
    {
        StageMap m;
        TEST_ASSERT_EQUAL_UINT8(s, m.update(StageMap::stageToPercent(s)));
    }
    TEST_ASSERT_EQUAL_UINT8(0, StageMap::stageToPercent(0));
    TEST_ASSERT_EQUAL_UINT8(88, StageMap::stageToPercent(4));
    TEST_ASSERT_EQUAL_UINT8(88, StageMap::stageToPercent(200));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_nullpunkt_ohne_hysterese);
    RUN_TEST(test_jeder_wert_ab_eins_ist_mindestens_stufe_1);
    RUN_TEST(test_aufwaerts_mit_hysterese);
    RUN_TEST(test_abwaerts_mit_hysterese);
    RUN_TEST(test_zitternder_dimmer_bleibt_folgenlos);
    RUN_TEST(test_sprung_ueber_mehrere_stufen);
    RUN_TEST(test_werte_ueber_100_werden_geklemmt);
    RUN_TEST(test_stufe_setzen_verschiebt_den_bezugspunkt);
    RUN_TEST(test_rueckmeldung_ist_rundreisefest);
    return UNITY_END();
}
