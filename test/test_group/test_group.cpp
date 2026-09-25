// Tests des Verbunds.
//
// docs/PLAN.md Phase 4c. Geprueft werden die drei Stufenregeln, der Zykluskonflikt,
// die Takt-Zustandsmaschine mit Zykluszeit je Stufe und Totzeit, der Anteil und der
// Richtungskonflikt.
//
// Die drei Abnahmefaelle aus Phase 4a stehen als eigene Tests: Raum 1 Nacht
// (Deckel 1) + Raum 2 CO2 ueber GW3 -> beide Stufe 1; Raum 1 Standby (Deckel 2)
// -> beide Stufe 2; Regel "Raum 2 fuehrt" -> beide Stufe 3.

#include <unity.h>

#include "KwlGroup.h"

using namespace Kwl;

namespace
{
    GroupMember member(uint8_t room, uint8_t stage, uint8_t maxStage, uint8_t phase,
                       CycleRule wish = CycleRule::Wrg, bool demand = false,
                       Direction dir = Direction::Supply, uint8_t share = 100)
    {
        GroupMember m;
        m.roomNo = room;
        m.stage = stage;
        m.maxStage = maxStage;
        m.cycleWish = wish;
        m.directionDemanded = demand;
        m.direction = dir;
        m.phase = phase;
        m.share = share;
        return m;
    }

    /// Verbund mit zwei laufenden Lueftern ohne Richtungsforderung.
    void fill(KwlGroup& g, uint8_t stage1, uint8_t max1, uint8_t stage2, uint8_t max2)
    {
        g.clearMembers();
        g.addMember(member(1, stage1, max1, 0));
        g.addMember(member(2, stage2, max2, 1));
    }
} // namespace

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- Stufenregel

static void test_abnahme_nachtdeckel_zieht_den_verbund_herunter(void)
{
    // Raum 1 Nacht (Deckel 1), Raum 2 CO2 ueber GW3 (Stufe 3, Deckel 2).
    // Der Nachtdeckel im Schlafzimmer darf nicht dadurch fallen, dass nebenan
    // das CO2 steigt.
    KwlGroup g;
    fill(g, 1, 1, 3, 2);
    TEST_ASSERT_EQUAL_UINT8(1, g.update(0).stage);
}

static void test_abnahme_standby_gibt_zwei_frei(void)
{
    // Derselbe Nachbar, aber Raum 1 jetzt Standby (Deckel 2).
    KwlGroup g;
    fill(g, 1, 2, 3, 2);
    TEST_ASSERT_EQUAL_UINT8(2, g.update(0).stage);
}

static void test_abnahme_ein_raum_fuehrt(void)
{
    KwlGroup g;
    g.setStageRule(GroupStageRule::LeadRoom);
    g.setLeadRoom(2);
    fill(g, 1, 1, 3, 2);
    TEST_ASSERT_EQUAL_UINT8(3, g.update(0).stage);
}

static void test_stufenregel_minimum(void)
{
    KwlGroup g;
    g.setStageRule(GroupStageRule::Min);
    fill(g, 1, 4, 3, 4);
    TEST_ASSERT_EQUAL_UINT8(1, g.update(0).stage);
}

static void test_fuehrender_raum_nicht_im_verbund(void)
{
    // Fehlparametrierung: dann gilt die sichere Regel, also das Minimum.
    KwlGroup g;
    g.setStageRule(GroupStageRule::LeadRoom);
    g.setLeadRoom(7);
    fill(g, 1, 4, 3, 4);
    TEST_ASSERT_EQUAL_UINT8(1, g.update(0).stage);
}

static void test_leerer_verbund(void)
{
    KwlGroup g;
    const GroupResult r = g.update(0);
    TEST_ASSERT_EQUAL_UINT8(0, r.stage);
    TEST_ASSERT_FALSE(r.inDeadTime);
}

// ---------------------------------------------------------------- Zykluszeit

static void test_zykluszeit_je_stufe(void)
{
    KwlGroup g;
    g.setCycleTime(1, 90);
    g.setCycleTime(4, 40);
    fill(g, 1, 4, 1, 4);
    TEST_ASSERT_EQUAL_UINT16(90, g.update(0).cycleSeconds);

    fill(g, 4, 4, 4, 4);
    TEST_ASSERT_EQUAL_UINT16(40, g.update(0).cycleSeconds);
}

static void test_kuerzester_zyklus_gewinnt(void)
{
    // Ein langer Zyklus ist die Abwesenheit von WRG - die laesst sich nicht
    // erzwingen, wenn der Nachbar sie braucht.
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(1, 2, 4, 0, CycleRule::Summer));
    g.addMember(member(2, 2, 4, 1, CycleRule::Wrg));
    TEST_ASSERT_EQUAL_UINT16(70, g.update(0).cycleSeconds);
}

static void test_fuehrender_raum_entscheidet_den_zyklus(void)
{
    KwlGroup g;
    g.setCycleConflict(GroupCycleConflict::LeadRoomWins);
    g.setLeadRoom(1);
    g.clearMembers();
    g.addMember(member(1, 2, 4, 0, CycleRule::Summer));
    g.addMember(member(2, 2, 4, 1, CycleRule::Wrg));
    TEST_ASSERT_EQUAL_UINT16(3600, g.update(0).cycleSeconds);
}

static void test_sommer_wechselt_weiter_die_richtung(void)
{
    // Sommerbetrieb ist kein Einrichtungsbetrieb, sondern ein langer Zyklus.
    KwlGroup g;
    g.setSummerCycleTime(3600);
    g.clearMembers();
    g.addMember(member(1, 2, 4, 0, CycleRule::Summer));
    g.addMember(member(2, 2, 4, 1, CycleRule::Summer));

    const GroupResult r = g.update(0);
    TEST_ASSERT_EQUAL_UINT16(3600, r.cycleSeconds);
    TEST_ASSERT_FALSE(r.directionFixed);

    TEST_ASSERT_EQUAL(Direction::Supply, g.update(3599000).phase0Direction);
    TEST_ASSERT_TRUE(g.update(3600000).inDeadTime);
    TEST_ASSERT_EQUAL(Direction::Exhaust, g.update(3602000).phase0Direction);
}

static void test_waermerueckgewinnung_gemeldet(void)
{
    // WRG heisst: es wird gependelt, und zwar kurz. Der Verbund meldet das, damit
    // der Luefter es nicht raten muss.
    KwlGroup g;
    fill(g, 2, 4, 2, 4);
    TEST_ASSERT_TRUE(g.update(0).hrv);

    // Stillstand ist keine Waermerueckgewinnung.
    fill(g, 0, 4, 0, 4);
    TEST_ASSERT_FALSE(g.update(0).hrv);
}

static void test_sommer_ist_keine_waermerueckgewinnung(void)
{
    // Der lange Zyklus wechselt weiter die Richtung, aber der Regenerator ist
    // nach etwa einer Minute gesaettigt - praktisch keine WRG mehr.
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(1, 2, 4, 0, CycleRule::Summer));
    g.addMember(member(2, 2, 4, 1, CycleRule::Summer));
    TEST_ASSERT_FALSE(g.update(0).hrv);

    // Will einer der beiden WRG, gewinnt der kuerzere Zyklus - und damit die WRG.
    g.clearMembers();
    g.addMember(member(1, 2, 4, 0, CycleRule::Summer));
    g.addMember(member(2, 2, 4, 1, CycleRule::Wrg));
    TEST_ASSERT_TRUE(g.update(0).hrv);
}

static void test_feste_richtung_ist_keine_waermerueckgewinnung(void)
{
    // Ohne Wechsel kein Regenerator-Betrieb.
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(1, 4, 4, 0, CycleRule::Wrg, true, Direction::Exhaust));
    g.addMember(member(2, 2, 4, 1));
    TEST_ASSERT_FALSE(g.update(0).hrv);
}

// ---------------------------------------------------------------- Takt

static void test_pendeltakt_mit_totzeit(void)
{
    KwlGroup g; // 70 s Zyklus, 2 s Totzeit
    fill(g, 2, 4, 2, 4);

    GroupResult r = g.update(0);
    TEST_ASSERT_EQUAL(Direction::Supply, r.phase0Direction);
    TEST_ASSERT_FALSE(r.inDeadTime);

    // Kurz vor Ablauf laeuft es noch.
    TEST_ASSERT_FALSE(g.update(69999).inDeadTime);

    // Mit Ablauf beginnt die Totzeit - 5,00 V auf allen Kanaelen.
    TEST_ASSERT_TRUE(g.update(70000).inDeadTime);
    TEST_ASSERT_TRUE(g.update(71999).inDeadTime);

    // Danach die Gegenrichtung, und die Zykluszeit laeuft neu.
    r = g.update(72000);
    TEST_ASSERT_FALSE(r.inDeadTime);
    TEST_ASSERT_EQUAL(Direction::Exhaust, r.phase0Direction);

    TEST_ASSERT_FALSE(g.update(141999).inDeadTime);
    TEST_ASSERT_TRUE(g.update(142000).inDeadTime);
    TEST_ASSERT_EQUAL(Direction::Supply, g.update(144000).phase0Direction);
}

static void test_phase_1_laeuft_immer_gegenlaeufig(void)
{
    // Zwei Luefter eines Verbunds koennen nie in dieselbe Richtung laufen.
    KwlGroup g;
    fill(g, 2, 4, 2, 4);
    const GroupResult r = g.update(0);
    TEST_ASSERT_EQUAL(Direction::Supply, KwlGroup::directionFor(r, 0));
    TEST_ASSERT_EQUAL(Direction::Exhaust, KwlGroup::directionFor(r, 1));
}

static void test_stillstand_haelt_den_takt_an(void)
{
    // Bei Stufe 0 dreht sich nichts, also gibt es nichts zu wechseln. Und der
    // erste Takt nach dem Anlauf muss eine volle Zykluszeit stehen.
    KwlGroup g;
    fill(g, 0, 4, 0, 4);
    TEST_ASSERT_FALSE(g.update(0).inDeadTime);
    TEST_ASSERT_FALSE(g.update(500000).inDeadTime);

    fill(g, 2, 4, 2, 4);
    TEST_ASSERT_FALSE(g.update(500000).inDeadTime);
    TEST_ASSERT_FALSE(g.update(500000 + 69999).inDeadTime);
    TEST_ASSERT_TRUE(g.update(500000 + 70000).inDeadTime);
}

static void test_laengere_totzeit(void)
{
    KwlGroup g;
    g.setDeadTime(10);
    fill(g, 2, 4, 2, 4);
    g.update(0);
    TEST_ASSERT_TRUE(g.update(70000).inDeadTime);
    TEST_ASSERT_TRUE(g.update(79999).inDeadTime);
    TEST_ASSERT_EQUAL(Direction::Exhaust, g.update(80000).phase0Direction);
}

static void test_takt_ueber_den_millis_ueberlauf(void)
{
    KwlGroup g;
    fill(g, 2, 4, 2, 4);
    const uint32_t start = 0xFFFF0000u;
    g.update(start);
    TEST_ASSERT_FALSE(g.update(start + 69999).inDeadTime);
    TEST_ASSERT_TRUE(g.update(start + 70000).inDeadTime);
    TEST_ASSERT_EQUAL(Direction::Exhaust, g.update(start + 72000).phase0Direction);
}

// ---------------------------------------------------------------- Anforderung

static void test_anforderung_haelt_die_richtung_fest(void)
{
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(1, 4, 4, 0, CycleRule::Wrg, true, Direction::Exhaust));
    g.addMember(member(2, 2, 4, 1));

    // Der Wechsel in die geforderte Richtung laeuft ueber die Totzeit.
    TEST_ASSERT_TRUE(g.update(0).inDeadTime);
    GroupResult r = g.update(2000);
    TEST_ASSERT_FALSE(r.inDeadTime);
    TEST_ASSERT_EQUAL(Direction::Exhaust, r.phase0Direction);
    TEST_ASSERT_TRUE(r.directionFixed);
    TEST_ASSERT_FALSE(r.conflict);

    // Und sie bleibt stehen, auch weit ueber die Zykluszeit hinaus.
    r = g.update(600000);
    TEST_ASSERT_FALSE(r.inDeadTime);
    TEST_ASSERT_EQUAL(Direction::Exhaust, r.phase0Direction);

    // Der Partner in Phase 1 stroemt nach - das ist die Zuluftanforderung.
    TEST_ASSERT_EQUAL(Direction::Supply, KwlGroup::directionFor(r, 1));
}

static void test_gleichgerichtete_forderungen_sind_kein_konflikt(void)
{
    // Raum 1 (Phase 0) will Abluft, Raum 2 (Phase 1) will Zuluft: beide meinen
    // dieselbe Verbundrichtung.
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(1, 2, 4, 0, CycleRule::Wrg, true, Direction::Exhaust));
    g.addMember(member(2, 2, 4, 1, CycleRule::Wrg, true, Direction::Supply));

    const GroupResult r = g.update(0);
    TEST_ASSERT_FALSE(r.conflict);
    TEST_ASSERT_EQUAL_UINT8(0, r.conflictRoom);
    TEST_ASSERT_TRUE(r.directionFixed);
}

static void test_richtungskonflikt_hoehere_stufe_fuehrt(void)
{
    // Bad (Raum 2, Phase 1) fordert Abluft mit Stufe 4, Flur (Raum 1, Phase 0)
    // fordert ebenfalls Abluft mit Stufe 2. Beide zugleich geht nicht - was in den
    // einen Raum hinein soll, muss aus dem anderen heraus.
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(1, 2, 4, 0, CycleRule::Wrg, true, Direction::Exhaust));
    g.addMember(member(2, 4, 4, 1, CycleRule::Wrg, true, Direction::Exhaust));

    const GroupResult r = g.update(0);
    TEST_ASSERT_TRUE(r.conflict);
    TEST_ASSERT_EQUAL_UINT8(1, r.conflictRoom); // der Flur ist unterlegen
    TEST_ASSERT_TRUE(r.directionFixed);

    // Das Bad bekommt seine Abluft, der Flur stroemt nach.
    TEST_ASSERT_EQUAL(Direction::Supply, r.phase0Direction);
    TEST_ASSERT_EQUAL(Direction::Exhaust, KwlGroup::directionFor(r, 1));
    TEST_ASSERT_EQUAL(Direction::Supply, KwlGroup::directionFor(r, 0));
}

static void test_richtungskonflikt_bei_gleichstand_entscheidet_die_raumnummer(void)
{
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(3, 2, 4, 1, CycleRule::Wrg, true, Direction::Exhaust));
    g.addMember(member(1, 2, 4, 0, CycleRule::Wrg, true, Direction::Exhaust));

    const GroupResult r = g.update(0);
    TEST_ASSERT_TRUE(r.conflict);
    TEST_ASSERT_EQUAL_UINT8(3, r.conflictRoom); // Raum 1 gewinnt

    // Der Verbund stand auf Zuluft; auch der Weg in eine erzwungene Richtung
    // laeuft ueber die Totzeit.
    TEST_ASSERT_TRUE(r.inDeadTime);
    TEST_ASSERT_EQUAL(Direction::Exhaust, KwlGroup::directionFor(g.update(2000), 0));
}

static void test_konflikt_verschwindet_mit_der_forderung(void)
{
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(1, 2, 4, 0, CycleRule::Wrg, true, Direction::Exhaust));
    g.addMember(member(2, 4, 4, 1, CycleRule::Wrg, true, Direction::Exhaust));
    TEST_ASSERT_TRUE(g.update(0).conflict);

    g.clearMembers();
    g.addMember(member(1, 2, 4, 0));
    g.addMember(member(2, 4, 4, 1, CycleRule::Wrg, true, Direction::Exhaust));
    const GroupResult r = g.update(1000);
    TEST_ASSERT_FALSE(r.conflict);
    TEST_ASSERT_EQUAL_UINT8(0, r.conflictRoom);
}

// ---------------------------------------------------------------- Anteil

static void test_anteil_skaliert_die_gruppenstufe(void)
{
    // Arcus: drei e²60, einer voll, zwei mit halbem Anteil.
    TEST_ASSERT_EQUAL_UINT8(4, KwlGroup::stageForShare(4, 100));
    TEST_ASSERT_EQUAL_UINT8(2, KwlGroup::stageForShare(4, 50));
    TEST_ASSERT_EQUAL_UINT8(2, KwlGroup::stageForShare(3, 50));
    TEST_ASSERT_EQUAL_UINT8(1, KwlGroup::stageForShare(2, 50));
}

static void test_anteil_haelt_keinen_luefter_an(void)
{
    // 25 % von Stufe 1 waere rechnerisch 0 - das wuerde einen laufenden Luefter
    // anhalten und die Balance kippen.
    TEST_ASSERT_EQUAL_UINT8(1, KwlGroup::stageForShare(1, 25));
    TEST_ASSERT_EQUAL_UINT8(1, KwlGroup::stageForShare(1, 50));

    // Bei Stillstand bleibt es aber Stillstand.
    TEST_ASSERT_EQUAL_UINT8(0, KwlGroup::stageForShare(0, 100));
    TEST_ASSERT_EQUAL_UINT8(0, KwlGroup::stageForShare(0, 25));
}

// ---------------------------------------------------------------- Randfaelle

static void test_mitgliederzahl_ist_begrenzt(void)
{
    KwlGroup g;
    for (uint8_t i = 0; i < kGroupMembersMax; i++)
        TEST_ASSERT_TRUE(g.addMember(member(static_cast<uint8_t>(i + 1), 1, 4, i % 2)));
    TEST_ASSERT_FALSE(g.addMember(member(7, 1, 4, 0)));
    TEST_ASSERT_EQUAL_UINT8(kGroupMembersMax, g.memberCount());
}

static void test_stufen_werden_geklemmt(void)
{
    KwlGroup g;
    g.clearMembers();
    g.addMember(member(1, 9, 9, 0));
    g.addMember(member(2, 9, 9, 1));
    TEST_ASSERT_EQUAL_UINT8(kStageMax, g.update(0).stage);
}

static void test_fehlercode_des_konflikts(void)
{
    // Die Reihenfolge der Fehlercodes IST ihre Prioritaet (PLAN Anhang): liegen
    // mehrere an, gewinnt der kleinste. Ein neuer Code gehoert deshalb ans Ende.
    // Dieser Test haelt die Nummerierung fest, damit sie nicht still verrutscht.
    TEST_ASSERT_EQUAL_UINT8(11, static_cast<uint8_t>(ErrorCode::DirectionConflict));
    TEST_ASSERT_EQUAL_UINT8(10, static_cast<uint8_t>(ErrorCode::FilterDue));
    TEST_ASSERT_EQUAL_UINT8(4, static_cast<uint8_t>(ErrorCode::DacUnreachable));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(ErrorCode::NoRelease));

    // Der Richtungskonflikt ist kein Alarm: die Anlage laeuft weiter, sie tut nur
    // nicht, was ein Raum verlangt hat.
    TEST_ASSERT_FALSE(isAlarm(ErrorCode::DirectionConflict));
    TEST_ASSERT_FALSE(isAlarm(ErrorCode::FilterDue));
    TEST_ASSERT_TRUE(isAlarm(ErrorCode::DacUnreachable));
    TEST_ASSERT_TRUE(isAlarm(ErrorCode::MasterTimeout));
}

static void test_reset(void)
{
    KwlGroup g;
    fill(g, 2, 4, 2, 4);
    g.update(0);
    g.reset();
    TEST_ASSERT_EQUAL_UINT8(0, g.memberCount());
    TEST_ASSERT_EQUAL_UINT8(0, g.update(0).stage);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_abnahme_nachtdeckel_zieht_den_verbund_herunter);
    RUN_TEST(test_abnahme_standby_gibt_zwei_frei);
    RUN_TEST(test_abnahme_ein_raum_fuehrt);
    RUN_TEST(test_stufenregel_minimum);
    RUN_TEST(test_fuehrender_raum_nicht_im_verbund);
    RUN_TEST(test_leerer_verbund);
    RUN_TEST(test_zykluszeit_je_stufe);
    RUN_TEST(test_kuerzester_zyklus_gewinnt);
    RUN_TEST(test_fuehrender_raum_entscheidet_den_zyklus);
    RUN_TEST(test_sommer_wechselt_weiter_die_richtung);
    RUN_TEST(test_waermerueckgewinnung_gemeldet);
    RUN_TEST(test_sommer_ist_keine_waermerueckgewinnung);
    RUN_TEST(test_feste_richtung_ist_keine_waermerueckgewinnung);
    RUN_TEST(test_pendeltakt_mit_totzeit);
    RUN_TEST(test_phase_1_laeuft_immer_gegenlaeufig);
    RUN_TEST(test_stillstand_haelt_den_takt_an);
    RUN_TEST(test_laengere_totzeit);
    RUN_TEST(test_takt_ueber_den_millis_ueberlauf);
    RUN_TEST(test_anforderung_haelt_die_richtung_fest);
    RUN_TEST(test_gleichgerichtete_forderungen_sind_kein_konflikt);
    RUN_TEST(test_richtungskonflikt_hoehere_stufe_fuehrt);
    RUN_TEST(test_richtungskonflikt_bei_gleichstand_entscheidet_die_raumnummer);
    RUN_TEST(test_konflikt_verschwindet_mit_der_forderung);
    RUN_TEST(test_anteil_skaliert_die_gruppenstufe);
    RUN_TEST(test_anteil_haelt_keinen_luefter_an);
    RUN_TEST(test_mitgliederzahl_ist_begrenzt);
    RUN_TEST(test_stufen_werden_geklemmt);
    RUN_TEST(test_fehlercode_des_konflikts);
    RUN_TEST(test_reset);
    return UNITY_END();
}
