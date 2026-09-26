// test_scenario_rite.cpp — T6: one rite, start to finish, with the five
// powers (rpg-systems §10.1), driven only through a real WorldState
// (sim/World.hpp) and the public sim/Magic.hpp API.
//
// The rite: sacrifice_fish_sea_gems (db/canon/rites.csv), performed at the
// City of the Moon. It is the ONLY canon rite with a real deity column
// (the_two_waters) rather than "any" — deity=="any" rites apply no favour
// change at all (Magic.cpp reading 3), so they cannot exercise the favour
// power on a named god. The City of the Moon is canon's maritime capital
// with "direct sea trade routes" (cities.csv), a plausible home for a rite
// to the deities of salt and fresh water; its place requirement, "the first
// great temple (origin myth)", is already exercised against the place tag
// "temple:city_of_the_moon" by test_magic.cpp's own fixtures, so this
// scenario reuses that exact tag as the City of the Moon's temple.
//
// Script, one WorldState, in order:
//   1. Refusal — an unknown rite id (unknown_rite), then the real rite
//      without knowledge (rite_not_known); neither changes state.
//   2. Learning it — the kernel has NO persistent "known rites" state
//      (Magic.hpp:45 "caller tracks"), so "learning" is modeled the only way
//      the kernel supports it: the caller's own bookkeeping flips
//      performer_knows_rite true on every RiteInputs from here on. This gap
//      is reported in kernel/contracts/scenario_rite.md.
//   3. Purity path — rites.csv carries no purity_required value for this row
//      (empty column) and the fixed formula's requirement defaults to 0
//      (Magic.hpp:10), so D-011's "carried but non-gating" holds exactly:
//      a fully impure performer (purity=0) scores identically to a pure one
//      (purity=100). Both are exercised.
//   4. A successful performance — full five-power score, favour +2.
//   5. A failed performance — weakened score, net favour -3 (+2 performed,
//      -5 angered, per Magic.cpp reading 2).
//   6. Every one of the five powers touched by the script, enumerated
//      explicitly against rpg-systems §10.1's own five names.
//   7. Determinism — two identically-seeded worlds run the same script and
//      land on identical RiteResults and identical MagicState bytes.
#include "sim/World.hpp"
#include "sim/Rites.hpp"
#include "sim/Snapshot.hpp"

#include "sim/Test.hpp"

#include <cstdint>
#include <string>

using namespace sim;

namespace {

// seed 1, day 5: this rite rolls 3493 of 10000 (D-022; test_magic.cpp header) — a
// full-offering score of 8000 bp at this roll always succeeds; re-used here so
// the success path is provably a success, not luck.
constexpr std::uint64_t kScenarioSeed = 1;
constexpr std::uint64_t kFailSeed = 14;          // day 5: this rite rolls 9589 (fails <= 9589 bp)
constexpr std::uint64_t kDeterminismSeed = 5150;

const Id kRite = "sacrifice_fish_sea_gems";
const Id kDeity = "the_two_waters";
const std::string kCityTemple = "temple:city_of_the_moon";  // D-005: the slice city

RiteInputs unknowing() { return RiteInputs{}; }  // knows nothing, carries nothing

RiteInputs full_offering(bool known) {
    RiteInputs in;
    in.performer_knows_rite = known;
    in.materials_held = {{"fish", 2}, {"sea_gems", 1}};
    return in;
}

// known=true/false, no materials — the "empty hands" attempt.
RiteInputs empty_handed(bool known) {
    RiteInputs in;
    in.performer_knows_rite = known;
    return in;
}

bool close_enough(double a, double b) { return (a > b ? a - b : b - a) <= 1e-9; }

std::string state_bytes(const MagicState& s) {
    std::string out = "purity=" + std::to_string(s.purity) + ";place=" + s.place + ";favour=";
    for (const auto& [deity, value] : s.favour_by_deity) out += deity + ":" + std::to_string(value) + ",";
    return out;
}

struct ScriptObs {
    RiteResult refusal_unknown;
    RiteResult refusal_no_knowledge;
    RiteResult success;
    RiteResult failure;
};

// The whole rite lifecycle, run identically on any world at the given seed.
// Lands the world on day 5 first (the roll test_magic.cpp already verified
// for this rite), so every attempt of this rite below uses the same
// pinned-down Rng{seed}.fork(5).fork(stable_hash(rite)).below(10000).
ScriptObs run_rite_script(WorldState& w) {
    ScriptObs obs;
    w.advance_days(4);  // day 1 -> day 5
    w.magic.place = kCityTemple;

    // 1. Refusal path, unknown_rite: a nonexistent rite id is refused before
    // any row lookup succeeds, so RiteResult.deity stays empty (Magic.cpp:
    // the "unknown_rite" branch returns before result.deity is ever set).
    obs.refusal_unknown = perform_rite(w.context(), w.magic, "no_such_rite", unknowing());

    // 1b. Refusal path, rite_not_known: the row IS real canon (its deity is
    // filled in before the knowledge gate runs), but the performer has not
    // learned it yet — the refusal this scenario is really about.
    obs.refusal_no_knowledge = perform_rite(w.context(), w.magic, kRite,
                                            full_offering(/*known=*/false));

    // 2. "Learning" the rite: the kernel has no state for this, so the
    // caller's own RiteInputs.performer_knows_rite flips true from here on
    // (see file header — this is the reported gap, not a kernel feature).

    // 3. Purity path (D-011: carried, non-gating): asserted separately, on
    // twin worlds with identical favour so the comparison isn't confounded
    // by this world's own favour changing between the two attempts (see
    // test_purity_is_carried_but_non_gating). Here it only sets purity=100
    // (a clean performer) for the rest of the script's steps.
    w.magic.purity = 100;

    // 4. A successful performance: full offering, correct place, this
    // world's seed/day roll < score (kScenarioSeed at day 5 verified above).
    obs.success = perform_rite(w.context(), w.magic, kRite, full_offering(true));

    // 5. A weakened attempt: no materials, wrong place -> lower score. On
    // THIS seed's day-5 roll it may still succeed once favour has climbed
    // (the roll is fixed per rite and day, not reset per attempt) — captured here
    // only for the determinism check below; the guaranteed failure case
    // (documented -5 anger, kFailSeed) is proven on its own fresh world in
    // test_success_then_failure_move_favour.
    w.magic.place = "riverbank";
    obs.failure = perform_rite(w.context(), w.magic, kRite, empty_handed(true));
    return obs;
}

}  // namespace

// --- 1. a refusal, in isolation, changes no state ----------------------------

static bool test_refusal_changes_no_state() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    w.magic.place = kCityTemple;

    const RiteResult unknown = perform_rite(w.context(), w.magic, "no_such_rite", unknowing());
    SIM_CHECK(!unknown.performed);
    SIM_CHECK(w.magic.favour_by_deity.empty());

    const RiteResult not_known = perform_rite(w.context(), w.magic, kRite, full_offering(false));
    SIM_CHECK(!not_known.performed);
    SIM_CHECK(w.magic.favour_by_deity.empty());  // still nothing: a refusal is a refusal
    return true;
}

// --- 1/2. refusal, then knowledge gates the attempt -------------------------

static bool test_refusal_then_knowledge_gates_attempt() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    const ScriptObs obs = run_rite_script(w);

    SIM_CHECK(!obs.refusal_unknown.performed);
    SIM_CHECK_EQ(obs.refusal_unknown.refusal_reason, std::string("unknown_rite"));
    SIM_CHECK(obs.refusal_unknown.deity.empty());
    SIM_CHECK(obs.refusal_unknown.effect_family.empty());
    SIM_CHECK(obs.refusal_unknown.score == 0.0);

    SIM_CHECK(!obs.refusal_no_knowledge.performed);
    SIM_CHECK_EQ(obs.refusal_no_knowledge.refusal_reason, std::string("rite_not_known"));
    SIM_CHECK_EQ(obs.refusal_no_knowledge.deity, kDeity);  // the row was found; only knowledge failed
    return true;
}

// 3. Purity path (D-011: "carried but non-gating until [purity_required is
// authored]"). Twin worlds, identical in everything but purity, so the
// comparison isn't confounded by one attempt's own favour change feeding
// the next attempt's score (rites.csv sets no purity_required for this row,
// so the fixed formula's default-0 requirement makes both worlds identical).
static bool test_purity_is_carried_but_non_gating() {
    WorldState pure;
    pure.init("../db/canon", kScenarioSeed);
    pure.advance_days(4);
    pure.magic.place = kCityTemple;
    pure.magic.purity = 100;
    const RiteResult pure_ok = perform_rite(pure.context(), pure.magic, kRite, full_offering(true));

    WorldState impure;
    impure.init("../db/canon", kScenarioSeed);
    impure.advance_days(4);
    impure.magic.place = kCityTemple;
    impure.magic.purity = 0;  // fully impure
    const RiteResult impure_ok = perform_rite(impure.context(), impure.magic, kRite, full_offering(true));

    SIM_CHECK(pure_ok.performed);
    SIM_CHECK(impure_ok.performed);
    SIM_CHECK(pure_ok.score == impure_ok.score);          // identical score: purity carried, not gating
    SIM_CHECK_EQ(pure_ok.succeeded, impure_ok.succeeded);  // same draw, same score -> same outcome
    return true;
}

// --- 4/5. success raises favour by 2; failure nets -3 -----------------------
//
// Each on its own fresh world (a shared multi-attempt world would let one
// attempt's favour change feed the next attempt's score AND its draw stays
// fixed per day — see run_rite_script's own note — so isolating each claim
// is the only way to pin an exact favour number to it).

static bool test_successful_performance_raises_favour_by_two() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);  // seed 1
    w.advance_days(4);  // day 5: this rite rolls 3493 (test_magic.cpp)
    w.magic.place = kCityTemple;

    const RiteResult r = perform_rite(w.context(), w.magic, kRite, full_offering(true));
    SIM_CHECK(r.performed);
    SIM_CHECK_EQ(r.deity, kDeity);
    SIM_CHECK_EQ(r.effect_family, std::string("offering (favour)"));
    SIM_CHECK(close_enough(r.score, 0.80));
    SIM_CHECK(r.succeeded);           // score 8000 bp, roll 3493 < 8000
    SIM_CHECK_EQ(favour(w.magic, kDeity), 52);  // 50 + 2 (performed, succeeded)
    return true;
}

static bool test_failed_performance_nets_negative_three() {
    // The documented failure arithmetic (+2 performed, -5 angered, net -3),
    // exactly as test_magic.cpp verifies: score 5000 bp, roll 9589 >= 5000.
    WorldState fw;
    fw.init("../db/canon", kFailSeed);
    fw.advance_days(4);  // day 5
    fw.magic.place = "riverbank";  // wrong place, and no materials below
    const RiteResult failed = perform_rite(fw.context(), fw.magic, kRite, empty_handed(true));
    SIM_CHECK(failed.performed);
    SIM_CHECK(close_enough(failed.score, 0.50));
    SIM_CHECK(!failed.succeeded);
    SIM_CHECK_EQ(favour(fw.magic, kDeity), 47);  // 50 + 2 (performed) - 5 (angered)
    return true;
}

// --- 6. every one of the five powers is exercised somewhere in the script ---

static bool test_all_five_powers_exercised() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    w.magic.place = kCityTemple;

    // Power 1 — favour: absent -> neutral 50; a performed rite moves it.
    SIM_CHECK_EQ(favour(w.magic, kDeity), 50);
    const RiteResult r1 = perform_rite(w.context(), w.magic, kRite, full_offering(true));
    SIM_CHECK(r1.performed);
    SIM_CHECK(favour(w.magic, kDeity) != 50);  // favour moved: the power fired

    // Power 2 — knowledge: a gate, not a weight (score unaffected by it, but
    // the attempt is refused entirely without it — see the refusal test).
    const RiteResult refused = perform_rite(w.context(), w.magic, kRite, full_offering(false));
    SIM_CHECK_EQ(refused.refusal_reason, std::string("rite_not_known"));

    // Power 3 — materials: full offering scores 0.20 higher than none.
    const RiteResult with_materials = perform_rite(w.context(), w.magic, kRite, full_offering(true));
    const RiteResult without_materials = perform_rite(w.context(), w.magic, kRite, empty_handed(true));
    SIM_CHECK(with_materials.score > without_materials.score);

    // Power 4 — purity and place: place is the half the kernel gates on
    // (purity is carried, non-gating per D-011, proven above). Right place
    // scores 0.10 higher than the wrong one, all else equal.
    w.magic.place = kCityTemple;
    const RiteResult right_place = perform_rite(w.context(), w.magic, kRite, full_offering(true));
    w.magic.place = "riverbank";
    const RiteResult wrong_place = perform_rite(w.context(), w.magic, kRite, full_offering(true));
    SIM_CHECK(right_place.score > wrong_place.score);

    // Power 5 — time: this rite's time_window is empty (rites.csv), so the
    // 0.10 is always granted regardless of the calendar day — the trivial
    // case is exercised (no canon rite sets a real time_window; see the
    // scenario contract for the gap this leaves unexercised). Twin fresh
    // worlds (as in the purity check above) so one attempt's own favour
    // change can't confound the comparison.
    WorldState early;
    early.init("../db/canon", kScenarioSeed);
    early.magic.place = kCityTemple;
    const RiteResult day1 = perform_rite(early.context(), early.magic, kRite, full_offering(true));

    WorldState late;
    late.init("../db/canon", kScenarioSeed);
    late.advance_days(10);
    late.magic.place = kCityTemple;
    const RiteResult day11 = perform_rite(late.context(), late.magic, kRite, full_offering(true));
    SIM_CHECK(close_enough(day1.score, day11.score));  // time never varies this rite's score
    return true;
}

// --- 7. determinism -----------------------------------------------------

static bool test_rite_lifecycle_is_deterministic() {
    WorldState a;
    a.init("../db/canon", kDeterminismSeed);
    WorldState b;
    b.init("../db/canon", kDeterminismSeed);

    const ScriptObs oa = run_rite_script(a);
    const ScriptObs ob = run_rite_script(b);

    SIM_CHECK_EQ(oa.success.performed, ob.success.performed);
    SIM_CHECK_EQ(oa.success.succeeded, ob.success.succeeded);
    SIM_CHECK(oa.success.score == ob.success.score);
    SIM_CHECK_EQ(oa.failure.performed, ob.failure.performed);
    SIM_CHECK_EQ(oa.failure.succeeded, ob.failure.succeeded);
    SIM_CHECK(!state_bytes(a.magic).empty());
    SIM_CHECK_EQ(state_bytes(a.magic), state_bytes(b.magic));
    return true;
}

// ===========================================================================
// K-1 — the magic loop closed: knowledge -> offering -> rite -> effect,
// through the caller-side verbs of sim/Rites.hpp (gaps 1, 2 and 5 of
// kernel/contracts/scenario_rite.md). Magic itself is unchanged.
// ===========================================================================

namespace {

const Id kPriest = "lu_dingira_priest_moon";  // people.csv, role "priest of the moon"
const Id kTrader = "ea_nasir_grain_trader";   // people.csv, a market trader: teaches no rite

int count_of(const WorldState& w, const Id& item) {
    const auto inv = w.inventories.find("player");
    if (inv == w.inventories.end()) return 0;
    return count_of(inv->second, item);
}

}  // namespace

// Learned from the temple's priest; fish and sea gems leave the inventory;
// success is favour with the two waters beyond the rite's own +2.
static bool test_k1_sacrifice_learned_offered_and_answered() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    w.advance_days(4);  // day 5: this rite rolls 3493
    w.magic.place = kCityTemple;
    set_count(w.inventories["player"], "fish", 2);
    set_count(w.inventories["player"], "sea_gems", 1);

    // Unlearned: refused, nothing spent.
    RiteOutcome o = perform_rite_in_world(w, kRite);
    SIM_CHECK(!o.rite.performed);
    SIM_CHECK_EQ(o.refusal_reason, std::string("rite_not_known"));
    SIM_CHECK_EQ(count_of(w, "fish"), 2);
    SIM_CHECK(o.consumed.empty());

    // The wrong teacher, then the priest of the moon.
    SIM_CHECK_EQ(learn_rite_from_teacher(w, kRite, kTrader).refusal_reason,
                 std::string("not_taught_by_teacher"));
    SIM_CHECK_EQ(learn_rite_from_teacher(w, kRite, "nobody_at_all").refusal_reason,
                 std::string("unknown_teacher"));
    SIM_CHECK_EQ(learn_rite_from_teacher(w, "no_such_rite", kPriest).refusal_reason,
                 std::string("unknown_rite"));
    const std::vector<Id> taught = rites_taught_by(w, kPriest);
    SIM_CHECK_EQ(taught.size(), std::size_t{2});
    SIM_CHECK_EQ(taught[0], std::string("hymns_deity_names"));
    SIM_CHECK_EQ(taught[1], kRite);
    SIM_CHECK(learn_rite_from_teacher(w, kRite, kPriest).learned);
    SIM_CHECK(knows_rite(w.magic, kRite));
    SIM_CHECK_EQ(learn_rite_from_teacher(w, kRite, kPriest).refusal_reason,
                 std::string("already_known"));

    o = perform_rite_in_world(w, kRite);
    SIM_CHECK(o.rite.performed);
    SIM_CHECK(o.rite.succeeded);
    SIM_CHECK(close_enough(o.rite.score, 0.80));  // the fixed formula, untouched
    SIM_CHECK_EQ(o.addressed, kDeity);
    SIM_CHECK_EQ(o.consumed.size(), std::size_t{2});
    SIM_CHECK_EQ(count_of(w, "fish"), 1);
    SIM_CHECK_EQ(count_of(w, "sea_gems"), 0);
    SIM_CHECK_EQ(favour(w.magic, kDeity), 50 + 2 + kOfferingFavour);
    SIM_CHECK_EQ(o.effect, "favour:" + kDeity + ":+" + std::to_string(kOfferingFavour));
    return true;
}

// Magic.hpp:16 — a failed rite still consumes the materials; no effect lands.
static bool test_k1_failed_rite_still_consumes_the_offering() {
    WorldState w;
    w.init("../db/canon", kFailSeed);
    w.advance_days(4);  // day 5: this rite rolls 9589
    w.magic.place = "riverbank";
    set_count(w.inventories["player"], "fish", 1);  // no sea gems: materials short
    SIM_CHECK(learn_rite_from_teacher(w, kRite, kPriest).learned);

    const RiteOutcome o = perform_rite_in_world(w, kRite);
    SIM_CHECK(o.rite.performed);
    SIM_CHECK(!o.rite.succeeded);          // 5000 bp <= roll 9589
    SIM_CHECK_EQ(count_of(w, "fish"), 0);  // spent anyway
    SIM_CHECK(o.effect.empty());
    SIM_CHECK_EQ(favour(w.magic, kDeity), 47);  // Magic's own +2 -5, nothing more
    return true;
}

// A hymn to "any" god must name one; a refusal spends and changes nothing.
static bool test_k1_hymn_addresses_a_named_god() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    w.advance_days(4);
    w.magic.place = kCityTemple;
    SIM_CHECK(learn_rite_from_teacher(w, "hymns_deity_names", kPriest).learned);

    const std::string before = save_world(w);
    SIM_CHECK_EQ(perform_rite_in_world(w, "hymns_deity_names").refusal_reason,
                 std::string("no_deity_addressed"));
    SIM_CHECK_EQ(perform_rite_in_world(w, "hymns_deity_names", "not_a_god").refusal_reason,
                 std::string("unknown_deity"));
    SIM_CHECK(save_world(w) == before);  // refusals leave no trace

    // Intangible materials (vocal performance; the deity's true name) are the
    // knowing performer's own: full 0.20, nothing debited.
    const RiteOutcome o = perform_rite_in_world(w, "hymns_deity_names", "inanna");
    SIM_CHECK(o.rite.performed);
    SIM_CHECK(close_enough(o.rite.score, 0.80));
    SIM_CHECK(o.rite.succeeded);
    SIM_CHECK(o.consumed.empty());
    SIM_CHECK_EQ(favour(w.magic, "inanna"), 50 + 2 + kHymnFavour);
    SIM_CHECK(w.magic.favour_by_deity.count("any") == 0);
    return true;
}

// Zisurru read from a tablet; the flour circle wards a place for kWardDays.
static bool test_k1_zisurru_from_a_tablet_wards_a_household() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    w.advance_days(4);
    const Id rite = "zisurru_warding";
    SIM_CHECK_EQ(learn_rite_from_text(w, rite, "zisurru_incantation_tablet").refusal_reason,
                 std::string("text_not_held"));
    set_count(w.inventories["player"], "zisurru_incantation_tablet", 1);
    set_count(w.inventories["player"], "clay_liver_model", 1);
    SIM_CHECK_EQ(learn_rite_from_text(w, rite, "clay_liver_model").refusal_reason,
                 std::string("not_taught_in_text"));
    SIM_CHECK(learn_rite_from_text(w, rite, "zisurru_incantation_tablet").learned);
    SIM_CHECK_EQ(count_of(w, "zisurru_incantation_tablet"), 1);  // read, not consumed

    set_count(w.inventories["player"], "different_types_of_flour", 1);
    w.magic.place = "household:player";
    const RiteOutcome o = perform_rite_in_world(w, rite);
    SIM_CHECK(o.rite.succeeded);  // 9000 bp > roll 5312
    SIM_CHECK_EQ(count_of(w, "different_types_of_flour"), 0);
    SIM_CHECK_EQ(o.addressed, std::string("household:player"));
    const DayNumber until = 5 + kWardDays - 1;
    SIM_CHECK_EQ(w.rite_effects.wards_by_place.at("household:player").until, until);
    SIM_CHECK(ward_holds(w.rite_effects, "household:player", w.day));
    SIM_CHECK(ward_holds(w.rite_effects, "household:player", until));
    SIM_CHECK(!ward_holds(w.rite_effects, "household:player", until + 1));
    SIM_CHECK(!ward_holds(w.rite_effects, "temple:city_of_the_moon", w.day));
    SIM_CHECK(w.magic.favour_by_deity.empty());  // protection addresses no god
    return true;
}

// Barutu from the clay liver model: an omen with a probability, never a certainty.
static bool test_k1_barutu_reads_an_omen() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    w.advance_days(4);
    set_count(w.inventories["player"], "clay_liver_model", 1);
    SIM_CHECK(learn_rite_from_text(w, "barutu_haruspicy", "clay_liver_model").learned);
    set_count(w.inventories["player"], "a_burned_goat's_liver", 1);
    w.magic.place = "house:diviner";
    add_favour(w.magic, "inanna", 30);  // 80: the true answer is "favourable"
    const std::uint64_t rng_before = w.rng.state();

    const RiteOutcome o = perform_rite_in_world(w, "barutu_haruspicy", "inanna");
    SIM_CHECK(o.rite.succeeded);
    SIM_CHECK_EQ(count_of(w, "a_burned_goat's_liver"), 0);
    SIM_CHECK_EQ(w.rite_effects.omens.size(), std::size_t{1});
    const Omen& omen = w.rite_effects.omens.front();
    SIM_CHECK_EQ(omen.subject, std::string("inanna"));
    SIM_CHECK_EQ(omen.confidence_pct, kOmenTruthPct);
    SIM_CHECK(omen.confidence_pct < 100);  // never a certainty
    const bool reads_true =
        w.rng.fork(static_cast<std::uint64_t>(w.day) ^ kOmenSalt).unit() < kOmenTruthPct / 100.0;
    SIM_CHECK_EQ(omen.sign, std::string(reads_true ? "favourable" : "unfavourable"));
    SIM_CHECK_EQ(w.rng.state(), rng_before);  // forked, never advanced
    return true;
}

// The whole K-1 loop is deterministic and survives save/load byte-for-byte.
static bool test_k1_loop_is_deterministic_and_saves() {
    const auto run = [](WorldState& w) {
        w.advance_days(4);
        w.magic.place = kCityTemple;
        set_count(w.inventories["player"], "fish", 3);
        set_count(w.inventories["player"], "sea_gems", 3);
        set_count(w.inventories["player"], "clay_liver_model", 1);
        set_count(w.inventories["player"], "a_burned_goat's_liver", 2);
        (void)learn_rite_from_teacher(w, kRite, kPriest);
        (void)learn_rite_from_text(w, "barutu_haruspicy", "clay_liver_model");
        (void)perform_rite_in_world(w, kRite);
        w.advance_days(3);
        (void)perform_rite_in_world(w, kRite);
        w.magic.place = "house:diviner";
        (void)perform_rite_in_world(w, "barutu_haruspicy", kDeity);
    };
    WorldState a;
    a.init("../db/canon", kDeterminismSeed);
    WorldState b;
    b.init("../db/canon", kDeterminismSeed);
    run(a);
    run(b);
    const std::string sa = save_world(a);
    SIM_CHECK(sa == save_world(b));

    WorldState r;
    load_world(r, "../db/canon", sa);
    SIM_CHECK(save_world(r) == sa);
    SIM_CHECK(knows_rite(r.magic, kRite));
    SIM_CHECK(knows_rite(r.magic, "barutu_haruspicy"));
    SIM_CHECK_EQ(r.rite_effects.omens.size(), a.rite_effects.omens.size());
    return true;
}

// D-022: every rite's roll is its own (fork(day).fork(stable_hash(rite))) and
// is taken from the saved rng state, so a world saved and loaded mid-day
// rolls exactly as the world that never left memory.
static bool test_d022_rolls_survive_save_load() {
    WorldState a;
    a.init("../db/canon", kDeterminismSeed);
    a.advance_days(4);
    a.magic.place = kCityTemple;
    set_count(a.inventories["player"], "fish", 2);
    set_count(a.inventories["player"], "sea_gems", 2);
    SIM_CHECK(learn_rite_from_teacher(a, kRite, kPriest).learned);
    SIM_CHECK(learn_rite_from_teacher(a, "hymns_deity_names", kPriest).learned);

    WorldState r;
    load_world(r, "../db/canon", save_world(a));
    SIM_CHECK_EQ(r.rng.state(), a.rng.state());

    for (WorldState* w : {&a, &r}) {
        const RiteOutcome s = perform_rite_in_world(*w, kRite);
        const RiteOutcome h = perform_rite_in_world(*w, "hymns_deity_names", "inanna");
        SIM_CHECK(s.rite.performed && h.rite.performed);
        const auto roll = [&](const Id& rite) {
            return Rng{kDeterminismSeed}.fork(5).fork(stable_hash(rite)).below(10000);
        };
        SIM_CHECK_EQ(s.rite.succeeded, roll(kRite) < static_cast<std::uint64_t>(s.rite.score_bp));
        SIM_CHECK_EQ(h.rite.succeeded,
                     roll("hymns_deity_names") < static_cast<std::uint64_t>(h.rite.score_bp));
    }
    SIM_CHECK(save_world(a) == save_world(r));
    return true;
}

SIM_MAIN(test_refusal_changes_no_state,
         test_refusal_then_knowledge_gates_attempt,
         test_purity_is_carried_but_non_gating,
         test_successful_performance_raises_favour_by_two,
         test_failed_performance_nets_negative_three,
         test_all_five_powers_exercised,
         test_rite_lifecycle_is_deterministic,
         test_k1_sacrifice_learned_offered_and_answered,
         test_k1_failed_rite_still_consumes_the_offering,
         test_k1_hymn_addresses_a_named_god,
         test_k1_zisurru_from_a_tablet_wards_a_household,
         test_k1_barutu_reads_an_omen,
         test_k1_loop_is_deterministic_and_saves,
         test_d022_rolls_survive_save_load)
