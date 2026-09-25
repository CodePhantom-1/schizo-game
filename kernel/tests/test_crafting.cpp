// test_crafting.cpp — the grain -> bread and grain -> beer chains
// (db/canon/recipes.csv; sim/Crafting.hpp).
#include "sim/Crafting.hpp"

#include "sim/Db.hpp"
#include "sim/Test.hpp"

#include <set>
#include <string>

using namespace sim;

namespace {

Db load_canon() { return Db::load("../db/canon"); }

}  // namespace

static bool test_bread_chain_end_to_end() {
    const Db db = load_canon();
    Inventory inv;
    inv.counts["grain"] = 3;
    const std::set<Id> stations = {"quern", "oven"};

    std::string reason;
    SIM_CHECK(craft(db, inv, "mill_flour", stations, 1, &reason));
    SIM_CHECK_EQ(inv.counts["grain"], 2);
    SIM_CHECK_EQ(inv.counts["barley_flour"], 1);

    // bake_bread needs flour:2, so mill a second batch first.
    SIM_CHECK(craft(db, inv, "mill_flour", stations));
    SIM_CHECK_EQ(inv.counts["barley_flour"], 2);
    inv.counts["water"] = 1;

    SIM_CHECK(craft(db, inv, "bake_bread", stations, 1, &reason));
    SIM_CHECK_EQ(inv.counts["barley_flour"], 0);
    SIM_CHECK_EQ(inv.counts["water"], 0);
    SIM_CHECK_EQ(inv.counts["bread"], 1);
    return true;
}

static bool test_beer_chain_end_to_end() {
    const Db db = load_canon();
    Inventory inv;
    inv.counts["grain"] = 4;
    inv.counts["water"] = 3;
    const std::set<Id> stations = {"quern", "oven", "brewing_vat"};

    SIM_CHECK(craft(db, inv, "malt_barley", stations, 2));  // grain:2 x2 = 4
    SIM_CHECK_EQ(inv.counts["grain"], 0);
    SIM_CHECK_EQ(inv.counts["malted_barley"], 2);

    SIM_CHECK(craft(db, inv, "bake_bappir", stations));  // malted_barley:2; water:1
    SIM_CHECK_EQ(inv.counts["malted_barley"], 0);
    SIM_CHECK_EQ(inv.counts["water"], 2);
    SIM_CHECK_EQ(inv.counts["bappir"], 1);

    SIM_CHECK(craft(db, inv, "brew_beer", stations));  // bappir:1; water:2
    SIM_CHECK_EQ(inv.counts["bappir"], 0);
    SIM_CHECK_EQ(inv.counts["water"], 0);
    SIM_CHECK_EQ(inv.counts["beer"], 1);
    return true;
}

static bool test_missing_input_refused_no_partial_effects() {
    const Db db = load_canon();
    Inventory inv;
    inv.counts["grain"] = 1;  // mill_flour needs grain:1, so this alone would pass...
    inv.counts["water"] = 5;  // ...but bake_bread needs flour:2, which we don't have.
    const Inventory before = inv;
    const std::set<Id> stations = {"quern", "oven"};

    std::string reason;
    SIM_CHECK(!craft(db, inv, "bake_bread", stations, 1, &reason));
    SIM_CHECK(!reason.empty());
    SIM_CHECK(inv.counts == before.counts);  // no partial consumption
    SIM_CHECK(!can_craft(db, inv, "bake_bread", stations));
    return true;
}

static bool test_missing_station_refused_no_partial_effects() {
    const Db db = load_canon();
    Inventory inv;
    inv.counts["grain"] = 5;
    const Inventory before = inv;
    const std::set<Id> stations = {};  // no quern at hand

    std::string reason;
    SIM_CHECK(!craft(db, inv, "mill_flour", stations, 1, &reason));
    SIM_CHECK(!reason.empty());
    SIM_CHECK(inv.counts == before.counts);
    SIM_CHECK(!can_craft(db, inv, "mill_flour", stations));
    return true;
}

static bool test_unknown_recipe_refused() {
    const Db db = load_canon();
    Inventory inv;
    inv.counts["grain"] = 5;
    const std::set<Id> stations = {"quern", "oven", "brewing_vat"};

    std::string reason;
    SIM_CHECK(!craft(db, inv, "no_such_recipe", stations, 1, &reason));
    SIM_CHECK(!reason.empty());
    SIM_CHECK(!can_craft(db, inv, "no_such_recipe", stations));
    return true;
}

// Every recipe's inputs, outputs and station resolve to a real items.csv row.
static bool test_every_recipe_item_exists_in_items_csv() {
    const Db db = load_canon();
    const std::vector<Recipe> recipes = load_recipes(db);
    SIM_CHECK(!recipes.empty());
    for (const Recipe& r : recipes) {
        for (const auto& [item, qty] : r.inputs) {
            SIM_CHECK(db.has("items", item));
            SIM_CHECK(qty > 0);
        }
        for (const auto& [item, qty] : r.outputs) {
            SIM_CHECK(db.has("items", item));
            SIM_CHECK(qty > 0);
        }
        if (!r.station.empty()) SIM_CHECK(db.has("items", r.station));
    }
    return true;
}

// Two independent loads from the same canon dir must agree byte-for-byte
// (same ids, same order, same fields) — determinism, no randomness/clock.
static bool test_load_recipes_is_deterministic() {
    const Db db = load_canon();
    const std::vector<Recipe> a = load_recipes(db);
    const std::vector<Recipe> b = load_recipes(db);
    SIM_CHECK_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        SIM_CHECK_EQ(a[i].id, b[i].id);
        SIM_CHECK_EQ(a[i].station, b[i].station);
        SIM_CHECK_EQ(a[i].hours, b[i].hours);
        SIM_CHECK(a[i].inputs == b[i].inputs);
        SIM_CHECK(a[i].outputs == b[i].outputs);
        // sorted by id (ascending)
        if (i > 0) SIM_CHECK(a[i - 1].id < a[i].id);
    }
    return true;
}

SIM_MAIN(test_bread_chain_end_to_end, test_beer_chain_end_to_end,
          test_missing_input_refused_no_partial_effects,
          test_missing_station_refused_no_partial_effects,
          test_unknown_recipe_refused, test_every_recipe_item_exists_in_items_csv,
          test_load_recipes_is_deterministic)
