// test_scenario_crime.cpp — scenario hardening: the slice's crime cycle
// (docs/plan.md §7 "guards + one full crime cycle") as far as the kernel
// carries it today: theft -> witness -> rumour -> hearing -> sealed verdict.
//
// The player steals in the City of the Moon, witnessed by the_prophet (the
// only named canon NPC seeded there, db/canon/people.csv). The witness
// records the act in memory (Population::witness); the fact then spreads
// along a `knows` edge on a LATER tick (D-011: no canon social links exist
// yet, so the edge is wired here, exactly as test_world.cpp's rumour test
// and test_population.cpp do); the crime is reported against a real
// db/canon/laws.csv row and its verdict is read from that row, not the
// "compensation" fallback (proven with burglary, whose real first option is
// "death" — a different value than the fallback, so the assertion cannot
// pass by coincidence). A second, unwitnessed theft stays open. Same seed,
// same script -> same JusticeState and PopulationState bytes.
//
// What this does NOT exercise (kernel has no surface for it yet — see
// kernel/contracts/scenario_crime.md): alarm/pursuit/detention, the sealed
// verdict TABLET item, compensation paid from the purse, and any automatic
// effect of a verdict on FactionState standing or rank (D-011; mechanics.md
// row 12 "outlawry -> rank 0"). The test below checks standing explicitly
// stays untouched by hold_hearing, since Justice.hpp forbids writing any
// other module's state.
#include "sim/World.hpp"

#include "sim/Test.hpp"

#include <cstdint>
#include <string>

using namespace sim;

namespace {

const char* kCity = "city_of_the_moon";    // D-005: the slice city
const char* kWitness = "the_prophet";      // the only named NPC seeded there
const char* kListener = "the_warchief";    // wired knows-edge target (proves the hop)
constexpr std::uint64_t kSeed = 925;

std::string justice_bytes(const JusticeState& s) {
    std::string out = "next_id=" + std::to_string(s.next_id);
    for (const Crime& c : s.open_crimes)
        out += "|open:" + c.id + "," + c.criminal + "," + c.law_row + "," +
               std::to_string(c.day) + "," + c.witnessed_by;
    for (const Hearing& h : s.verdicts)
        out += "|verdict:" + h.crime_id + "," + std::to_string(h.day) + "," + h.verdict;
    return out;
}

std::string population_bytes(const PopulationState& s) {
    std::string out;
    for (const Npc& n : s.npcs) {
        out += "npc " + n.id;
        for (const MemoryEntry& m : n.memory)
            out += " mem[" + std::to_string(m.day) + "](" + m.subject + ": " + m.fact + ")";
        out += "\n";
    }
    return out;
}

}  // namespace

// The full witnessed path: theft, real law, verdict visible via state, and
// the rumour crossing one wired hop on a later tick.
static bool test_witnessed_theft_runs_the_cycle_to_a_verdict_from_real_law() {
    WorldState w;
    w.init("../db/canon", kSeed);

    const Npc* witness_npc = find_npc(w.population, kWitness);
    SIM_CHECK(witness_npc != nullptr);
    SIM_CHECK_EQ(witness_npc->home_city, std::string(kCity));  // he is there to see it

    // Confirm the theft law is real, shipped canon — the verdict below must
    // come from this row, not the "no usable row" fallback.
    SIM_CHECK(w.db.has("laws", "theft"));
    const Row theft_row = *w.db.find("laws", "theft");
    SIM_CHECK(theft_row.get("tag") == "A" || theft_row.get("tag") == "CANON");

    // The player commits theft in the City of the Moon; the_prophet sees it.
    // (No "commit crime" verb exists in the kernel yet — see the contract
    // note; the scenario drives Population::witness and Justice::report_crime
    // directly, exactly as the engine layer will once the surface exists.)
    witness(w.population, kWitness, "player",
            "saw the stranger lift grain from the market stall", w.day);
    Crime& theft = report_crime(w.justice, "player", "theft", "theft", w.day, kWitness);
    SIM_CHECK_EQ(w.justice.open_crimes.size(), std::size_t{1});
    SIM_CHECK_EQ(theft.witnessed_by, Id(kWitness));

    // The rumour graph: no canon social link exists yet (D-011), so the
    // scenario wires the edge, then proves the fact has NOT hopped yet.
    w.population.knows[kWitness].push_back(kListener);
    const Npc* listener_before = find_npc(w.population, kListener);
    SIM_CHECK(listener_before != nullptr);
    SIM_CHECK(listener_before->memory.empty());

    // First tick: a witnessed crime is heard the morning it is processed
    // (Justice.cpp), so the hearing happens on this very tick. The rumour has
    // not spread yet — a fact heard "today" retells "tomorrow" (Population.cpp).
    w.advance_days(1);
    SIM_CHECK(w.justice.open_crimes.empty());
    SIM_CHECK_EQ(w.justice.verdicts.size(), std::size_t{1});
    const Hearing& theft_verdict = w.justice.verdicts.back();
    SIM_CHECK_EQ(theft_verdict.crime_id, theft.id);
    // The real theft row's first penalty option (laws.csv) decides — not a
    // hardcoded expectation of the content.
    std::string expected = theft_row.get("penalty_options");
    expected = expected.substr(0, expected.find(';'));
    SIM_CHECK_EQ(theft_verdict.verdict, expected);
    SIM_CHECK(find_npc(w.population, kListener)->memory.empty());  // still not hopped

    // A second tick: the rumour finally crosses the wired edge.
    w.advance_days(1);
    const Npc* listener_after = find_npc(w.population, kListener);
    SIM_CHECK(listener_after != nullptr);
    SIM_CHECK_EQ(listener_after->memory.size(), std::size_t{1});
    SIM_CHECK_EQ(listener_after->memory[0].subject, Id("player"));

    // Justice writes only JusticeState (Justice.hpp: "Must never: write
    // another module's state"): the verdict does not touch faction standing —
    // documented as a missing kernel surface in scenario_crime.md.
    SIM_CHECK_EQ(standing(w.faction, "the_empire"), 0);
    return true;
}

// A second, real-law crime whose first option differs from the compensation
// fallback (burglary's first option is "death"), so a passing verdict here
// is proof the pipeline read laws.csv and not the fallback.
static bool test_burglary_verdict_differs_from_the_fallback() {
    WorldState w;
    w.init("../db/canon", kSeed);
    SIM_CHECK(w.db.has("laws", "burglary"));
    const Row burglary_row = *w.db.find("laws", "burglary");

    witness(w.population, kWitness, "player", "saw the breach in the merchant's wall", w.day);
    Crime& burglary = report_crime(w.justice, "player", "burglary", "burglary", w.day, kWitness);
    w.advance_days(1);

    SIM_CHECK_EQ(w.justice.verdicts.size(), std::size_t{1});
    const Hearing& v = w.justice.verdicts.back();
    SIM_CHECK_EQ(v.crime_id, burglary.id);
    std::string expected = burglary_row.get("penalty_options");
    expected = expected.substr(0, expected.find(';'));
    SIM_CHECK_EQ(expected, std::string("death"));   // the real canon row, not the fallback
    SIM_CHECK_EQ(v.verdict, expected);
    SIM_CHECK(v.verdict != std::string("compensation"));  // could not pass by fallback coincidence
    return true;
}

// The unwitnessed path: evidence never surfaces on its own, so the crime
// stays on the docket through many ticks (Justice.hpp: "evidence can be
// found later"; a hearing must be requested explicitly).
static bool test_unwitnessed_theft_stays_open() {
    WorldState w;
    w.init("../db/canon", kSeed);
    Crime& theft = report_crime(w.justice, "player", "theft", "theft", w.day, "");  // unwitnessed
    SIM_CHECK_EQ(theft.witnessed_by, Id(""));

    w.advance_days(30);
    SIM_CHECK_EQ(w.justice.verdicts.size(), std::size_t{0});
    SIM_CHECK(find_crime(w.justice, theft.id) != nullptr);

    // No named NPC ever learns of it either — nothing to witness means
    // nothing enters memory or the rumour graph.
    // (W4-C: the city does talk — of the bandit camps beyond the walls — so
    // the check is on memories of the criminal, not on silence itself.)
    for (const Npc& n : w.population.npcs)
        for (const MemoryEntry& m : n.memory) SIM_CHECK(m.subject != "player");
    return true;
}

// Same seed, same script -> identical JusticeState and PopulationState bytes.
static bool test_same_seed_same_script_same_state_bytes() {
    auto run = [] {
        WorldState w;
        w.init("../db/canon", kSeed);
        witness(w.population, kWitness, "player", "saw the stranger lift grain", w.day);
        w.population.knows[kWitness].push_back(kListener);
        (void)report_crime(w.justice, "player", "theft", "theft", w.day, kWitness);
        (void)report_crime(w.justice, "npc_ghost", "theft", "theft", w.day, "");  // unwitnessed
        w.advance_days(3);
        return w;
    };
    const WorldState a = run();
    const WorldState b = run();
    SIM_CHECK_EQ(justice_bytes(a.justice), justice_bytes(b.justice));
    SIM_CHECK_EQ(population_bytes(a.population), population_bytes(b.population));
    // And the run did the expected, non-vacuous thing.
    SIM_CHECK_EQ(a.justice.verdicts.size(), std::size_t{1});     // the witnessed one only
    SIM_CHECK_EQ(a.justice.open_crimes.size(), std::size_t{1});  // the unwitnessed one, still open
    return true;
}

SIM_MAIN(test_witnessed_theft_runs_the_cycle_to_a_verdict_from_real_law,
         test_burglary_verdict_differs_from_the_fallback,
         test_unwitnessed_theft_stays_open,
         test_same_seed_same_script_same_state_bytes)
