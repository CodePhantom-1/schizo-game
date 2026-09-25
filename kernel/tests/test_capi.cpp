// test_capi.cpp — the C boundary the engine will live behind: everything the
// SimRuntime plugin needs must work through extern "C" alone.
#include "sim/CApi.h"

#include "sim/Test.hpp"

#include <cstdio>
#include <cstring>
#include <string>

static bool test_lifecycle_and_clock() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    if (sim_world_day(w) != 1) { sim_world_destroy(w); return false; }
    sim_world_advance_days(w, 30);
    if (sim_world_day(w) != 31) { sim_world_destroy(w); return false; }
    char season[16];
    const int len = sim_world_season(w, season, sizeof(season));
    if (len < 0) { sim_world_destroy(w); return false; }
    if (std::strcmp(season, "rains") != 0 && std::strcmp(season, "sowing") != 0 &&
        std::strcmp(season, "harvest") != 0 && std::strcmp(season, "vintage") != 0) {
        std::fprintf(stderr, "FAIL unknown season id '%s'\n", season);
        sim_world_destroy(w);
        return false;
    }
    sim_world_destroy(w);
    return true;
}

static bool test_prices_and_the_drought_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    sim_world_advance_days(w, 60);
    const int64_t before = sim_world_price(w, "city_of_the_moon", "grain");
    sim_world_set_drought(w, 3);
    sim_world_advance_days(w, 60);
    const int64_t after = sim_world_price(w, "city_of_the_moon", "grain");
    const bool ok = after > before;
    sim_world_destroy(w);
    return ok;
}

static bool test_standing_and_favour_clamps() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    sim_world_add_favour(w, "inanna", 200);
    const int fav = sim_world_favour(w, "inanna");
    sim_world_add_standing(w, "the_empire", -500);
    const int st = sim_world_standing(w, "the_empire");
    const bool ok = fav == 100 && st == 0;
    sim_world_destroy(w);
    return ok;
}

static bool test_two_worlds_same_seed_agree() {
    SimWorld* a = sim_world_create("../db/canon", 9);
    SimWorld* b = sim_world_create("../db/canon", 9);
    if (!a || !b) { sim_world_destroy(a); sim_world_destroy(b); return false; }
    sim_world_advance_days(a, 120);
    sim_world_advance_days(b, 120);
    const bool ok = sim_world_price(a, "city_of_jewels", "grain") ==
                    sim_world_price(b, "city_of_jewels", "grain");
    sim_world_destroy(a);
    sim_world_destroy(b);
    return ok;
}

static bool test_needs_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    bool ok = sim_world_hunger(w, "player") == 0 && sim_world_thirst(w, "player") == 0 &&
              sim_world_fatigue(w, "player") == 0;
    sim_world_advance_needs(w, "player", 5, 0);
    ok = ok && sim_world_hunger(w, "player") == 10 && sim_world_thirst(w, "player") == 15;
    sim_world_advance_needs(w, "player", 20, 1);  // sleeping: fatigue restores
    ok = ok && sim_world_fatigue(w, "player") == 0;
    char eff[64];
    int len = sim_world_need_effects(w, "player", eff, sizeof(eff));
    ok = ok && len >= 0;  // just exercises the buffer path; thresholds not yet crossed
    sim_world_destroy(w);
    return ok;
}

static bool test_inventory_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    bool ok = sim_world_item_count(w, "player", "grain") == 0;
    ok = ok && sim_world_give_item(w, "player", "grain", 3) == 3;
    ok = ok && sim_world_item_count(w, "player", "grain") == 3;
    ok = ok && sim_world_give_item(w, "player", "grain", -10) == 0;  // clamps at 0
    ok = ok && sim_world_item_count(w, "player", "grain") == 0;
    sim_world_destroy(w);
    return ok;
}

static bool test_eat_drink_error_codes_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    bool ok = sim_world_eat(w, "player", "grain") == -2;  // none in inventory yet
    ok = ok && sim_world_drink(w, "player", "water") == 0;  // virtual water: no inventory needed
    ok = ok && sim_world_hunger(w, "player") == 0 && sim_world_thirst(w, "player") == 0;
    sim_world_give_item(w, "player", "silver", 5);
    ok = ok && sim_world_eat(w, "player", "silver") == -3;  // not a food category
    ok = ok && sim_world_item_count(w, "player", "silver") == 5;  // untouched on refusal
    sim_world_destroy(w);
    return ok;
}

// The grain -> flour -> bread crafting chain, then eating the bread, all
// through the C boundary alone.
static bool test_craft_chain_and_eat_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    // Water is free well-water infrastructure (Crafting.cpp): never given
    // here, and bake_bread's water input is drawn without touching the
    // inventory.
    bool ok = sim_world_give_item(w, "player", "grain", 2) == 2;

    // 2 grain -> 2 barley_flour (quern)
    ok = ok && sim_world_craft(w, "player", "mill_flour", "quern", 2) == 0;
    ok = ok && sim_world_item_count(w, "player", "grain") == 0;
    ok = ok && sim_world_item_count(w, "player", "barley_flour") == 2;

    // missing station: no station list given
    ok = ok && sim_world_craft(w, "player", "bake_bread", "", 1) == -3;

    // 2 barley_flour (+ free water) -> 1 bread (oven)
    ok = ok && sim_world_craft(w, "player", "bake_bread", "oven", 1) == 0;
    ok = ok && sim_world_item_count(w, "player", "bread") == 1;

    // unknown recipe
    ok = ok && sim_world_craft(w, "player", "no_such_recipe", "oven", 1) == -2;

    sim_world_advance_needs(w, "player", 10, 0);  // work up an appetite first
    const int hunger_before = sim_world_hunger(w, "player");
    ok = ok && sim_world_eat(w, "player", "bread") == 0;
    ok = ok && sim_world_hunger(w, "player") < hunger_before;
    ok = ok && sim_world_item_count(w, "player", "bread") == 0;

    sim_world_destroy(w);
    return ok;
}

static bool test_task_at_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    char task[128];
    // Just exercises the buffer path for every known role; no assumption
    // about which roles have schedule rows (that's canon content, not this
    // seam's concern) beyond: a bad role/hour never crashes and returns -1.
    const int len = sim_world_task_at(w, "definitely_not_a_role", 6, task, sizeof(task));
    const bool ok = len == -1;
    sim_world_destroy(w);
    return ok;
}

static bool test_save_load_continue_through_c() {
    SimWorld* a = sim_world_create("../db/canon", 7);
    if (!a) return false;
    sim_world_advance_days(a, 50);
    sim_world_give_item(a, "player", "grain", 4);
    sim_world_advance_needs(a, "player", 10, 0);
    sim_world_set_drought(a, 2);

    const char* path = "/tmp/claude-1000/sim_capi_test_save.txt";
    bool ok = sim_world_save(a, path) == 0;

    SimWorld* b = sim_world_load("../db/canon", path);
    ok = ok && b != nullptr;
    ok = ok && sim_world_day(b) == sim_world_day(a);
    ok = ok && sim_world_item_count(b, "player", "grain") == 4;
    ok = ok && sim_world_hunger(b, "player") == sim_world_hunger(a, "player");
    ok = ok && sim_world_drought(b) == 2;

    // Continue both forward identically (determinism through the C API).
    sim_world_advance_days(a, 75);
    sim_world_advance_days(b, 75);
    ok = ok && sim_world_price(a, "city_of_the_moon", "grain") ==
                   sim_world_price(b, "city_of_the_moon", "grain");
    ok = ok && sim_world_day(a) == sim_world_day(b);

    // Buffer round-trip too.
    char buf[1 << 16];
    const int len = sim_world_save_to_buffer(a, buf, sizeof(buf));
    ok = ok && len > 0 && len < static_cast<int>(sizeof(buf));
    SimWorld* c = sim_world_load_from_buffer("../db/canon", buf);
    ok = ok && c != nullptr && sim_world_day(c) == sim_world_day(a);

    sim_world_destroy(a);
    sim_world_destroy(b);
    sim_world_destroy(c);
    return ok;
}

static bool test_save_load_null_and_bad_path() {
    bool ok = sim_world_save(nullptr, "/tmp/x") == -1;
    ok = ok && sim_world_load(nullptr, "/tmp/x") == nullptr;
    ok = ok && sim_world_load("../db/canon", "/no/such/dir/no_such_file.txt") == nullptr;
    return ok;
}

// K-1: the magic loop through extern "C" alone — learn, offer, perform,
// read the effect, save/load it.
static bool test_rites_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 1);
    if (!w) return false;
    sim_world_advance_days(w, 4);  // day 5 (the verified draw 0.099786)
    char buf[256];

    bool ok = sim_world_knows_rite(w, "sacrifice_fish_sea_gems") == 0;
    ok = ok && sim_world_perform_rite(w, "sacrifice_fish_sea_gems", nullptr, buf, sizeof(buf)) == -3;
    ok = ok && sim_world_perform_rite(w, "no_such_rite", nullptr, nullptr, 0) == -2;

    ok = ok && sim_world_rites_taught_by(w, "lu_dingira_priest_moon", buf, sizeof(buf)) > 0 &&
         std::strcmp(buf, "hymns_deity_names;sacrifice_fish_sea_gems") == 0;
    ok = ok && sim_world_learn_rite_from_teacher(w, "sacrifice_fish_sea_gems", "ea_nasir_grain_trader") == -4;
    ok = ok && sim_world_learn_rite_from_teacher(w, "sacrifice_fish_sea_gems", "nobody") == -3;
    ok = ok && sim_world_learn_rite_from_teacher(w, "sacrifice_fish_sea_gems", "lu_dingira_priest_moon") == 0;
    ok = ok && sim_world_learn_rite_from_teacher(w, "sacrifice_fish_sea_gems", "lu_dingira_priest_moon") == 1;
    ok = ok && sim_world_knows_rite(w, "sacrifice_fish_sea_gems") == 1;

    sim_world_set_rite_place(w, "temple:city_of_the_moon");
    ok = ok && sim_world_rite_place(w, buf, sizeof(buf)) > 0 &&
         std::strcmp(buf, "temple:city_of_the_moon") == 0;
    sim_world_give_item(w, "player", "fish", 1);
    sim_world_give_item(w, "player", "sea_gems", 1);
    ok = ok && sim_world_perform_rite(w, "sacrifice_fish_sea_gems", nullptr, buf, sizeof(buf)) == 1;
    ok = ok && std::strcmp(buf, "favour:the_two_waters:+5") == 0;
    ok = ok && sim_world_item_count(w, "player", "fish") == 0;
    ok = ok && sim_world_item_count(w, "player", "sea_gems") == 0;
    ok = ok && sim_world_favour(w, "the_two_waters") == 57;

    // Text learning and a ward.
    ok = ok && sim_world_learn_rite_from_text(w, "zisurru_warding", "zisurru_incantation_tablet") == -5;
    sim_world_give_item(w, "player", "zisurru_incantation_tablet", 1);
    ok = ok && sim_world_rites_taught_in(w, "zisurru_incantation_tablet", buf, sizeof(buf)) > 0 &&
         std::strcmp(buf, "zisurru_warding") == 0;
    ok = ok && sim_world_learn_rite_from_text(w, "zisurru_warding", "zisurru_incantation_tablet") == 0;
    sim_world_give_item(w, "player", "different_types_of_flour", 1);
    sim_world_set_rite_place(w, "household:player");
    ok = ok && sim_world_perform_rite(w, "zisurru_warding", "", buf, sizeof(buf)) == 1;
    ok = ok && std::strcmp(buf, "ward:household:player:until=34") == 0;
    ok = ok && sim_world_ward_until(w, "household:player") == 34;
    ok = ok && sim_world_warded(w, "household:player") == 1;
    ok = ok && sim_world_ward_until(w, "nowhere") == 0;

    // An "any" rite with no god addressed is refused, nothing spent.
    ok = ok && sim_world_learn_rite_from_teacher(w, "hymns_deity_names", "lu_dingira_priest_moon") == 0;
    ok = ok && sim_world_perform_rite(w, "hymns_deity_names", nullptr, buf, sizeof(buf)) == -4;
    ok = ok && sim_world_perform_rite(w, "hymns_deity_names", "not_a_god", buf, sizeof(buf)) == -5;

    // An omen.
    sim_world_give_item(w, "player", "clay_liver_model", 1);
    sim_world_give_item(w, "player", "a_burned_goat's_liver", 1);
    ok = ok && sim_world_learn_rite_from_text(w, "barutu_haruspicy", "clay_liver_model") == 0;
    sim_world_set_rite_place(w, "house:diviner");
    ok = ok && sim_world_omen_count(w) == 0;
    ok = ok && sim_world_perform_rite(w, "barutu_haruspicy", "the_two_waters", buf, sizeof(buf)) == 1;
    ok = ok && sim_world_omen_count(w) == 1;
    ok = ok && sim_world_omen(w, 0, buf, sizeof(buf)) > 0 &&
         std::strncmp(buf, "5;barutu_haruspicy;the_two_waters;", 34) == 0;
    ok = ok && sim_world_omen(w, 1, buf, sizeof(buf)) == -1;

    ok = ok && sim_world_known_rites(w, buf, sizeof(buf)) > 0 &&
         std::strcmp(buf, "barutu_haruspicy;hymns_deity_names;sacrifice_fish_sea_gems;zisurru_warding") == 0;

    // Purity clamps.
    sim_world_set_purity(w, 140);
    ok = ok && sim_world_purity(w) == 100;
    sim_world_set_purity(w, -3);
    ok = ok && sim_world_purity(w) == 0;

    // Buffer save/load carries all of it.
    static char save[1 << 16];
    const int len = sim_world_save_to_buffer(w, save, sizeof(save));
    ok = ok && len > 0 && len < static_cast<int>(sizeof(save));
    SimWorld* r = sim_world_load_from_buffer("../db/canon", save);
    ok = ok && r != nullptr;
    ok = ok && sim_world_knows_rite(r, "zisurru_warding") == 1;
    ok = ok && sim_world_ward_until(r, "household:player") == 34;
    ok = ok && sim_world_omen_count(r) == 1;
    ok = ok && sim_world_purity(r) == 0;

    // Nulls.
    ok = ok && sim_world_knows_rite(nullptr, "x") == -1;
    ok = ok && sim_world_perform_rite(nullptr, "x", nullptr, nullptr, 0) == -1;
    ok = ok && sim_world_learn_rite_from_text(w, nullptr, "x") == -1;
    ok = ok && sim_world_omen_count(nullptr) == -1;

    sim_world_destroy(r);
    sim_world_destroy(w);
    return ok;
}

SIM_MAIN(test_rites_through_c,
         test_lifecycle_and_clock,
         test_prices_and_the_drought_through_c,
         test_standing_and_favour_clamps,
         test_two_worlds_same_seed_agree,
         test_needs_through_c,
         test_inventory_through_c,
         test_eat_drink_error_codes_through_c,
         test_craft_chain_and_eat_through_c,
         test_task_at_through_c,
         test_save_load_continue_through_c,
         test_save_load_null_and_bad_path)
