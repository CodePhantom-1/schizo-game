// test_scenario_oaths.cpp — the full oath lifecycle through the world kernel,
// driven only by the public C++ API (WorldState::init + sim/Faction.hpp):
//
//   1. add_standing walks the seating ladder through every tier of
//      rpg-systems §4.2 — Stranger, Known, Trusted, Sworn, Oath-bound —
//      up and down, clamped at both ends.
//   2. The player swears at Oath-bound standing; the one-oath rule refuses a
//      second unbroken oath; breaking the oath sets the curse flag, drops
//      standing by the documented 40, and frees the swearer to swear again.
//   3. The whole lifecycle — the world ticks it is interleaved with included —
//      lands byte-identically on two worlds seeded identically.
//
// Faction ids are read through sim::Db from the canon the world itself loaded
// and their `kind` is hard-asserted, so the refusal below exercises the
// header's documented major-faction gate (Faction.hpp:46-48) — not the
// enforced superset that test_faction.cpp pins as an open wall. The faction
// mechanics consume no randomness (Faction.cpp:8-10), so determinism here is
// the same-script/same-seed identity of pure bookkeeping.
#include "sim/World.hpp"

#include "sim/Test.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

using namespace sim;

namespace {

constexpr std::uint64_t kScenarioSeed = 97;      // single-world scenario runs
constexpr std::uint64_t kDeterminismSeed = 4242; // the seed BOTH worlds reuse

// Deterministic byte encoding of FactionState: the standing map is Id-sorted,
// oaths are in insertion order — same bytes => same state (the module DoD).
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

// The two oath targets, straight from the factions.csv rows the world loaded
// (wb §4.1 and §4.3). The kind assertions make a canon regression fail loudly
// instead of silently changing what the scenario exercises.
bool fetch_canon_majors(const Db& db, Id& major_a, Id& major_b) {
    const auto empire = db.find("factions", "the_empire");
    const auto barbarians = db.find("factions", "the_barbarians");
    SIM_CHECK(empire.has_value());
    SIM_CHECK(barbarians.has_value());
    SIM_CHECK_EQ(empire->at("kind"), std::string("major"));
    SIM_CHECK_EQ(barbarians->at("kind"), std::string("major"));
    major_a = empire->at("id");
    major_b = barbarians->at("id");
    return true;
}

struct ScriptObs {
    bool second_refused = false;  // the one-oath rule refused the second oath
    bool reswear_accepted = false;  // the break freed the swearer
    Id reswear_id;
};

// The scripted lifecycle, run identically on any world. Mutations only — the
// documented outcomes are asserted by assert_documented_outcomes(), so the
// determinism pair can run the same script and check the same things.
ScriptObs run_oath_script(WorldState& w, const Id& major_a, const Id& major_b) {
    ScriptObs obs;

    // Climb to Oath-bound standing, one documented tier band at a time.
    add_standing(w.faction, major_a, 20);  // 20  Known
    add_standing(w.faction, major_a, 30);  // 50  Trusted
    add_standing(w.faction, major_a, 25);  // 75  Sworn
    add_standing(w.faction, major_a, 15);  // 90  Oath-bound

    // The oath is sworn on the world's first morning (init leaves day 1).
    swear(w.faction, "player", major_a, w.day);                // oath_1
    obs.second_refused = swear(w.faction, "player", major_b, w.day) == nullptr;
    swear(w.faction, "npc_sun_priest", major_b, w.day);        // oath_2

    w.advance_days(3);  // three mornings; the faction tick keeps the books

    break_oath(w.faction, "oath_1");  // the betrayal
    break_oath(w.faction, "oath_1");  // double break: one drop, one curse

    // The break freed the player: the oath is sworn again.
    Oath* again = swear(w.faction, "player", major_b, w.day);
    obs.reswear_accepted = again != nullptr;
    if (again) obs.reswear_id = again->id;

    w.advance_days(2);  // two more mornings; curse and books must persist
    return obs;
}

// Every documented outcome of the script, asserted on a world that ran it.
bool assert_documented_outcomes(const WorldState& w, const Id& major_a,
                                const Id& major_b, const ScriptObs& obs) {
    const FactionState& f = w.faction;

    // The standing book: 90 at the swear, 90 - 40 after the break
    // (Faction.hpp:49-50), unchanged by five clamp-only faction ticks.
    SIM_CHECK_EQ(standing(f, major_a), 50);
    SIM_CHECK_EQ(tier_of(standing(f, major_a)), std::string("Trusted"));  // 50-74
    // The drop hit THAT faction only: major_b's book was never even created.
    SIM_CHECK(f.standing_by_faction.find(major_b) == f.standing_by_faction.end());
    SIM_CHECK_EQ(standing(f, major_b), 0);
    SIM_CHECK_EQ(tier_of(standing(f, major_b)), std::string("Stranger"));

    // The curse flag: set by the break, never by a refusal.
    SIM_CHECK(f.oath_breaker_curse);

    // The three oaths, "oath_<n>" in sequence (Faction.hpp:24).
    SIM_CHECK_EQ(f.oaths.size(), std::size_t{3});
    const Oath& first = f.oaths[0];
    SIM_CHECK_EQ(first.id, std::string("oath_1"));
    SIM_CHECK_EQ(first.swearer, std::string("player"));
    SIM_CHECK_EQ(first.to_faction, major_a);
    SIM_CHECK_EQ(first.day, DayNumber{1});
    SIM_CHECK(first.broken);
    const Oath& second = f.oaths[1];
    SIM_CHECK_EQ(second.id, std::string("oath_2"));
    SIM_CHECK_EQ(second.swearer, std::string("npc_sun_priest"));
    SIM_CHECK_EQ(second.to_faction, major_b);
    SIM_CHECK(!second.broken);  // another swearer's oath survives the betrayal
    const Oath& third = f.oaths[2];
    SIM_CHECK_EQ(third.id, std::string("oath_3"));
    SIM_CHECK_EQ(third.swearer, std::string("player"));
    SIM_CHECK_EQ(third.to_faction, major_b);
    SIM_CHECK_EQ(third.day, DayNumber{4});  // the morning after advance_days(3)
    SIM_CHECK(!third.broken);

    // Both decisions of the script went the documented way, on this world.
    SIM_CHECK(obs.second_refused);
    SIM_CHECK(obs.reswear_accepted);
    SIM_CHECK_EQ(obs.reswear_id, std::string("oath_3"));

    // The world moved exactly as scripted, ticks included.
    SIM_CHECK_EQ(w.day, DayNumber{6});
    return true;
}

}  // namespace

// --- 1. the seating ladder ---------------------------------------------------

static bool test_add_standing_walks_every_tier() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    Id major_a, major_b;
    SIM_CHECK(fetch_canon_majors(w.db, major_a, major_b));

    // A fresh world stands at the documented default 0: Stranger, empty books.
    SIM_CHECK_EQ(standing(w.faction, major_a), 0);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Stranger"));
    SIM_CHECK(w.faction.oaths.empty());
    SIM_CHECK(!w.faction.oath_breaker_curse);

    // Up through every band edge of rpg-systems §4.2 (Faction.hpp:42-44),
    // adding exactly the band widths the tiers are documented with.
    add_standing(w.faction, major_a, 19);  // 19: top of Stranger
    SIM_CHECK_EQ(standing(w.faction, major_a), 19);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Stranger"));
    add_standing(w.faction, major_a, 1);   // 20: first Known
    SIM_CHECK_EQ(standing(w.faction, major_a), 20);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Known"));
    add_standing(w.faction, major_a, 29);  // 49: top of Known
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Known"));
    add_standing(w.faction, major_a, 1);   // 50: first Trusted
    SIM_CHECK_EQ(standing(w.faction, major_a), 50);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Trusted"));
    add_standing(w.faction, major_a, 24);  // 74: top of Trusted
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Trusted"));
    add_standing(w.faction, major_a, 1);   // 75: first Sworn
    SIM_CHECK_EQ(standing(w.faction, major_a), 75);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Sworn"));
    add_standing(w.faction, major_a, 14);  // 89: top of Sworn
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Sworn"));
    add_standing(w.faction, major_a, 1);   // 90: first Oath-bound
    SIM_CHECK_EQ(standing(w.faction, major_a), 90);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Oath-bound"));
    add_standing(w.faction, major_a, 10);  // 100: the ceiling itself
    SIM_CHECK_EQ(standing(w.faction, major_a), 100);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Oath-bound"));
    add_standing(w.faction, major_a, 7);   // clamped: never above 100
    SIM_CHECK_EQ(standing(w.faction, major_a), 100);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Oath-bound"));

    // ... and back down every edge, clamped at 0 (Faction.hpp invariant).
    add_standing(w.faction, major_a, -11);  // 89: last Sworn
    SIM_CHECK_EQ(standing(w.faction, major_a), 89);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Sworn"));
    add_standing(w.faction, major_a, -15);  // 74: last Trusted
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Trusted"));
    add_standing(w.faction, major_a, -25);  // 49: last Known
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Known"));
    add_standing(w.faction, major_a, -30);  // 19: last Stranger
    SIM_CHECK_EQ(standing(w.faction, major_a), 19);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Stranger"));
    add_standing(w.faction, major_a, -19);  // 0: the floor itself
    SIM_CHECK_EQ(standing(w.faction, major_a), 0);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Stranger"));
    add_standing(w.faction, major_a, -1);   // clamped: never negative
    SIM_CHECK_EQ(standing(w.faction, major_a), 0);
    return true;
}

// --- 2. the oath lifecycle ---------------------------------------------------

static bool test_oath_lifecycle_one_rule_curse_and_reswear() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    Id major_a, major_b;
    SIM_CHECK(fetch_canon_majors(w.db, major_a, major_b));

    // The climb lands each documented tier on the way to the oath.
    add_standing(w.faction, major_a, 20);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Known"));
    add_standing(w.faction, major_a, 30);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Trusted"));
    add_standing(w.faction, major_a, 25);
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Sworn"));
    add_standing(w.faction, major_a, 15);  // 90: sworn as Oath-bound
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Oath-bound"));
    SIM_CHECK_EQ(standing(w.faction, major_a), 90);

    // Swear: recorded with the documented fields, stamped this morning.
    Oath* oath = swear(w.faction, "player", major_a, w.day);
    SIM_CHECK(oath != nullptr);
    SIM_CHECK_EQ(oath->id, std::string("oath_1"));
    SIM_CHECK_EQ(oath->swearer, std::string("player"));
    SIM_CHECK_EQ(oath->to_faction, major_a);
    SIM_CHECK_EQ(oath->day, DayNumber{1});
    SIM_CHECK(!oath->broken);

    // The one-oath rule: the player already holds an unbroken oath to a MAJOR
    // faction, so a second oath to another MAJOR faction is refused — the
    // documented gate (Faction.hpp:46-48). The refusal records nothing,
    // curses nobody, and reprices nothing.
    SIM_CHECK(swear(w.faction, "player", major_b, w.day) == nullptr);
    SIM_CHECK_EQ(w.faction.oaths.size(), std::size_t{1});
    SIM_CHECK(!w.faction.oath_breaker_curse);
    SIM_CHECK_EQ(standing(w.faction, major_a), 90);
    SIM_CHECK_EQ(standing(w.faction, major_b), 0);

    // The rule binds the swearer, not the world: another NPC may still swear.
    Oath* other = swear(w.faction, "npc_sun_priest", major_b, w.day);
    SIM_CHECK(other != nullptr);
    SIM_CHECK_EQ(other->id, std::string("oath_2"));

    // Break oath_1: marked, standing drops by the documented 40 with THAT
    // faction only, and the Oath-breaker curse sets (Faction.hpp:49-51).
    // (oaths grew since the swear — index by id, never reuse the pointer.)
    break_oath(w.faction, "oath_1");
    SIM_CHECK(w.faction.oaths[0].broken);
    SIM_CHECK(w.faction.oath_breaker_curse);
    SIM_CHECK_EQ(standing(w.faction, major_a), 50);  // 90 - 40
    SIM_CHECK_EQ(tier_of(standing(w.faction, major_a)), std::string("Trusted"));
    SIM_CHECK_EQ(standing(w.faction, major_b), 0);   // the other book untouched

    // A second break of the same oath is not a second betrayal: one drop.
    break_oath(w.faction, "oath_1");
    SIM_CHECK_EQ(standing(w.faction, major_a), 50);

    // The break freed the swearer: the player swears again, sequence continues.
    Oath* again = swear(w.faction, "player", major_b, w.day);
    SIM_CHECK(again != nullptr);
    SIM_CHECK_EQ(again->id, std::string("oath_3"));  // "oath_<n>" in sequence
    SIM_CHECK(!again->broken);

    // The world's own tick preserves the finished lifecycle: the faction tick
    // maintains the standing invariants only (Faction.cpp:97-110).
    w.advance_days(5);
    SIM_CHECK_EQ(standing(w.faction, major_a), 50);
    SIM_CHECK(w.faction.oath_breaker_curse);
    SIM_CHECK_EQ(w.faction.oaths.size(), std::size_t{3});
    SIM_CHECK(w.faction.oaths[0].broken);
    SIM_CHECK(!w.faction.oaths[1].broken);
    SIM_CHECK(!w.faction.oaths[2].broken);
    return true;
}

// --- 3. determinism ----------------------------------------------------------

static bool test_lifecycle_is_deterministic_across_identical_seeds() {
    WorldState a;
    a.init("../db/canon", kDeterminismSeed);
    WorldState b;
    b.init("../db/canon", kDeterminismSeed);  // the SAME seed, a second world
    SIM_CHECK_EQ(a.day, b.day);

    // Both worlds loaded the same canon; the scenario runs against the same
    // two major-faction ids in each.
    Id a_a, a_b, b_a, b_b;
    SIM_CHECK(fetch_canon_majors(a.db, a_a, a_b));
    SIM_CHECK(fetch_canon_majors(b.db, b_a, b_b));
    SIM_CHECK_EQ(a_a, b_a);
    SIM_CHECK_EQ(a_b, b_b);

    const ScriptObs oa = run_oath_script(a, a_a, a_b);
    const ScriptObs ob = run_oath_script(b, b_a, b_b);

    // Both worlds took the same branch at every decision point of the script.
    SIM_CHECK_EQ(oa.second_refused, ob.second_refused);
    SIM_CHECK_EQ(oa.reswear_accepted, ob.reswear_accepted);
    SIM_CHECK_EQ(oa.reswear_id, ob.reswear_id);

    // ... and landed on the same state: same world clock, same faction bytes.
    SIM_CHECK_EQ(a.day, b.day);
    SIM_CHECK(!state_bytes(a.faction).empty());  // the script produced state
    SIM_CHECK_EQ(state_bytes(a.faction), state_bytes(b.faction));

    // Byte equality is not luck: each world independently satisfies every
    // documented outcome of the lifecycle.
    SIM_CHECK(assert_documented_outcomes(a, a_a, a_b, oa));
    SIM_CHECK(assert_documented_outcomes(b, b_a, b_b, ob));
    return true;
}

SIM_MAIN(test_add_standing_walks_every_tier,
         test_oath_lifecycle_one_rule_curse_and_reswear,
         test_lifecycle_is_deterministic_across_identical_seeds)
