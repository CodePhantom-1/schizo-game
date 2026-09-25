// test_scenario_rite_action.cpp — K-1: perform_rite_action (sim/Actions.hpp),
// the engine-facing "one rite, start to finish" verb that scenario_rite.md's
// gaps 1/2/5 ask for: durable knowledge (sim/Magic.hpp's learn_rite/
// knows_rite), material consumption from a real WorldState::inventories
// entry (no partial consumption on refusal), and effect application by
// family (protection -> a ward, divination -> an omen, healing/purification
// -> Needs relief/purity restore, curse -> a self-cost ward). One canon rite
// per family (db/canon/rites.csv): sacrifice_fish_sea_gems (offering/
// favour, the scenario's original pick, untouched by this track), the K-1
// additions zisurru_warding (protection, now purity-gated) and
// barutu_haruspicy (divination, now purity- and festival-gated), and the new
// gula_healing_rite (healing and purification). Curse/binding has no canon
// rite yet (scenario_rite.md: "not exercised, out of scope"), so it is
// exercised here against a small fixture canon dir, the same pattern
// test_magic.cpp uses for the effect families the real table doesn't cover.
//
// Rng draw reused throughout (verified in test_magic.cpp's own header):
// Rng{1}.fork(5).unit() = 0.099786 — under every 0.60+ score below, so every
// "full offering" attempt in this file succeeds by construction, not luck.
#include "sim/Actions.hpp"

#include "sim/Test.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using namespace sim;

namespace {

constexpr std::uint64_t kSeed = 1;
const char* kPlayer = "player";

// Lands a fresh world on day 5, the pinned draw's day.
WorldState fresh_world() {
    WorldState w;
    w.init("../db/canon", kSeed);
    w.advance_days(4);
    return w;
}

}  // namespace

// --- refusal: no inventory entry is ever created, nothing consumed ---------
// (WorldState::init seeds "player" with an empty Inventory already — the
// prisoner start, D-009 — so this uses an actor init never touches.)

static bool test_refusal_creates_no_inventory_entry() {
    WorldState w = fresh_world();
    const char* fresh_actor = "the_prophet";
    SIM_CHECK(w.inventories.find(fresh_actor) == w.inventories.end());

    const RiteResult unknown = perform_rite_action(w, fresh_actor, "no_such_rite", "riverbank");
    SIM_CHECK(!unknown.performed);
    SIM_CHECK_EQ(unknown.refusal_reason, std::string("unknown_rite"));
    SIM_CHECK(w.inventories.find(fresh_actor) == w.inventories.end());  // still nothing
    SIM_CHECK(w.magic.favour_by_deity.empty());

    const RiteResult not_known =
        perform_rite_action(w, fresh_actor, "sacrifice_fish_sea_gems", "temple:city_of_the_moon");
    SIM_CHECK(!not_known.performed);
    SIM_CHECK_EQ(not_known.refusal_reason, std::string("rite_not_known"));
    SIM_CHECK(w.inventories.find(fresh_actor) == w.inventories.end());  // a refusal never touches it
    return true;
}

// A performer who already HOLDS the materials but never learned the rite is
// still refused, and those materials are provably untouched afterward.
static bool test_refusal_leaves_held_materials_untouched() {
    WorldState w = fresh_world();
    w.inventories[kPlayer].counts["fish"] = 2;
    w.inventories[kPlayer].counts["sea_gems"] = 1;

    const RiteResult r =
        perform_rite_action(w, kPlayer, "sacrifice_fish_sea_gems", "temple:city_of_the_moon");
    SIM_CHECK(!r.performed);
    SIM_CHECK_EQ(w.inventories[kPlayer].counts["fish"], 2);
    SIM_CHECK_EQ(w.inventories[kPlayer].counts["sea_gems"], 1);
    return true;
}

// --- the offering rite: learn -> materials consumed (presence-only: -1 ----
// each, not to zero) -> favour raised -----------------------------------

static bool test_offering_rite_learns_consumes_and_raises_favour() {
    WorldState w = fresh_world();
    learn_rite(w.magic, w.db, "sacrifice_fish_sea_gems");
    SIM_CHECK(knows_rite(w.magic, "sacrifice_fish_sea_gems"));
    w.inventories[kPlayer].counts["fish"] = 2;
    w.inventories[kPlayer].counts["sea_gems"] = 1;

    const RiteResult r =
        perform_rite_action(w, kPlayer, "sacrifice_fish_sea_gems", "temple:city_of_the_moon");
    SIM_CHECK(r.performed);
    SIM_CHECK(r.succeeded);
    SIM_CHECK_EQ(favour(w.magic, "the_two_waters"), 52);  // +2 performed, succeeded

    // Presence-only materials: exactly 1 unit of each consumed.
    SIM_CHECK_EQ(w.inventories[kPlayer].counts["fish"], 1);
    SIM_CHECK(w.inventories[kPlayer].counts.find("sea_gems") == w.inventories[kPlayer].counts.end());

    // "offering (favour)" hits no ward/omen/needs branch: nothing else moves.
    SIM_CHECK(w.magic.active_wards.empty());
    SIM_CHECK(w.magic.omens.empty());
    return true;
}

// A performed-but-refused-on-knowledge attempt never reaches the consumption
// step at all (already covered above); this checks the OTHER refusal path
// (unknown rite id) behaves identically when materials are on hand too.
static bool test_unknown_rite_refusal_with_materials_held() {
    WorldState w = fresh_world();
    w.inventories[kPlayer].counts["fish"] = 2;
    const RiteResult r = perform_rite_action(w, kPlayer, "no_such_rite", "riverbank");
    SIM_CHECK(!r.performed);
    SIM_CHECK_EQ(w.inventories[kPlayer].counts["fish"], 2);
    return true;
}

// --- protection: zisurru_warding, now purity-gated (rites.csv) ------------

static bool test_protection_rite_creates_an_expiring_ward() {
    WorldState w = fresh_world();
    learn_rite(w.magic, w.db, "zisurru_warding");
    w.inventories[kPlayer].counts["different_types_of_flour"] = 1;
    w.magic.purity = 100;  // >= purity_required (40): full score

    const RiteResult r = perform_rite_action(w, kPlayer, "zisurru_warding", "household:city_of_the_moon");
    SIM_CHECK(r.performed);
    SIM_CHECK(r.succeeded);
    SIM_CHECK_EQ(r.effect_family, std::string("protection"));

    SIM_CHECK(has_active_ward(w.magic, kPlayer, w.day));       // active the day it's cast
    SIM_CHECK(!has_active_ward(w.magic, kPlayer, w.day + 7));  // kWardDurationDays later: expired

    const std::vector<Ward> active = active_wards(w.magic, w.day);
    SIM_CHECK_EQ(active.size(), std::size_t{1});
    SIM_CHECK_EQ(active.front().kind, std::string("protection"));
    SIM_CHECK_EQ(active.front().rite_id, std::string("zisurru_warding"));
    return true;
}

// Purity is a SCORE input (rpg-systems §10.1 power 4), not a hard refusal —
// below the requirement it costs exactly the documented 0.20, same shape as
// test_magic.cpp's own place/materials checks.
static bool test_protection_rite_purity_gating_moves_the_score() {
    WorldState pure = fresh_world();
    learn_rite(pure.magic, pure.db, "zisurru_warding");
    pure.inventories[kPlayer].counts["different_types_of_flour"] = 1;
    pure.magic.purity = 100;
    const RiteResult ok = perform_rite_action(pure, kPlayer, "zisurru_warding", "household:x");

    WorldState impure = fresh_world();
    learn_rite(impure.magic, impure.db, "zisurru_warding");
    impure.inventories[kPlayer].counts["different_types_of_flour"] = 1;
    impure.magic.purity = 0;  // below purity_required=40
    const RiteResult low = perform_rite_action(impure, kPlayer, "zisurru_warding", "household:x");

    SIM_CHECK(ok.performed && low.performed);
    SIM_CHECK((ok.score - low.score > 0.19) && (ok.score - low.score < 0.21));  // the 0.20 purity term
    return true;
}

// --- divination: barutu_haruspicy, now purity- AND festival-gated --------

static bool test_divination_rite_creates_an_omen_on_a_festival_day() {
    WorldState w = fresh_world();  // day 5
    // World::init does not yet wire calendar.csv's festival_days prose into
    // CalendarConfig (a gap this track does not own — see the K-1 report);
    // this test overrides the world's own (public, caller-set) Calendar the
    // same way tests already set w.magic.place, so the festival branch this
    // rite's time_window now demands is provably exercised.
    CalendarConfig cfg;
    cfg.festival_days = {w.day};
    w.cal = Calendar{cfg};

    learn_rite(w.magic, w.db, "barutu_haruspicy");
    w.inventories[kPlayer].counts["a_burned_goat's_liver"] = 1;
    w.magic.purity = 30;  // exactly purity_required

    const RiteResult r = perform_rite_action(w, kPlayer, "barutu_haruspicy", "house:city_of_the_moon");
    SIM_CHECK(r.performed);
    SIM_CHECK(r.succeeded);
    SIM_CHECK_EQ(r.effect_family, std::string("divination (omens with probabilities)"));
    SIM_CHECK_EQ(w.magic.omens.size(), std::size_t{1});
    SIM_CHECK_EQ(w.magic.omens.front().reading, std::string("favourable"));  // score 0.80 >= 0.70
    SIM_CHECK(w.magic.omens.front().probability > 0.79 && w.magic.omens.front().probability < 0.81);

    // Off a festival day, the same attempt loses the 0.10 time term.
    WorldState off = fresh_world();
    learn_rite(off.magic, off.db, "barutu_haruspicy");
    off.inventories[kPlayer].counts["a_burned_goat's_liver"] = 1;
    off.magic.purity = 30;
    const RiteResult r_off =
        perform_rite_action(off, kPlayer, "barutu_haruspicy", "house:city_of_the_moon");
    SIM_CHECK(r_off.performed);
    SIM_CHECK((r.score - r_off.score > 0.09) && (r.score - r_off.score < 0.11));
    return true;
}

// --- healing and purification: the new gula_healing_rite -------------------

static bool test_healing_rite_relieves_needs_and_restores_purity() {
    WorldState w = fresh_world();
    learn_rite(w.magic, w.db, "gula_healing_rite");
    w.inventories[kPlayer].counts["herbs"] = 1;
    w.inventories[kPlayer].counts["clean_water"] = 1;
    w.inventories[kPlayer].counts["a_lamb"] = 1;
    w.magic.purity = 50;  // >= purity_required (20), room to show the boost

    Needs& n = needs_of(w.needs, kPlayer);
    n.hunger = 50;
    n.thirst = 50;
    n.fatigue = 50;

    const RiteResult r = perform_rite_action(w, kPlayer, "gula_healing_rite", "temple:city_of_the_moon");
    SIM_CHECK(r.performed);
    SIM_CHECK(r.succeeded);

    const Needs& after = needs_of(w.needs, kPlayer);
    SIM_CHECK_EQ(after.hunger, 20);
    SIM_CHECK_EQ(after.thirst, 20);
    SIM_CHECK_EQ(after.fatigue, 20);
    SIM_CHECK_EQ(w.magic.purity, 80);  // 50 + kPurificationBoost(30)

    SIM_CHECK(w.inventories[kPlayer].counts.find("herbs") == w.inventories[kPlayer].counts.end());
    SIM_CHECK(w.inventories[kPlayer].counts.find("a_lamb") == w.inventories[kPlayer].counts.end());
    return true;
}

// --- curse and binding: no canon rite yet, exercised against a fixture -----
// (test scaffolding only, same convention as test_magic.cpp's TempCanon —
// "not canon and ships nowhere").

namespace {
struct TempCurseCanon {
    std::filesystem::path dir;
    explicit TempCurseCanon()
        : dir(std::filesystem::temp_directory_path() / "sim_test_scenario_rite_action_curse") {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        std::ofstream out(dir / "rites.csv");
        out << "id,name,tradition,deity,materials,place,time_window,purity_required,effect_family,"
               "effect_tag,tag,source_ref\n"
            << "fixture_curse,Fixture curse (test scaffolding),fixture,any,black wool,,,,curse and "
               "binding,INVENTED: EFFECT,CANON,tests/test_scenario_rite_action.cpp\n";
    }
    ~TempCurseCanon() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    std::string str() const { return dir.string(); }
};
}  // namespace

static bool test_curse_rite_wards_and_costs_purity() {
    TempCurseCanon canon;
    WorldState w;
    w.init(canon.str(), kSeed);
    w.advance_days(4);  // day 5, the pinned draw

    learn_rite(w.magic, w.db, "fixture_curse");
    w.inventories[kPlayer].counts["black_wool"] = 1;
    w.magic.purity = 100;

    const RiteResult r = perform_rite_action(w, kPlayer, "fixture_curse", "riverbank");
    SIM_CHECK(r.performed);
    SIM_CHECK(r.succeeded);  // deity "any": favour 50 -> 0.20, materials/purity/time all satisfied -> 0.80

    SIM_CHECK(has_active_ward(w.magic, kPlayer, w.day));
    SIM_CHECK_EQ(active_wards(w.magic, w.day).front().kind, std::string("curse"));
    SIM_CHECK_EQ(w.magic.purity, 100 - kCursePurityCost);  // the price of forbidden magic
    return true;
}

// --- determinism: two identically-seeded worlds run the same script --------

static bool test_determinism_same_seed_same_bytes() {
    auto run = [](WorldState& w) {
        w.init("../db/canon", 777);
        w.advance_days(4);
        learn_rite(w.magic, w.db, "zisurru_warding");
        w.inventories[kPlayer].counts["different_types_of_flour"] = 1;
        return perform_rite_action(w, kPlayer, "zisurru_warding", "household:x");
    };

    WorldState a, b;
    const RiteResult ra = run(a);
    const RiteResult rb = run(b);

    SIM_CHECK_EQ(ra.performed, rb.performed);
    SIM_CHECK_EQ(ra.succeeded, rb.succeeded);
    SIM_CHECK(ra.score == rb.score);
    SIM_CHECK_EQ(a.magic.active_wards.size(), b.magic.active_wards.size());
    SIM_CHECK_EQ(a.magic.active_wards.front().id, b.magic.active_wards.front().id);
    SIM_CHECK_EQ(a.magic.active_wards.front().expires_day, b.magic.active_wards.front().expires_day);
    return true;
}

SIM_MAIN(test_refusal_creates_no_inventory_entry,
         test_refusal_leaves_held_materials_untouched,
         test_offering_rite_learns_consumes_and_raises_favour,
         test_unknown_rite_refusal_with_materials_held,
         test_protection_rite_creates_an_expiring_ward,
         test_protection_rite_purity_gating_moves_the_score,
         test_divination_rite_creates_an_omen_on_a_festival_day,
         test_healing_rite_relieves_needs_and_restores_purity,
         test_curse_rite_wards_and_costs_purity,
         test_determinism_same_seed_same_bytes)
