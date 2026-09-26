#pragma once
// Crafting.hpp — CONTRACT (T4: the first crafting chain, grain -> bread and
// beer; rpg-systems §6: "Cooking is a skill. The stations are the quern, the
// bread oven and the brewing vat... Recipes come only from the bible §3 food
// lists and are tagged").
//
// Recipes are canon (db/canon/recipes.csv), not engine content: this module
// only interprets the table. An Inventory is a normalized list of item stacks
// (sim/Items.hpp; the engine or test owns the instance — this module never holds one itself, unlike the
// other Wave modules' *State).
//
// Invariants:
//  - craft() either fully applies a recipe `times` times or leaves the
//    inventory untouched (no partial consumption/production on failure)
//  - deterministic: load_recipes() returns recipes sorted by id; no
//    randomness, no wall clock
//  - reads db table: recipes (skips OPEN rows)
#include "sim/Db.hpp"
#include "sim/Items.hpp"
#include "sim/Types.hpp"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace sim {


struct Recipe {
    Id id;
    std::vector<std::pair<Id, int>> inputs;
    std::vector<std::pair<Id, int>> outputs;
    Id station;
    int hours = 0;
};

// Every non-OPEN row of db/canon/recipes.csv, sorted by id (determinism).
std::vector<Recipe> load_recipes(const Db& db);

// True iff the inventory holds `times` full sets of the recipe's inputs and
// `stations_at_hand` contains the recipe's station. False (with no crash) for
// an unknown/OPEN recipe id or times <= 0.
bool can_craft(const Db& db, const Inventory& inv, const Id& recipe_id,
                const std::set<Id>& stations_at_hand, int times = 1);

// Consumes inputs and produces outputs, `times` times. On any failure
// (unknown/OPEN recipe, missing station, insufficient inputs, times <= 0)
// returns false, writes a reason if `reason` is non-null, and leaves `inv`
// byte-for-byte unchanged.
bool craft(const Db& db, Inventory& inv, const Id& recipe_id,
           const std::set<Id>& stations_at_hand, int times = 1,
           std::string* reason = nullptr);

}  // namespace sim
