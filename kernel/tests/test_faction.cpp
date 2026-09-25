// test_faction.cpp — standing clamp, tiers, oaths, the one-Great-King rule,
// the oath-breaker curse, and the tick's invariant maintenance. Every check
// tests behavior documented in sim/Faction.hpp (or its invariants), with one
// deliberate exception: test_oath_gate_enforced_superset_open_wall pins the
// implementation's enforced superset so the documented-vs-enforced oath-gate
// discrepancy stays visible until the coordinator reconciles it (open wall).
#include "sim/Context.hpp"  // the seam: builds a real WorldContext (const refs)

#include "sim/Test.hpp"
#include <climits>

#include <string>
#include <utility>

using namespace sim;

namespace {

// A real world scaffold: canon db, deterministic rng, and all eight module
// states alive so the WorldContext's const refs stay valid.
struct World {
    Db db;
    Rng rng;
    Calendar cal{};
    WorldFacts facts{};
    EconomyState economy{};
    PopulationState population{};
    FactionState faction{};
    MagicState magic{};
    JusticeState justice{};
    EventsState events{};
    PropertyState property{};
    QuestState quests{};
    NeedsState needs{};
    std::map<Id, Inventory> inventories{};

    World(std::uint64_t seed, Db loaded) : db(std::move(loaded)), rng(seed) {}

    WorldContext ctx(DayNumber day) {
        return WorldContext{db,       rng,       day,    cal,   facts,  economy,
                            population, faction, magic, justice, events, property,
                            quests, needs, inventories};
    }
};

Db real_canon() {
    // ctest runs from kernel/ (CMake WORKING_DIRECTORY); also accept the repo
    // root so the binary is runnable from anywhere. Same canon either way.
    if (Db db = Db::load("../db/canon"); !db.rows("factions").empty()) return db;
    return Db::load("db/canon");
}

// Deterministic byte encoding of FactionState (map is Id-sorted, oaths in
// insertion order) — the module's DoD is "same seed -> same state bytes".
std::string state_bytes(const FactionState& s) {
    std::string out;
    for (const auto& entry : s.standing_by_faction)
        out += entry.first + '=' + std::to_string(entry.second) + ';';
    out += '|';
    for (const Oath& o : s.oaths)
        out += o.id + '>' + o.swearer + '@' + o.to_faction + ':' +
               std::to_string(o.day) + (o.broken ? '!' : '.');
    out += '|';
    out += s.oath_breaker_curse ? '1' : '0';
    return out;
}

// One scripted play sequence against a fresh state.
void play_script(FactionState& s, DayNumber day) {
    add_standing(s, "the_empire", 70);
    add_standing(s, "the_barbarians", 30);
    add_standing(s, "sky_cult", 95);
    add_standing(s, "dead_cult", -500);  // clamps at 0
    swear(s, "player", "the_empire", day);                    // oath_1
    swear(s, "player", "the_barbarians", day);                // refused
    swear(s, "npc_sun_priest", "sky_cult", day + 3);          // oath_2
    break_oath(s, "oath_1");
    break_oath(s, "no_such_oath");  // graceful
    break_oath(s, "oath_1");        // double break: no second drop
}

}  // namespace

static bool test_unknown_faction_stands_at_zero() {
    FactionState s;
    SIM_CHECK_EQ(standing(s, "the_empire"), 0);
    SIM_CHECK_EQ(standing(s, "never_heard_of_it"), 0);
    SIM_CHECK(s.standing_by_faction.empty());  // a read creates no entry
    SIM_CHECK(!s.oath_breaker_curse);
    return true;
}

static bool test_add_standing_clamps_to_0_100() {
    FactionState s;
    add_standing(s, "the_empire", 30);
    SIM_CHECK_EQ(standing(s, "the_empire"), 30);
    add_standing(s, "the_empire", 100);
    SIM_CHECK_EQ(standing(s, "the_empire"), 100);  // 130 clamped to 100
    add_standing(s, "the_empire", -10);
    SIM_CHECK_EQ(standing(s, "the_empire"), 90);
    add_standing(s, "the_empire", -500);
    SIM_CHECK_EQ(standing(s, "the_empire"), 0);  // clamped, never negative
    // Factions are independent books.
    add_standing(s, "the_barbarians", 55);
    SIM_CHECK_EQ(standing(s, "the_barbarians"), 55);
    SIM_CHECK_EQ(standing(s, "the_empire"), 0);
    // A write on an unknown faction starts it at the documented default 0.
    add_standing(s, "sky_cult", -40);
    SIM_CHECK_EQ(standing(s, "sky_cult"), 0);
    return true;
}

static bool test_tier_boundaries() {
    // rpg-systems §4.2 exactly: 0-19, 20-49, 50-74, 75-89, 90-100.
    SIM_CHECK_EQ(tier_of(0), "Stranger");
    SIM_CHECK_EQ(tier_of(19), "Stranger");
    SIM_CHECK_EQ(tier_of(20), "Known");
    SIM_CHECK_EQ(tier_of(49), "Known");
    SIM_CHECK_EQ(tier_of(50), "Trusted");
    SIM_CHECK_EQ(tier_of(74), "Trusted");
    SIM_CHECK_EQ(tier_of(75), "Sworn");
    SIM_CHECK_EQ(tier_of(89), "Sworn");
    SIM_CHECK_EQ(tier_of(90), "Oath-bound");
    SIM_CHECK_EQ(tier_of(100), "Oath-bound");
    return true;
}

static bool test_swear_records_sequenced_oath() {
    FactionState s;
    Oath* a = swear(s, "player", "the_empire", 10);
    SIM_CHECK(a != nullptr);
    SIM_CHECK_EQ(a->id, "oath_1");
    SIM_CHECK_EQ(a->swearer, "player");
    SIM_CHECK_EQ(a->to_faction, "the_empire");
    SIM_CHECK_EQ(a->day, DayNumber{10});
    SIM_CHECK(!a->broken);
    SIM_CHECK(std::size_t{1} == s.oaths.size());

    Oath* b = swear(s, "npc_sun_priest", "sky_cult", 12);
    SIM_CHECK(b != nullptr);
    SIM_CHECK_EQ(b->id, "oath_2");  // "oath_<n>" in sequence
    SIM_CHECK_EQ(b->day, DayNumber{12});
    return true;
}

static bool test_one_great_king_rule() {
    FactionState s;
    SIM_CHECK(swear(s, "player", "the_empire", 10) != nullptr);
    // The swearer already holds an unbroken oath to a MAJOR faction (canon:
    // the_empire kind=major, asserted in the open-wall pin below): refused
    // under BOTH the documented rule (Faction.hpp:46-48) and the enforced
    // superset. Nothing recorded.
    SIM_CHECK(swear(s, "player", "the_barbarians", 11) == nullptr);
    SIM_CHECK_EQ(std::size_t{1}, s.oaths.size());
    // The rule is per swearer: others may still swear.
    SIM_CHECK(swear(s, "npc_sun_priest", "the_barbarians", 12) != nullptr);
    // After breaking, the same swearer may swear again; the sequence continues.
    break_oath(s, "oath_1");
    Oath* again = swear(s, "player", "southern_city_states_alliance", 15);
    SIM_CHECK(again != nullptr);
    SIM_CHECK_EQ(again->id, "oath_3");
    return true;
}

static bool test_oath_gate_enforced_superset_open_wall() {
    // OPEN WALL, deliberately pinned (DECISIONS.md D-010: open walls escalate,
    // never get filled). Faction.hpp:46-48 documents the gate as "refuses
    // (returns nullptr) if the swearer already holds an unbroken oath to a
    // MAJOR faction", but the frozen swear() signature takes no WorldContext,
    // so the factions.csv `kind` column cannot be consulted, and hardcoding
    // the major set would be canon invention. The implementation therefore
    // enforces the superset — ANY unbroken oath blocks the next one. The canon
    // facts below (read strictly through sim::Db, the only canon door) are the
    // exact case the documented rule would ACCEPT and the superset refuses.
    // Pinning it here makes the reconciliation — amend the header's oath-gate
    // text to the enforced superset, or extend the API with a db-`kind` seam;
    // a coordinator decision — a deliberate, visible change instead of silent
    // drift. Canon rows are hard-asserted like in
    // test_canon_faction_oath_lifecycle: if the row or its kind regresses to
    // OPEN/absent, that must surface loudly, never be papered over.
    const Db db = real_canon();
    const auto minor_faction = db.find("factions", "sky_cult");
    const auto major_faction = db.find("factions", "the_empire");
    SIM_CHECK(minor_faction.has_value());
    SIM_CHECK(major_faction.has_value());
    SIM_CHECK_EQ(minor_faction->get("kind"), "minor");
    SIM_CHECK_EQ(major_faction->get("kind"), "major");

    // The repro case: hold ONLY an oath to a minor faction, then swear to a
    // major one. Documented rule: accepted. Enforced superset: refused.
    FactionState s;
    Oath* held = swear(s, "player", minor_faction->at("id"), 1);
    SIM_CHECK(held != nullptr);
    SIM_CHECK_EQ(held->id, "oath_1");
    SIM_CHECK_EQ(held->to_faction, "sky_cult");
    Oath* blocked = swear(s, "player", major_faction->at("id"), 2);
    SIM_CHECK(blocked == nullptr);  // the open wall: superset, not the doc rule
    SIM_CHECK_EQ(std::size_t{1}, s.oaths.size());  // nothing extra recorded
    SIM_CHECK(!s.oath_breaker_curse);  // a refusal never touches the curse
    return true;
}

static bool test_break_oath_drops_standing_and_sets_curse() {
    FactionState s;
    add_standing(s, "the_empire", 80);
    Oath* o = swear(s, "player", "the_empire", 20);
    SIM_CHECK(o != nullptr);
    break_oath(s, o->id);
    SIM_CHECK(o->broken);
    SIM_CHECK_EQ(standing(s, "the_empire"), 40);  // 80 - 40
    SIM_CHECK(s.oath_breaker_curse);              // the curse flag is set
    return true;
}

static bool test_break_oath_clamps_at_zero() {
    FactionState s;
    add_standing(s, "the_barbarians", 20);
    Oath* o = swear(s, "player", "the_barbarians", 5);
    SIM_CHECK(o != nullptr);
    break_oath(s, o->id);
    SIM_CHECK_EQ(standing(s, "the_barbarians"), 0);  // clamped, never negative
    return true;
}

static bool test_break_unknown_or_already_broken_oath_is_graceful() {
    FactionState s;
    add_standing(s, "the_empire", 80);
    SIM_CHECK(swear(s, "player", "the_empire", 1) != nullptr);
    break_oath(s, "no_such_oath");  // unknown id: nothing happens
    SIM_CHECK(!s.oath_breaker_curse);
    SIM_CHECK_EQ(standing(s, "the_empire"), 80);
    SIM_CHECK(!s.oaths[0].broken);
    break_oath(s, "oath_1");
    SIM_CHECK_EQ(standing(s, "the_empire"), 40);  // one drop of 40
    break_oath(s, "oath_1");                      // double break: no second drop
    SIM_CHECK_EQ(standing(s, "the_empire"), 40);
    SIM_CHECK(s.oath_breaker_curse);
    return true;
}

static bool test_tick_maintains_standing_invariant() {
    World w{12345, real_canon()};
    FactionState& s = w.faction;
    // Anything that bypassed the clamp is brought back into [0, 100].
    s.standing_by_faction["the_empire"] = 150;
    s.standing_by_faction["the_barbarians"] = -5;
    s.standing_by_faction["sky_cult"] = 50;
    Oath* o = swear(s, "player", "the_empire", 30);
    SIM_CHECK(o != nullptr);
    s.oath_breaker_curse = true;

    tick_faction(w.ctx(31), s, 1);
    SIM_CHECK_EQ(standing(s, "the_empire"), 100);
    SIM_CHECK_EQ(standing(s, "the_barbarians"), 0);
    SIM_CHECK_EQ(standing(s, "sky_cult"), 50);
    SIM_CHECK_EQ(std::size_t{1}, s.oaths.size());
    SIM_CHECK_EQ(s.oaths[0].day, DayNumber{30});
    SIM_CHECK(!s.oaths[0].broken);
    SIM_CHECK(s.oath_breaker_curse);  // untouched by the tick

    // Idempotent on already-valid state, and `days` drives nothing.
    const std::string before = state_bytes(s);
    tick_faction(w.ctx(32), s, 1);
    tick_faction(w.ctx(33), s, 7);
    SIM_CHECK(before == state_bytes(s));
    return true;
}

static bool test_tick_on_empty_db_and_state_is_safe() {
    // Canon absence must be handled gracefully: an empty Db, an empty state.
    World w{7, Db{}};
    tick_faction(w.ctx(1), w.faction, 3);
    SIM_CHECK(w.faction.standing_by_faction.empty());
    SIM_CHECK(w.faction.oaths.empty());
    SIM_CHECK(!w.faction.oath_breaker_curse);
    // And on a state with standing only.
    add_standing(w.faction, "the_empire", 60);
    tick_faction(w.ctx(2), w.faction);
    SIM_CHECK_EQ(standing(w.faction, "the_empire"), 60);
    return true;
}

static bool test_state_bytes_deterministic_across_seeds() {
    // The module consumes no randomness, so identical scripts must give
    // identical state bytes — same seed AND different seeds (stronger).
    World w1{12345, real_canon()};
    World w2{12345, real_canon()};
    World w3{987654321, real_canon()};
    play_script(w1.faction, 100);
    play_script(w2.faction, 100);
    play_script(w3.faction, 100);
    const std::string b1 = state_bytes(w1.faction);
    SIM_CHECK(!b1.empty());  // the script actually produced state
    tick_faction(w1.ctx(101), w1.faction, 1);
    tick_faction(w2.ctx(101), w2.faction, 1);
    tick_faction(w3.ctx(101), w3.faction, 1);
    SIM_CHECK(b1 == state_bytes(w1.faction));      // same seed, same bytes
    SIM_CHECK(b1 == state_bytes(w2.faction));      // re-run: identical
    SIM_CHECK(b1 == state_bytes(w3.faction));      // different seed: identical
    // And the script did the documented things (spot check on one world).
    SIM_CHECK_EQ(w1.faction.oaths.size(), std::size_t{2});
    SIM_CHECK(w1.faction.oaths[0].broken);
    SIM_CHECK(!w1.faction.oaths[1].broken);
    SIM_CHECK(w1.faction.oath_breaker_curse);
    return true;
}

static bool test_canon_faction_oath_lifecycle() {
    // Oath targets are factions.csv ids; use the real CANON row for the
    // Empire (wb §4.1) — read strictly through sim::Db, never invented.
    const Db db = real_canon();
    const auto empire = db.find("factions", "the_empire");
    SIM_CHECK(empire.has_value());
    FactionState s;
    add_standing(s, empire->at("id"), 90);
    Oath* o = swear(s, "player", empire->at("id"), 200);
    SIM_CHECK(o != nullptr);
    SIM_CHECK_EQ(tier_of(standing(s, empire->at("id"))), "Oath-bound");
    break_oath(s, o->id);
    SIM_CHECK(o->broken);
    SIM_CHECK_EQ(standing(s, empire->at("id")), 50);  // 90 - 40 -> Trusted
    SIM_CHECK_EQ(tier_of(standing(s, empire->at("id"))), "Trusted");
    SIM_CHECK(s.oath_breaker_curse);
    return true;
}


// Bug-review 2026-09-25: s + delta overflowed int (UB; wrapped to 0).
static bool test_add_standing_extreme_delta_clamps() {
    FactionState s;
    add_standing(s, "the_empire", 50);
    add_standing(s, "the_empire", INT_MAX);
    SIM_CHECK_EQ(standing(s, "the_empire"), 100);
    add_standing(s, "the_empire", INT_MIN);
    SIM_CHECK_EQ(standing(s, "the_empire"), 0);
    return true;
}

SIM_MAIN(test_unknown_faction_stands_at_zero,
         test_add_standing_clamps_to_0_100,
         test_tier_boundaries,
         test_swear_records_sequenced_oath,
         test_one_great_king_rule,
         test_oath_gate_enforced_superset_open_wall,
         test_break_oath_drops_standing_and_sets_curse,
         test_break_oath_clamps_at_zero,
         test_break_unknown_or_already_broken_oath_is_graceful,
         test_tick_maintains_standing_invariant,
         test_tick_on_empty_db_and_state_is_safe,
         test_state_bytes_deterministic_across_seeds,
         test_canon_faction_oath_lifecycle,
         test_add_standing_extreme_delta_clamps)
