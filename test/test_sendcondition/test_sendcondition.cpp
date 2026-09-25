// Tests der Sendebedingung.
//
// GroupObject::value() markiert ein Objekt IMMER zum Senden. Wer es in loop()
// aufruft, flutet den Bus - bei acht Raeumen mit je sechs Ausgaengen und einem
// Durchlauf je Millisekunde sind das Tausende Telegramme je Sekunde. Diese
// Schicht entscheidet vorher.
//
// Das Totband ist aus der Vorlage OFM-FanControl uebernommen (GPL-3.0) und auf
// 32-Bit-Absolutwerte erweitert.

#include <unity.h>

#include "SendCondition.h"

using namespace Kwl;

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- Totband

static void test_gleicher_wert_geht_nicht_hinaus(void)
{
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(100, 100, 10, 5));
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(0, 0, 0, 0));
}

static void test_absolutes_totband(void)
{
    // 5 Einheiten Mindestabstand.
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(104, 100, 0, 5));
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(105, 100, 0, 5));
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(95, 100, 0, 5));
}

static void test_relatives_totband(void)
{
    // 10 % von 100 sind 10.
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(109, 100, 10, 0));
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(110, 100, 10, 0));

    // 10 % von 1000 sind 100 - dieselbe absolute Aenderung genuegt nicht mehr.
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(1050, 1000, 10, 0));
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(1100, 1000, 10, 0));
}

static void test_der_absolute_sockel_rettet_den_nullpunkt(void)
{
    // Der Grund fuer die Oder-Verknuepfung: bei einem letzten Wert von 0 ist die
    // relative Schwelle immer 0, und ohne Sockel ginge jedes Bit Rauschen hinaus.
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(3, 0, 10, 5));
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(5, 0, 10, 5));
}

static void test_negative_werte(void)
{
    // Der Volumenstrom ist vorzeichenbehaftet: negativ heisst Abluft.
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(-40, 40, 0, 5));
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(-42, -40, 10, 5));
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(-50, -40, 10, 0));
}

static void test_ohne_schwelle_zaehlt_jede_aenderung(void)
{
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(101, 100, 0, 0));
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(100, 100, 0, 0));
}

static void test_grosse_werte_laufen_nicht_ueber(void)
{
    // diff * 100 sprengt bei Rohwerten dieser Groesse einen 32-Bit-Wert; die
    // Rechnung laeuft deshalb in 64 Bit.
    TEST_ASSERT_TRUE(SendCondition::passesDeadband(2000000000, 1000000000, 10, 0));
    TEST_ASSERT_FALSE(SendCondition::passesDeadband(1000000010, 1000000000, 10, 0));
}

// ---------------------------------------------------------------- Zyklus

static void test_zykluszeit(void)
{
    TEST_ASSERT_FALSE(SendCondition::cycleElapsed(1000, 0, 0)); // 0 = aus
    TEST_ASSERT_FALSE(SendCondition::cycleElapsed(999, 0, 1000));
    TEST_ASSERT_TRUE(SendCondition::cycleElapsed(1000, 0, 1000));
}

static void test_zykluszeit_ueber_den_ueberlauf(void)
{
    const uint32_t start = 0xFFFFFF00u;
    TEST_ASSERT_FALSE(SendCondition::cycleElapsed(start + 999, start, 1000));
    TEST_ASSERT_TRUE(SendCondition::cycleElapsed(start + 1000, start, 1000));
}

// ---------------------------------------------------------------- Sent<T>

static void test_sent_erster_wert_geht_immer_hinaus(void)
{
    // Ein Wert, der zufaellig 0 ist, ist nicht dasselbe wie einer, den noch
    // niemand bekommen hat.
    Sent<uint8_t> s;
    TEST_ASSERT_TRUE(s.due(0, 0, 0));
    s.mark(0, 0);
    TEST_ASSERT_FALSE(s.due(0, 0, 0));
}

static void test_sent_aenderung_und_zyklus(void)
{
    Sent<uint8_t> s;
    s.mark(2, 1000);

    TEST_ASSERT_FALSE(s.due(2, 5000, 0));     // unveraendert, kein Zyklus
    TEST_ASSERT_TRUE(s.due(3, 5000, 0));      // geaendert
    TEST_ASSERT_FALSE(s.due(2, 5000, 10000)); // Zyklus noch nicht um
    TEST_ASSERT_TRUE(s.due(2, 11000, 10000)); // Zyklus abgelaufen
}

static void test_sent_mit_bool(void)
{
    Sent<bool> s;
    TEST_ASSERT_TRUE(s.due(false, 0, 0));
    s.mark(false, 0);
    TEST_ASSERT_FALSE(s.due(false, 100, 0));
    TEST_ASSERT_TRUE(s.due(true, 100, 0));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_gleicher_wert_geht_nicht_hinaus);
    RUN_TEST(test_absolutes_totband);
    RUN_TEST(test_relatives_totband);
    RUN_TEST(test_der_absolute_sockel_rettet_den_nullpunkt);
    RUN_TEST(test_negative_werte);
    RUN_TEST(test_ohne_schwelle_zaehlt_jede_aenderung);
    RUN_TEST(test_grosse_werte_laufen_nicht_ueber);
    RUN_TEST(test_zykluszeit);
    RUN_TEST(test_zykluszeit_ueber_den_ueberlauf);
    RUN_TEST(test_sent_erster_wert_geht_immer_hinaus);
    RUN_TEST(test_sent_aenderung_und_zyklus);
    RUN_TEST(test_sent_mit_bool);
    return UNITY_END();
}
