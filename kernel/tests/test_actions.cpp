// test_actions.cpp — W2-A: the world's verbs (kernel/include/sim/Actions.hpp).
// Exercises the full crime cycle (commit_crime -> hold_crime_hearing) and
// quest completion/failure against a real WorldState (canon loaded from
// ../db/canon), the same pattern test_scenario_crime.cpp and
// test_scenario_quest.cpp use.
#include "sim/Actions.hpp"

#include "sim/Test.hpp"

#include <cstdint>
#include <string>

using namespace sim;

namespace {

const char* kCity = "city_of_the_moon";  // D-005: the slice city
const char* kWitness = "the_prophet";    // the only named NPC seeded there
constexpr std::uint64_t kSeed = 4001;

std::string justice_bytes(const JusticeState& s) {
    std::string out = "next_id=" + std::to_string(s.next_id);
    for (const Crime& c : s.open_crimes)
        out += "|open:" + c.id + "," + c.criminal + "," + c.stage;
    for (const Hearing& h : s.verdicts)
        out += "|verdict:" + h.crime_id + "," + h.verdict + "," + h.tablet_id + "," +
               std::to_string(h.compensation_paid);
    return out;
}

std::string faction_bytes(const FactionState& s) {
    std::string out;
    for (const auto& [faction, standing] : s.standing_by_faction)
        out += "|" + faction + "=" + std::to_string(standing);
    for (const auto& [faction, outlawed] : s.outlawed_by_faction)
        out += "|outlawed:" + faction + "=" + std::to_string(outlawed);
    return out;
}

std::string property_bytes(const PropertyState& s) {
    std::string out;
    for (const auto& [owner, silver] : s.purse_by_owner)
        out += "|" + owner + "=" + std::to_string(silver);
    return out;
}

}  // namespace

// Full theft cycle: witness -> alarm/pursuit/detention -> hearing N days
// later -> compensation paid from a funded purse to the victim.
static bool test_theft_cycle_ends_in_paid_compensation() {
    WorldState w;
    w.init("../db/canon", kSeed);
    credit_purse(w.property, "player", 200);  // the criminal can cover it

    const Id crime_id = commit_crime(w, "player", "theft", kCity, {kWitness});
    SIM_CHECK_EQ(w.justice.open_crimes.size(), std::size_t{1});
    const Crime* c = find_crime(w.justice, crime_id);
    SIM_CHECK(c != nullptr);
    SIM_CHECK_EQ(c->stage, std::string("detained"));  // alarm -> pursuit -> detention collapsed

    w.advance_days(kHearingDelayDays);  // the window between detention and trial
    SIM_CHECK(find_crime(w.justice, crime_id) != nullptr);  // scheduled hearing_day not reached yet

    const Hearing sealed = hold_crime_hearing(w, crime_id, "the_grain_merchant", kCity);
    SIM_CHECK_EQ(sealed.verdict, std::string("compensation"));  // theft's real first option
    SIM_CHECK_EQ(sealed.compensation_paid, Silver{kCompensationSilver});
    SIM_CHECK_EQ(sealed.tablet_id, "verdict_tablet_" + crime_id);
    SIM_CHECK_EQ(purse(w.property, "player"), Silver{200 - kCompensationSilver});
    SIM_CHECK_EQ(purse(w.property, "the_grain_merchant"), Silver{kCompensationSilver});

    // Standing with the jurisdiction's faction drops (never to outlawry for
    // a mere compensation verdict).
    const Id faction = city_faction(kCity);
    SIM_CHECK(standing(w.faction, faction) < 100);
    SIM_CHECK(!is_outlawed(w.faction, faction));
    return true;
}

// Unpaid compensation escalates to the law row's next penalty_options entry
// (theft: compensation;confiscation;death -> confiscation).
static bool test_unpaid_compensation_escalates() {
    WorldState w;
    w.init("../db/canon", kSeed);
    // "player" has no purse at all: take_from_purse must fail.
    SIM_CHECK_EQ(purse(w.property, "player"), Silver{0});

    const Id crime_id = commit_crime(w, "player", "theft", kCity, {kWitness});
    const Hearing sealed = hold_crime_hearing(w, crime_id, "the_grain_merchant", kCity);
    SIM_CHECK_EQ(sealed.verdict, std::string("confiscation"));  // escalated past compensation
    SIM_CHECK_EQ(sealed.compensation_paid, Silver{0});
    SIM_CHECK_EQ(purse(w.property, "the_grain_merchant"), Silver{0});  // nothing to collect
    return true;
}

// Burglary's real first penalty option is death (laws.csv, not the
// compensation fallback) -> the hearing seals death, and death verdicts
// outlaw the criminal with the jurisdiction faction: rank 0 (mechanics.md
// row 12), standing forced to 0.
static bool test_burglary_verdict_is_death_and_outlaws() {
    WorldState w;
    w.init("../db/canon", kSeed);
    const Id faction = city_faction(kCity);
    add_standing(w.faction, faction, 60);  // some standing to lose
    SIM_CHECK(standing(w.faction, faction) > 0);

    const Id crime_id = commit_crime(w, "player", "burglary", kCity, {kWitness});
    const Hearing sealed = hold_crime_hearing(w, crime_id, "", kCity);
    SIM_CHECK_EQ(sealed.verdict, std::string("death"));
    SIM_CHECK_EQ(sealed.tablet_id, "verdict_tablet_" + crime_id);

    SIM_CHECK(is_outlawed(w.faction, faction));
    SIM_CHECK_EQ(standing(w.faction, faction), 0);  // rank 0: no standing left
    return true;
}

// complete_quest pays the reward columns from a real canon row
// (the_priestess_debt: reward_silver=35, reward_faction=temple_of_sun_and_moon,
// reward_standing=5 — db/canon/quests.csv, W2-A).
static bool test_quest_completion_pays_silver_and_standing() {
    WorldState w;
    w.init("../db/canon", kSeed);
    SIM_CHECK(w.db.has("quests", "the_priestess_debt"));
    const Row row = *w.db.find("quests", "the_priestess_debt");
    const Silver expected_silver = std::stoll(row.get("reward_silver"));
    const Id expected_faction = row.get("reward_faction");
    const int expected_standing = std::stoi(row.get("reward_standing"));
    SIM_CHECK(expected_silver > 0);
    SIM_CHECK(!expected_faction.empty());

    (void)accept(w.quests, "the_priestess_debt", w.day);
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});

    complete_quest(w, "the_priestess_debt", w.day);
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.completed.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.completed[0], Id("the_priestess_debt"));
    SIM_CHECK_EQ(purse(w.property, "player"), expected_silver);
    SIM_CHECK_EQ(standing(w.faction, expected_faction), expected_standing);
    return true;
}

// The failure path: fail_quest moves the quest to failed_list and pays
// nothing, independent of tick_quests' own deadline sweep.
static bool test_quest_failure_pays_nothing() {
    WorldState w;
    w.init("../db/canon", kSeed);
    (void)accept(w.quests, "the_priestess_debt", w.day);

    fail_quest(w, "the_priestess_debt", w.day);
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{1});
    SIM_CHECK(w.quests.completed.empty());
    SIM_CHECK_EQ(purse(w.property, "player"), Silver{0});
    SIM_CHECK_EQ(standing(w.faction, "temple_of_sun_and_moon"), 0);
    return true;
}

// Same seed, same script -> identical JusticeState/FactionState/PropertyState
// bytes (crime cycle + quest completion in one run).
static bool test_same_seed_same_script_same_state_bytes() {
    auto run = [] {
        WorldState w;
        w.init("../db/canon", kSeed);
        credit_purse(w.property, "player", 200);
        const Id crime_id = commit_crime(w, "player", "theft", kCity, {kWitness});
        w.advance_days(kHearingDelayDays);
        (void)hold_crime_hearing(w, crime_id, "the_grain_merchant", kCity);
        (void)accept(w.quests, "the_priestess_debt", w.day);
        complete_quest(w, "the_priestess_debt", w.day);
        return w;
    };
    const WorldState a = run();
    const WorldState b = run();
    SIM_CHECK_EQ(justice_bytes(a.justice), justice_bytes(b.justice));
    SIM_CHECK_EQ(faction_bytes(a.faction), faction_bytes(b.faction));
    SIM_CHECK_EQ(property_bytes(a.property), property_bytes(b.property));
    // Non-vacuous: the run did the expected thing both times.
    SIM_CHECK_EQ(a.justice.verdicts.size(), std::size_t{1});
    SIM_CHECK_EQ(a.quests.completed.size(), std::size_t{1});
    return true;
}

SIM_MAIN(test_theft_cycle_ends_in_paid_compensation,
         test_unpaid_compensation_escalates,
         test_burglary_verdict_is_death_and_outlaws,
         test_quest_completion_pays_silver_and_standing,
         test_quest_failure_pays_nothing,
         test_same_seed_same_script_same_state_bytes)
