// test_crafting.cpp — the grain -> bread and grain -> beer chains
// (db/canon/recipes.csv; sim/Crafting.hpp).
#include "sim/Crafting.hpp"

#include "sim/Db.hpp"
#include "sim/Test.hpp"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace sim;

namespace {

Db load_canon() { return Db::load("../db/canon"); }

}  // namespace

static bool test_bread_chain_end_to_end() {
    const Db db = load_canon();
    Inventory inv;
    set_count(inv, "grain", 3);
    const std::set<Id> stations = {"quern", "oven"};

    std::string reason;
    SIM_CHECK(craft(db, inv, "mill_flour", stations, 1, &reason));
    SIM_CHECK_EQ(count_of(inv, "grain"), 2);
    SIM_CHECK_EQ(count_of(inv, "barley_flour"), 1);

    // bake_bread needs flour:2, so mill a second batch first.
    SIM_CHECK(craft(db, inv, "mill_flour", stations));
    SIM_CHECK_EQ(count_of(inv, "barley_flour"), 2);

    SIM_CHECK(craft(db, inv, "bake_bread", stations, 1, &reason));
    SIM_CHECK_EQ(count_of(inv, "barley_flour"), 0);
    SIM_CHECK(count_of(inv, "water") == 0);  // water is drawn at the well, never held
    SIM_CHECK_EQ(count_of(inv, "bread"), 1);
    return true;
}

static bool test_beer_chain_end_to_end() {
    const Db db = load_canon();
    Inventory inv;
    set_count(inv, "grain", 4);
    const std::set<Id> stations = {"quern", "oven", "brewing_vat"};

    SIM_CHECK(craft(db, inv, "malt_barley", stations, 2));  // grain:2 x2 = 4
    SIM_CHECK_EQ(count_of(inv, "grain"), 0);
    SIM_CHECK_EQ(count_of(inv, "malted_barley"), 2);

    SIM_CHECK(craft(db, inv, "bake_bappir", stations));  // malted_barley:2; water:1
    SIM_CHECK_EQ(count_of(inv, "malted_barley"), 0);
    SIM_CHECK_EQ(count_of(inv, "bappir"), 1);

    SIM_CHECK(craft(db, inv, "brew_beer", stations));  // bappir:1; water:2
    SIM_CHECK_EQ(count_of(inv, "bappir"), 0);
    SIM_CHECK(count_of(inv, "water") == 0);
    SIM_CHECK_EQ(count_of(inv, "beer"), 1);
    return true;
}

static bool test_missing_input_refused_no_partial_effects() {
    const Db db = load_canon();
    Inventory inv;
    set_count(inv, "grain", 1);  // mill_flour needs grain:1, so this alone would pass...
    // ...but bake_bread needs flour:2, which we don't have.
    const Inventory before = inv;
    const std::set<Id> stations = {"quern", "oven"};

    std::string reason;
    SIM_CHECK(!craft(db, inv, "bake_bread", stations, 1, &reason));
    SIM_CHECK(!reason.empty());
    SIM_CHECK(inv.stacks == before.stacks);  // no partial consumption
    SIM_CHECK(!can_craft(db, inv, "bake_bread", stations));
    return true;
}

static bool test_missing_station_refused_no_partial_effects() {
    const Db db = load_canon();
    Inventory inv;
    set_count(inv, "grain", 5);
    const Inventory before = inv;
    const std::set<Id> stations = {};  // no quern at hand

    std::string reason;
    SIM_CHECK(!craft(db, inv, "mill_flour", stations, 1, &reason));
    SIM_CHECK(!reason.empty());
    SIM_CHECK(inv.stacks == before.stacks);
    SIM_CHECK(!can_craft(db, inv, "mill_flour", stations));
    return true;
}

static bool test_unknown_recipe_refused() {
    const Db db = load_canon();
    Inventory inv;
    set_count(inv, "grain", 5);
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

// Batch-1 review: malformed rows are absent (never free/output-less), duplicate
// inputs are summed, and a huge batch is refused instead of overflowing.
static bool test_malformed_and_duplicate_rows() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "sim_crafting_fixture";
    fs::create_directories(dir);
    {
        std::ofstream f(dir / "recipes.csv");
        f << "id,name,inputs,outputs,station,hours,tag,source_ref\n"
          << "no_qty,x,grain:1,bread,,1,INVENTED,t\n"
          << "bad_qty,x,grain:x,bread:1,,1,INVENTED,t\n"
          << "no_output,x,grain:1,,,1,INVENTED,t\n"
          << "dup_in,x,grain:1;grain:1,bread:1,,1,INVENTED,t\n";
    }
    const Db db = Db::load(dir.string());
    Inventory inv;
    set_count(inv, "grain", 1);
    const std::set<Id> none;
    for (const char* id : {"no_qty", "bad_qty", "no_output"}) {
        SIM_CHECK(!craft(db, inv, id, none));
        SIM_CHECK_EQ(count_of(inv, "grain"), 1);
    }
    SIM_CHECK(!craft(db, inv, "dup_in", none));  // needs grain:2 in total
    SIM_CHECK_EQ(count_of(inv, "grain"), 1);
    set_count(inv, "grain", 2);
    SIM_CHECK(craft(db, inv, "dup_in", none));
    SIM_CHECK_EQ(count_of(inv, "grain"), 0);
    SIM_CHECK(!craft(db, inv, "dup_in", none, 1'100'000'000));
    SIM_CHECK_EQ(load_recipes(db).size(), 1u);
    fs::remove_all(dir);
    return true;
}

SIM_MAIN(test_malformed_and_duplicate_rows, test_bread_chain_end_to_end, test_beer_chain_end_to_end,
          test_missing_input_refused_no_partial_effects,
          test_missing_station_refused_no_partial_effects,
          test_unknown_recipe_refused, test_every_recipe_item_exists_in_items_csv,
          test_load_recipes_is_deterministic)
