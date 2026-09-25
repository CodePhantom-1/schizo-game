// test_scenario_wild.cpp — W4-C scenario (the track brief): over a year of
// drought, raids rise, a camp forms, the player clears it, and it repopulates
// later if the drought persists — and does not if the rains come back.
//
// Method: two worlds from the same seed, one wet (drought 0) and one whose
// drought deepens 1 -> 2 -> 3 -> 3 by quarter, each run a full 360-day year. Raids are counted from the
// event log (EventsState::fire_count_by_rule "wild_raid_*" — the raid log
// itself keeps only the last 64).
#include "sim/Snapshot.hpp"
#include "sim/Test.hpp"
#include "sim/WildActions.hpp"

using namespace sim;

namespace {

constexpr const char* kCanon = "../db/canon";
constexpr std::uint64_t kSeed = 2026;

int raids_so_far(const WorldState& w) {
    int n = 0;
    for (const auto& [id, c] : w.events.fire_count_by_rule)
        if (id.rfind("wild_raid_", 0) == 0) n += c;
    return n;
}

int goods_taken(const WorldState& w) {
    int n = 0;
    for (const RaidRecord& r : w.wild.raids) n += r.goods_taken;
    return n;
}

struct Year {
    int raids = 0;
    int first_quarter = 0;
    int last_quarter = 0;
    int camps_active = 0;
};

// A year whose drought deepens quarter by quarter to `peak` (0 = a wet year).
Year run_year(WorldState& w, int peak) {
    w.init(kCanon, kSeed);
    Year y;
    w.facts.drought_stage = peak > 0 ? 1 : 0;
    w.advance_days(90);
    y.first_quarter = raids_so_far(w);
    w.facts.drought_stage = peak > 0 ? 2 : 0;
    w.advance_days(90);
    w.facts.drought_stage = peak;
    w.advance_days(90);
    const int before_last = raids_so_far(w);
    w.advance_days(90);
    y.raids = raids_so_far(w);
    y.last_quarter = y.raids - before_last;
    for (const auto& [id, g] : w.wild.groups) y.camps_active += g.active;
    return y;
}

// Clears the named camp; the player brings the city's hired spears.
bool clear(WorldState& w, const Id& group) {
    const Group* g = find_group(w.wild, group);
    if (g == nullptr || !g->active) return false;
    const Id camp = g->camp;
    w.wild.player_place = camp;
    for (int k = 0; k < 6; ++k) {
        w.wild.player_wounds = 0;
        if (attack_camp(w, camp, 80, 4).code == 1) return true;
    }
    return false;
}

}  // namespace

static bool test_a_year_of_drought() {
    WorldState wet;
    WorldState dry;
    const Year a = run_year(wet, 0);
    const Year b = run_year(dry, 3);

    // Raids scale with the drought.
    SIM_CHECK(b.raids > a.raids);
    SIM_CHECK(b.raids >= 10);
    // Raids rise through the year as camps form and grow.
    SIM_CHECK(b.last_quarter > b.first_quarter);
    // Camps formed in the dry year that the wet year never saw.
    SIM_CHECK(b.camps_active > a.camps_active);
    SIM_CHECK(find_group(dry.wild, "eastern_warband")->times_formed >= 1);
    SIM_CHECK(!find_group(wet.wild, "eastern_warband")->active);
    SIM_CHECK(find_group(wet.wild, "eastern_warband")->times_formed == 0);
    // The raids took real goods off the market, and the city felt them.
    SIM_CHECK(goods_taken(dry) > 0);
    int formed = 0;
    for (const auto& [id, c] : dry.events.fire_count_by_rule)
        if (id == "wild_camp_formed") formed = c;
    SIM_CHECK(formed >= 3);
    return true;
}

static bool test_cleared_camp_returns_while_the_drought_persists() {
    WorldState w;
    w.init(kCanon, kSeed);
    w.facts.drought_stage = 3;
    w.advance_days(120);
    Group* g = &w.wild.groups["eastern_warband"];
    SIM_CHECK(g->active);
    const int formed = g->times_formed;

    SIM_CHECK(clear(w, "eastern_warband"));
    g = &w.wild.groups["eastern_warband"];
    SIM_CHECK(!g->active);
    SIM_CHECK_EQ(g->times_cleared, 1);
    SIM_CHECK(purse(w.property, "player") > 0);  // the bounty

    // The rains stay away: the band re-forms (a new chief) after repop_days.
    WorldState still_dry = w;  // copy for the control below
    const GroupDef* def = find_group_def(w.wild, "eastern_warband");
    w.advance_days(def->repop_days + def->form_days + 10);
    g = &w.wild.groups["eastern_warband"];
    SIM_CHECK(g->active);
    SIM_CHECK_EQ(g->times_formed, formed + 1);
    SIM_CHECK(g->grudge_vs_player > 0);  // the new chief remembers
    int reoccupied = 0;
    for (const auto& [id, c] : w.events.fire_count_by_rule)
        if (id == "wild_camp_reoccupied") reoccupied += c;
    SIM_CHECK(reoccupied >= 1);

    // Control: the drought breaks the day the camp is cleared — no return.
    still_dry.facts.drought_stage = 0;
    still_dry.advance_days(def->repop_days + def->form_days + 60);
    SIM_CHECK(!find_group(still_dry.wild, "eastern_warband")->active);
    return true;
}

static bool test_the_scenario_survives_a_save() {
    WorldState a;
    a.init(kCanon, kSeed);
    a.facts.drought_stage = 3;
    a.advance_days(200);
    WorldState b;
    load_world(b, kCanon, save_world(a));
    a.advance_days(160);
    b.advance_days(160);
    SIM_CHECK_EQ(raids_so_far(a), raids_so_far(b));
    SIM_CHECK(wild_save_rows(a.wild) == wild_save_rows(b.wild));
    return true;
}

SIM_MAIN(test_a_year_of_drought, test_cleared_camp_returns_while_the_drought_persists,
         test_the_scenario_survives_a_save)
