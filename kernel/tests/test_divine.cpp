// test_divine.cpp — W5: divine wrath. Every designer row the module implements:
//   row 26 — an oath broken, the gods take notice (the oath-breaker curse);
//   rows 13/20 — purity is load-bearing: an impure or failed rite offends the
//                god it addresses;
//   row 12 — a conviction for an offence against the gods (sacrilege,
//            tomb_robbery, oath_breaking) adds substantial wrath;
// and the invented shape around them (tiers, decay, atonement, the omen skew),
// exactly as ledgered in docs/proposals/invented-ledger-divine.md.
//
// Rolls are pinned the way test_scenario_rite.cpp pins them: seed 1 day 5
// rolls 3493 for sacrifice_fish_sea_gems (success at 8000 bp), seed 14 day 5
// rolls 9589 (failure at 7000 bp), seed 1 day 5 rolls 7200 for the atonement
// rite (success at 8000 bp) and 6813 for barutu (success at 8000 bp; its omen
// draw that day is 47 of 100 — reads true against kOmenTruthPct 75).
#include "sim/World.hpp"
#include "sim/Divine.hpp"
#include "sim/Rites.hpp"
#include "sim/Actions.hpp"
#include "sim/Snapshot.hpp"
#include "sim/CApi.h"
// The opaque handle's one definition (private to kernel/src): included ONLY
// by the two C-boundary tests at the bottom, to swear an oath the C surface
// cannot yet create (there is no sim_world_swear).
#include "../src/CApiInternal.hpp"

#include "sim/Test.hpp"

#include <cstring>
#include <string>

using namespace sim;

namespace {

constexpr std::uint64_t kScenarioSeed = 1;      // day 5: the success rolls above
constexpr std::uint64_t kFailSeed = 14;         // day 5: sacrifice rolls 9589 (fails)
constexpr std::uint64_t kDeterminismSeed = 5150;

const Id kSacrifice = "sacrifice_fish_sea_gems";
const Id kMoonPriest = "lu_dingira_priest_moon";   // people.csv, role "priest of the moon"
const Id kSunPriest = "ibni_shamash_priest_sun";   // people.csv, role "priest of the sun"
const Id kWitness = "sin_eribam_watchman";         // people.csv, a watchman

// Day-5 world at the pinned seed, standing in the city temple, clean.
WorldState day5_world(std::uint64_t seed = kScenarioSeed) {
    WorldState w;
    w.init("../db/canon", seed);
    w.advance_days(4);  // day 1 -> day 5
    w.magic.place = "temple:city_of_the_moon";
    return w;
}

// Swear-and-break cycles (the one-Great-King rule allows a fresh oath once
// the last is broken). Six cycles = 90 wrath with the witness god: the curse.
bool swear_and_break(WorldState& w, int times) {
    for (int i = 0; i < times; ++i) {
        const Oath* oath = swear(w.faction, "player", "the_empire", w.day);
        SIM_CHECK(oath != nullptr);
        break_oath_in_world(w, oath->id);
    }
    return true;
}

// Deterministic byte encoding of DivineState (maps are Id-sorted).
std::string divine_bytes(const DivineState& d) {
    std::string out;
    for (const auto& [offender, per_deity] : d.wrath_by_offender)
        for (const auto& [deity, value] : per_deity)
            out += offender + '>' + deity + '=' + std::to_string(value) + ';';
    out += '|';
    for (const auto& [offender, deity] : d.curse_by_offender) out += offender + '>' + deity + '!';
    out += '|';
    for (const auto& [deity, delta] : d.pending_favour_penalty)
        out += deity + '=' + std::to_string(delta) + ';';
    return out;
}

}  // namespace

// --- the wrath book and the tier ladder --------------------------------------

static bool test_wrath_reads_zero_and_tier_ladder() {
    DivineState d;
    SIM_CHECK_EQ(wrath(d, "player", "utu"), 0);
    SIM_CHECK(d.wrath_by_offender.empty());  // a read creates no entry

    SIM_CHECK_EQ(wrath_tier(0), 0);
    SIM_CHECK_EQ(wrath_tier(19), 0);
    SIM_CHECK_EQ(wrath_tier(kWrathIllOmenAt), 1);   // 20: ill omens
    SIM_CHECK_EQ(wrath_tier(49), 1);
    SIM_CHECK_EQ(wrath_tier(kWrathDisfavourAt), 2); // 50: disfavour
    SIM_CHECK_EQ(wrath_tier(79), 2);
    SIM_CHECK_EQ(wrath_tier(kWrathCurseAt), 3);     // 80: the curse
    SIM_CHECK_EQ(wrath_tier(kWrathMax), 3);
    SIM_CHECK_EQ(wrath_tier(999), 3);               // total for raw caller input
    return true;
}

// --- row 26: the oath-breaker curse is divine wrath ---------------------------

static bool test_oath_break_accrues_wrath_with_the_witness() {
    // Faction-level (the module hook, no world needed).
    FactionState s;
    DivineState d;
    const Oath* oath = swear(s, "player", "the_empire", 1);
    SIM_CHECK(oath != nullptr);
    break_oath(s, oath->id);  // no divine book handed over: Wave-1 behaviour
    SIM_CHECK(d.wrath_by_offender.empty());

    const Oath* second = swear(s, "player", "the_barbarians", 2);
    SIM_CHECK(second != nullptr);
    break_oath(s, second->id, &d);
    SIM_CHECK_EQ(wrath(d, "player", kOathWitnessDeity), kOathBreakWrath);
    SIM_CHECK(s.oath_breaker_curse);  // Faction's own flag is untouched by W5
    SIM_CHECK(!is_cursed(d, "player"));  // one break is tier 0: no curse yet

    break_oath(s, second->id, &d);  // double break: one drop, one wrath, per oath
    SIM_CHECK_EQ(wrath(d, "player", kOathWitnessDeity), kOathBreakWrath);
    break_oath(s, "no_such_oath", &d);  // unknown id: nothing happens
    SIM_CHECK_EQ(wrath(d, "player", kOathWitnessDeity), kOathBreakWrath);
    return true;
}

// --- rows 13/20: impure and failed rites offend the addressed god --------------

static bool test_failed_rite_accrues_wrath() {
    WorldState w = day5_world(kFailSeed);  // day 5: sacrifice rolls 9589
    w.magic.place = "riverbank";           // wrong place: 7000 bp, fails
    set_count(w.inventories["player"], "fish", 1);
    set_count(w.inventories["player"], "sea_gems", 1);
    SIM_CHECK(learn_rite_from_teacher(w, kSacrifice, kMoonPriest).learned);

    const RiteOutcome o = perform_rite_in_world(w, kSacrifice);
    SIM_CHECK(o.rite.performed);
    SIM_CHECK(!o.rite.succeeded);  // 7000 bp <= roll 9589
    SIM_CHECK_EQ(wrath(w.divine, "player", "the_two_waters"), kFailedRiteWrath);
    return true;
}

static bool test_impure_rite_accrues_wrath_and_place_rites_offend_nobody() {
    WorldState w = day5_world();  // day 5: sacrifice rolls 3493 (8000 bp, succeeds)
    w.magic.purity = kImpureBelow - 1;  // below the floor: impure (this row sets
                                        // no requirement, so the score is unaffected)
    set_count(w.inventories["player"], "fish", 1);
    set_count(w.inventories["player"], "sea_gems", 1);
    SIM_CHECK(learn_rite_from_teacher(w, kSacrifice, kMoonPriest).learned);

    const RiteOutcome o = perform_rite_in_world(w, kSacrifice);
    SIM_CHECK(o.rite.performed);
    SIM_CHECK(o.rite.succeeded);
    SIM_CHECK_EQ(wrath(w.divine, "player", "the_two_waters"), kImpureRiteWrath);

    // A rite that addresses no god (the flour circle wards a place) offends
    // nobody, even failed: the offence is against the deity addressed.
    WorldState ward = day5_world(kFailSeed);
    ward.magic.place = "riverbank";
    set_count(ward.inventories["player"], "zisurru_incantation_tablet", 1);
    SIM_CHECK(learn_rite_from_text(ward, "zisurru_warding", "zisurru_incantation_tablet").learned);
    const RiteOutcome place = perform_rite_in_world(ward, "zisurru_warding");
    SIM_CHECK(place.rite.performed);
    SIM_CHECK(!place.rite.succeeded);  // no flour: 5000 bp <= roll 5312
    SIM_CHECK(ward.divine.wrath_by_offender.empty());
    return true;
}

// A purity_required row (the atonement rite sets 60): impure is below the
// ROW's bar even above the blank-row floor of 50.
static bool test_row_purity_requirement_raises_the_bar() {
    WorldState w = day5_world();
    w.magic.purity = 55;  // above the floor (50), below the atonement row's 60
    set_count(w.inventories["player"], "fish", 1);
    set_count(w.inventories["player"], "sea_gems", 1);
    SIM_CHECK(learn_rite_from_teacher(w, kSacrifice, kMoonPriest).learned);
    SIM_CHECK(learn_rite_from_teacher(w, kAtonementRite, kSunPriest).learned);

    // The sacrifice (no purity_required) at 55 is clean: no wrath.
    const RiteOutcome ok = perform_rite_in_world(w, kSacrifice);
    SIM_CHECK(ok.rite.succeeded);
    SIM_CHECK_EQ(wrath(w.divine, "player", "the_two_waters"), 0);

    // The supplication (purity_required 60) at 55 is impure — and its lower
    // score (6000 bp vs the day's roll 7200) fails it too: both offences.
    const RiteOutcome impure = perform_rite_in_world(w, kAtonementRite, "utu");
    SIM_CHECK(impure.rite.performed);
    SIM_CHECK(!impure.rite.succeeded);
    SIM_CHECK_EQ(wrath(w.divine, "player", "utu"), kImpureRiteWrath + kFailedRiteWrath);
    return true;
}

// --- the consequences: ill omens, favour penalties, the curse -------------------

static bool test_wrath_skews_the_omen() {
    // Twin worlds, one divine book apart: the same omen (barutu about utu,
    // day 5, seed 1 — roll 6813 of 8000 succeeds; the draw reads true) is
    // favourable at neutral wrath and unfavourable at tier 2, because the
    // asker's perceived favour drops by the wrath tier's penalty.
    WorldState clean = day5_world();
    WorldState angry = day5_world();
    SIM_CHECK(swear_and_break(angry, 4));  // 4 x 15 = 60: tier 2 with the witness god
    SIM_CHECK_EQ(wrath(angry.divine, "player", "utu"), 60);

    WorldState* const worlds[] = {&clean, &angry};
    for (WorldState* w : worlds) {
        w->magic.place = "house:diviner";
        set_count(w->inventories["player"], "clay_liver_model", 1);
        set_count(w->inventories["player"], "a_burned_goat's_liver", 1);
        SIM_CHECK(learn_rite_from_text(*w, "barutu_haruspicy", "clay_liver_model").learned);
        const RiteOutcome o = perform_rite_in_world(*w, "barutu_haruspicy", "utu");
        SIM_CHECK(o.rite.succeeded);
        SIM_CHECK_EQ(w->rite_effects.omens.size(), std::size_t{1});
    }
    // Neutral: favour 50 >= 50, the sign reads true -> favourable.
    SIM_CHECK_EQ(clean.rite_effects.omens.front().sign, std::string("favourable"));
    // Tier 2: perceived 50 - 20 < 50 while the sign still reads true -> the
    // ill omen — unfavourable.
    SIM_CHECK_EQ(angry.rite_effects.omens.front().sign, std::string("unfavourable"));
    return true;
}

static bool test_tiers_bank_favour_penalties_until_dawn() {
    WorldState w = day5_world();
    SIM_CHECK(swear_and_break(w, 4));  // 60: tier 2
    SIM_CHECK_EQ(w.divine.pending_favour_penalty.at("utu"), -kTierFavourPenalty);
    SIM_CHECK_EQ(favour(w.magic, "utu"), 50);  // not yet: it lands on the tick

    w.advance_days(1);  // the gods' morning
    SIM_CHECK_EQ(favour(w.magic, "utu"), 50 - kTierFavourPenalty);
    SIM_CHECK(w.divine.pending_favour_penalty.empty());

    // Tier 3 (the 6th break: 90) lays the curse and banks a second penalty.
    SIM_CHECK(swear_and_break(w, 2));
    SIM_CHECK(is_cursed(w.divine, "player"));
    SIM_CHECK_EQ(curse_deity(w.divine, "player"), Id("utu"));
    SIM_CHECK_EQ(w.divine.pending_favour_penalty.at("utu"), -kTierFavourPenalty);
    w.advance_days(1);
    SIM_CHECK_EQ(favour(w.magic, "utu"), 50 - 2 * kTierFavourPenalty);
    return true;
}

// --- row 12: divine-offence convictions ----------------------------------------

static bool test_convictions_accrue_to_the_right_gods() {
    WorldState w;
    w.init("../db/canon", kScenarioSeed);

    // Two tomb robberies (the underworld queen's own dead: "the stones of the
    // dead curse the violator"), one temple sacrilege in the City of the Moon
    // (its patron: nanna, the temple of sun and moon), one plain theft (no
    // god's business).
    (void)commit_crime(w, "player", "tomb_robbery", "city_of_the_moon", {kWitness});
    (void)commit_crime(w, "player", "tomb_robbery", "city_of_the_moon", {kWitness});
    (void)commit_crime(w, "player", "sacrilege", "city_of_the_moon", {kWitness});
    (void)commit_crime(w, "player", "theft", "city_of_the_moon", {kWitness});
    SIM_CHECK(w.divine.wrath_by_offender.empty());  // nothing until the verdict

    // commit_crime schedules the hearing kHearingDelayDays out; the day it
    // falls due, advance_days hears and seals it (Actions.cpp hold_due_hearings).
    w.advance_days(kHearingDelayDays + 1);
    SIM_CHECK_EQ(wrath(w.divine, "player", kTombDeity), 2 * kConvictionWrath);  // 60: tier 2
    SIM_CHECK_EQ(wrath(w.divine, "player", "nanna"), kConvictionWrath);
    SIM_CHECK_EQ(w.divine.wrath_by_offender.at("player").size(), std::size_t{2});  // theft added no god

    // A conviction for oath-breaking as a crime accrues to the oath witness.
    (void)commit_crime(w, "player", "oath_breaking", "city_of_the_moon", {kWitness});
    w.advance_days(kHearingDelayDays + 1);
    SIM_CHECK_EQ(wrath(w.divine, "player", kOathWitnessDeity), kConvictionWrath);
    return true;
}

// --- time and atonement ---------------------------------------------------------

static bool test_wrath_decays_slowly_but_a_curse_holds() {
    // A fresh world (no days ticked yet) so the decay counter starts at 0.
    WorldState w;
    w.init("../db/canon", kScenarioSeed);
    SIM_CHECK(swear_and_break(w, 1));  // 15
    w.advance_days(kWrathDecayDays - 1);
    SIM_CHECK_EQ(wrath(w.divine, "player", "utu"), kOathBreakWrath);  // not yet
    w.advance_days(1);
    SIM_CHECK_EQ(wrath(w.divine, "player", "utu"), kOathBreakWrath - 1);  // one point / N days

    // The cursed offender's wrath with the cursing god does not decay: only
    // atonement lifts a curse. (90 = 6 breaks, curse at tier 3.)
    WorldState cursed;
    cursed.init("../db/canon", kScenarioSeed);
    SIM_CHECK(swear_and_break(cursed, 6));
    SIM_CHECK(is_cursed(cursed.divine, "player"));
    cursed.advance_days(4 * kWrathDecayDays);
    SIM_CHECK_EQ(wrath(cursed.divine, "player", "utu"), 6 * kOathBreakWrath);
    SIM_CHECK(is_cursed(cursed.divine, "player"));  // time never clears it
    return true;
}

static bool test_atonement_drops_wrath_and_lifts_the_curse() {
    WorldState w = day5_world();
    // Unlearned: refused, nothing offends, nothing atones.
    const RiteOutcome refused = perform_rite_in_world(w, kAtonementRite, "utu");
    SIM_CHECK(!refused.rite.performed);
    SIM_CHECK(w.divine.wrath_by_offender.empty());

    SIM_CHECK(learn_rite_from_teacher(w, kAtonementRite, kSunPriest).learned);
    SIM_CHECK(swear_and_break(w, 6));  // 90 with utu, tier 3: the curse
    SIM_CHECK(is_cursed(w.divine, "player"));

    // Day 5, seed 1: the supplication rolls 7200 against a clean 8000 bp.
    const RiteOutcome o = perform_rite_in_world(w, kAtonementRite, "utu");
    SIM_CHECK(o.rite.performed);
    SIM_CHECK(o.rite.succeeded);
    SIM_CHECK_EQ(wrath(w.divine, "player", "utu"), 6 * kOathBreakWrath - kAtonementWrathDrop);
    SIM_CHECK(!is_cursed(w.divine, "player"));  // below tier 3: lifted
    SIM_CHECK_EQ(favour(w.magic, "utu"),
                 50 + 2 + kHymnFavour + kAtonementFavour);  // +2 performed, +3 hymn, +5 appeased
    SIM_CHECK_EQ(o.effect,
                 "favour:utu:+" + std::to_string(kHymnFavour) + ";atonement:utu:" +
                     std::to_string(6 * kOathBreakWrath) + "->" +
                     std::to_string(6 * kOathBreakWrath - kAtonementWrathDrop));

    // An atonement with nothing to atone is a no-op beyond the hymn's favour.
    const RiteOutcome again = perform_rite_in_world(w, kAtonementRite, "nanna");
    SIM_CHECK(again.rite.succeeded);
    SIM_CHECK(again.effect.rfind("atonement:", 0) != 0);
    return true;
}

// --- save/load and determinism ---------------------------------------------------

static bool test_divine_state_saves_and_loads() {
    WorldState w = day5_world();
    SIM_CHECK(swear_and_break(w, 6));  // 90, cursed, two penalties banked
    (void)commit_crime(w, "player", "sacrilege", "city_of_the_moon", {kWitness});
    w.advance_days(kHearingDelayDays + 1);  // nanna 30; the penalties land
    const std::string saved = save_world(w);
    SIM_CHECK(saved.find("DIVINE_WRATH") != std::string::npos);

    WorldState r;
    load_world(r, "../db/canon", saved);
    SIM_CHECK_EQ(divine_bytes(r.divine), divine_bytes(w.divine));
    SIM_CHECK(is_cursed(r.divine, "player"));
    SIM_CHECK_EQ(save_world(r), saved);  // round trip: byte-identical

    // A pre-W5 save (the DIVINE_* sections stripped) still loads: fresh books.
    std::string old = saved;
    const std::size_t cut = old.find("DIVINE_WRATH");
    SIM_CHECK(cut != std::string::npos);
    old.erase(cut);
    WorldState pre;
    load_world(pre, "../db/canon", old);
    SIM_CHECK(pre.divine.wrath_by_offender.empty());
    SIM_CHECK(!is_cursed(pre.divine, "player"));
    return true;
}

static bool test_divine_wrath_is_deterministic() {
    const auto run = [](WorldState& w) {
        w.advance_days(4);
        w.magic.place = "temple:city_of_the_moon";
        (void)swear_and_break(w, 5);
        w.magic.purity = 40;  // an impure sacrifice
        set_count(w.inventories["player"], "fish", 1);
        set_count(w.inventories["player"], "sea_gems", 1);
        (void)learn_rite_from_teacher(w, kSacrifice, kMoonPriest);
        (void)perform_rite_in_world(w, kSacrifice);
        (void)commit_crime(w, "player", "tomb_robbery", "city_of_the_moon", {kWitness});
        w.advance_days(kHearingDelayDays + 2);
        (void)perform_rite_in_world(w, kAtonementRite, "utu");
    };
    WorldState a;
    a.init("../db/canon", kDeterminismSeed);
    WorldState b;
    b.init("../db/canon", kDeterminismSeed);
    run(a);
    run(b);
    SIM_CHECK_EQ(divine_bytes(a.divine), divine_bytes(b.divine));
    SIM_CHECK_EQ(save_world(a), save_world(b));
    return true;
}

// --- the C boundary ----------------------------------------------------------------

static bool test_c_api_divine() {
    SIM_CHECK_EQ(sim_world_divine_wrath(nullptr, "player", "utu"), -1);
    SIM_CHECK_EQ(sim_world_divine_entry_count(nullptr), -1);

    SimWorld* w = sim_world_create("../db/canon", kScenarioSeed);
    if (!w) return false;
    SIM_CHECK_EQ(sim_world_divine_wrath(w, "player", "utu"), 0);
    SIM_CHECK_EQ(sim_world_divine_tier(w, "player", "utu"), 0);
    SIM_CHECK_EQ(sim_world_divine_cursed(w, "player", nullptr, 0), 0);
    SIM_CHECK_EQ(sim_world_divine_entry_count(w), 0);
    SIM_CHECK_EQ(sim_world_divine_entry(w, 0, nullptr, 0), -1);
    SIM_CHECK_EQ(sim_world_break_oath(w, "no_such_oath"), -2);
    SIM_CHECK_EQ(sim_world_break_oath(nullptr, "oath_1"), -1);

    const Oath* oath = swear(w->world.faction, "player", "the_empire", w->world.day);
    SIM_CHECK(oath != nullptr);
    SIM_CHECK_EQ(sim_world_break_oath(w, oath->id.c_str()), 0);
    SIM_CHECK_EQ(sim_world_break_oath(w, oath->id.c_str()), -3);  // already broken
    SIM_CHECK_EQ(sim_world_divine_wrath(w, "player", "utu"), kOathBreakWrath);
    SIM_CHECK_EQ(sim_world_divine_tier(w, "player", "utu"), 0);
    SIM_CHECK_EQ(sim_world_divine_entry_count(w), 1);
    char entry[64];
    SIM_CHECK(sim_world_divine_entry(w, 0, entry, sizeof(entry)) > 0);
    SIM_CHECK(std::strcmp(entry,
                          (std::string("player;utu;") + std::to_string(kOathBreakWrath)).c_str()) == 0);
    sim_world_destroy(w);
    return true;
}

static bool test_c_api_divine_curse_name() {
    SimWorld* w = sim_world_create("../db/canon", kScenarioSeed);
    if (!w) return false;
    for (int i = 0; i < 6; ++i) {
        const Oath* oath = swear(w->world.faction, "player", "the_empire", w->world.day);
        SIM_CHECK(oath != nullptr);
        break_oath_in_world(w->world, oath->id);
    }
    SIM_CHECK_EQ(sim_world_divine_tier(w, "player", "utu"), 3);
    char who[16];
    SIM_CHECK_EQ(sim_world_divine_cursed(w, "player", who, sizeof(who)), 1);
    SIM_CHECK(std::strcmp(who, "utu") == 0);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_wrath_reads_zero_and_tier_ladder,
         test_oath_break_accrues_wrath_with_the_witness,
         test_failed_rite_accrues_wrath,
         test_impure_rite_accrues_wrath_and_place_rites_offend_nobody,
         test_row_purity_requirement_raises_the_bar,
         test_wrath_skews_the_omen,
         test_tiers_bank_favour_penalties_until_dawn,
         test_convictions_accrue_to_the_right_gods,
         test_wrath_decays_slowly_but_a_curse_holds,
         test_atonement_drops_wrath_and_lifts_the_curse,
         test_divine_state_saves_and_loads,
         test_divine_wrath_is_deterministic,
         test_c_api_divine,
         test_c_api_divine_curse_name)
