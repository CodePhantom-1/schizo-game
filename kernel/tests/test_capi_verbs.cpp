// test_capi_verbs.cpp — W6-A: the C ABI the player's engine verbs read
// (inventory enumeration, best carried food/drink) over db/canon's own items.
#include "sim/CApi.h"
#include "sim/CApiVerbs.h"

#include "sim/Test.hpp"
#include <cstring>

static bool test_inventory_empty_then_filled() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);

    // An actor the world has never seen: empty, not an error.
    char buf[256];
    SIM_CHECK_EQ(sim_world_inventory(w, "player", buf, sizeof(buf)), 0);
    SIM_CHECK_EQ(std::strcmp(buf, ""), 0);

    // give_item makes the inventory exist; records are id-sorted ("beer" < "grain").
    sim_world_give_item(w, "player", "grain", 3);
    sim_world_give_item(w, "player", "beer", 2);
    const int len = sim_world_inventory(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(len, static_cast<int>(std::strlen("beer:2;grain:3")));
    SIM_CHECK_EQ(std::strcmp(buf, "beer:2;grain:3"), 0);

    // Consuming to zero drops the row from the carried goods.
    sim_world_give_item(w, "player", "beer", -2);
    SIM_CHECK_EQ(std::strcmp(buf, "beer:2;grain:3"), 0);  // buf untouched until re-read
    sim_world_inventory(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(std::strcmp(buf, "grain:3"), 0);

    sim_world_destroy(w);
    return true;
}

static bool test_best_food_prefers_the_most_restorative() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    char buf[64];

    // Nothing held: empty answer, not an error.
    SIM_CHECK_EQ(sim_world_best_food(w, "player", buf, sizeof(buf)), 0);
    SIM_CHECK_EQ(std::strcmp(buf, ""), 0);

    // dates restore 20, grain 30, fish 35 (Needs.cpp's category table).
    sim_world_give_item(w, "player", "dates", 1);
    sim_world_best_food(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(std::strcmp(buf, "dates"), 0);
    sim_world_give_item(w, "player", "grain", 1);
    sim_world_best_food(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(std::strcmp(buf, "grain"), 0);
    sim_world_give_item(w, "player", "fish", 1);
    sim_world_best_food(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(std::strcmp(buf, "fish"), 0);

    // A tie (bread 30 = grain 30) breaks to the first id in sorted order —
    // once the stronger fish is eaten away, bread wins over grain.
    sim_world_give_item(w, "player", "fish", -1);  // back to dates+grain
    sim_world_give_item(w, "player", "bread", 1);
    sim_world_best_food(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(std::strcmp(buf, "bread"), 0);

    // The answer composes with the eat verb: best_food then eat consumes one.
    SIM_CHECK_EQ(sim_world_eat(w, "player", "bread"), 0);
    SIM_CHECK_EQ(sim_world_item_count(w, "player", "bread"), 0);

    sim_world_destroy(w);
    return true;
}

static bool test_best_drink_is_beer_not_water() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    char buf[64];

    SIM_CHECK_EQ(sim_world_best_drink(w, "player", buf, sizeof(buf)), 0);
    SIM_CHECK_EQ(std::strcmp(buf, ""), 0);

    // Water is a virtual id, never held: only real holdings answer.
    sim_world_give_item(w, "player", "grain", 5);  // food, not drink
    sim_world_best_drink(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(std::strcmp(buf, ""), 0);

    sim_world_give_item(w, "player", "beer", 1);
    sim_world_best_drink(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(std::strcmp(buf, "beer"), 0);

    // A non-food holding never answers best_food either.
    sim_world_give_item(w, "player", "wool", 9);
    sim_world_best_drink(w, "player", buf, sizeof(buf));
    SIM_CHECK_EQ(std::strcmp(buf, "beer"), 0);

    sim_world_destroy(w);
    return true;
}

static bool test_null_arguments_refuse() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    char buf[64];
    SIM_CHECK_EQ(sim_world_inventory(nullptr, "player", buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_inventory(w, nullptr, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_inventory(w, "player", nullptr, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_best_food(nullptr, "player", buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_best_food(w, nullptr, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_best_drink(w, "player", nullptr, sizeof(buf)), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_buffer_convention_truncates_but_reports() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    sim_world_give_item(w, "player", "grain", 12);
    // "grain:12" is 8 chars; a 4-byte buffer holds 3 + NUL and reports 8.
    char tiny[4] = {'x', 'x', 'x', 'x'};
    SIM_CHECK_EQ(sim_world_inventory(w, "player", tiny, sizeof(tiny)), 8);
    SIM_CHECK_EQ(std::strncmp(tiny, "gra", 3), 0);
    SIM_CHECK_EQ(tiny[3], '\0');
    sim_world_destroy(w);
    return true;
}

static bool test_set_need_for_debugging() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK_EQ(sim_world_set_need(w, "player", "hunger", 80), 0);
    SIM_CHECK_EQ(sim_world_hunger(w, "player"), 80);
    SIM_CHECK_EQ(sim_world_set_need(w, "player", "thirst", 250), 0);  // clamped
    SIM_CHECK_EQ(sim_world_thirst(w, "player"), 100);
    SIM_CHECK_EQ(sim_world_set_need(w, "player", "fatigue", -5), 0);
    SIM_CHECK_EQ(sim_world_fatigue(w, "player"), 0);
    SIM_CHECK_EQ(sim_world_set_need(w, "player", "joy", 5), -1);
    SIM_CHECK_EQ(sim_world_set_need(nullptr, "player", "hunger", 5), -1);
    SIM_CHECK_EQ(sim_world_set_need(w, nullptr, "hunger", 5), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_needs_severity() {
    SimWorld* normal = sim_world_create("../db/canon", 42);
    SimWorld* harsh = sim_world_create("../db/canon", 42);
    SIM_CHECK_EQ(sim_world_needs_severity(normal), 100);  // the designed rate
    sim_world_set_needs_severity(harsh, 200);
    SIM_CHECK_EQ(sim_world_needs_severity(harsh), 200);
    sim_world_advance_needs(normal, "player", 10, 0);
    sim_world_advance_needs(harsh, "player", 10, 0);
    SIM_CHECK(sim_world_hunger(normal, "player") > 0);
    SIM_CHECK_EQ(sim_world_hunger(harsh, "player"), 2 * sim_world_hunger(normal, "player"));
    sim_world_set_needs_severity(harsh, 900);
    SIM_CHECK_EQ(sim_world_needs_severity(harsh), 300);  // clamped
    sim_world_set_needs_severity(harsh, 0);
    SIM_CHECK_EQ(sim_world_needs_severity(harsh), 25);
    SIM_CHECK_EQ(sim_world_needs_severity(nullptr), -1);
    sim_world_set_needs_severity(nullptr, 50);  // must not crash
    sim_world_destroy(normal);
    sim_world_destroy(harsh);
    return true;
}

SIM_MAIN(test_needs_severity, test_set_need_for_debugging, test_inventory_empty_then_filled,
         test_best_food_prefers_the_most_restorative,
         test_best_drink_is_beer_not_water,
         test_null_arguments_refuse,
         test_buffer_convention_truncates_but_reports)
