// Crafting.cpp — interprets db/canon/recipes.csv against an Inventory.
// No randomness, no wall clock, no static mutable state: load_recipes()
// reads the Db's already-sorted rows() (Db::load parses in file order, which
// is id-ascending for recipes.csv) and returns them as-is, so the result is
// deterministic for a given Db.
#include "sim/Crafting.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace sim {
namespace {

// "item:qty;item:qty" -> pairs, in the order written (recipes.csv order).
std::vector<std::pair<Id, int>> parse_list(const std::string& field) {
    std::vector<std::pair<Id, int>> out;
    std::stringstream ss(field);
    std::string entry;
    while (std::getline(ss, entry, ';')) {
        if (entry.empty()) continue;
        const auto colon = entry.find(':');
        if (colon == std::string::npos) continue;
        const Id item = entry.substr(0, colon);
        const int qty = std::atoi(entry.substr(colon + 1).c_str());
        if (item.empty() || qty <= 0) continue;
        out.emplace_back(item, qty);
    }
    return out;
}

const Recipe* find_recipe(const std::vector<Recipe>& recipes, const Id& id) {
    for (const Recipe& r : recipes)
        if (r.id == id) return &r;
    return nullptr;
}

void fail(std::string* reason, const std::string& why) {
    if (reason) *reason = why;
}

}  // namespace

std::vector<Recipe> load_recipes(const Db& db) {
    std::vector<Recipe> out;
    for (const Row& row : db.rows("recipes")) {
        if (row.get("tag") == "OPEN") continue;  // OPEN rows cannot ship (canon_lint.py)
        Recipe r;
        r.id = row.get("id");
        r.inputs = parse_list(row.get("inputs"));
        r.outputs = parse_list(row.get("outputs"));
        r.station = row.get("station");
        r.hours = std::atoi(row.get("hours").c_str());
        out.push_back(std::move(r));
    }
    std::sort(out.begin(), out.end(),
              [](const Recipe& a, const Recipe& b) { return a.id < b.id; });
    return out;
}

bool can_craft(const Db& db, const Inventory& inv, const Id& recipe_id,
                const std::set<Id>& stations_at_hand, int times) {
    if (times <= 0) return false;
    const std::vector<Recipe> recipes = load_recipes(db);
    const Recipe* r = find_recipe(recipes, recipe_id);
    if (!r) return false;
    if (!r->station.empty() && stations_at_hand.find(r->station) == stations_at_hand.end())
        return false;
    for (const auto& [item, qty] : r->inputs) {
        const auto it = inv.counts.find(item);
        const int have = it == inv.counts.end() ? 0 : it->second;
        if (have < qty * times) return false;
    }
    return true;
}

bool craft(const Db& db, Inventory& inv, const Id& recipe_id,
           const std::set<Id>& stations_at_hand, int times, std::string* reason) {
    if (times <= 0) {
        fail(reason, "times must be positive");
        return false;
    }
    const std::vector<Recipe> recipes = load_recipes(db);
    const Recipe* r = find_recipe(recipes, recipe_id);
    if (!r) {
        fail(reason, "unknown or OPEN recipe: " + recipe_id);
        return false;
    }
    if (!r->station.empty() && stations_at_hand.find(r->station) == stations_at_hand.end()) {
        fail(reason, "station not at hand: " + r->station);
        return false;
    }
    for (const auto& [item, qty] : r->inputs) {
        const auto it = inv.counts.find(item);
        const int have = it == inv.counts.end() ? 0 : it->second;
        if (have < qty * times) {
            fail(reason, "missing input: " + item);
            return false;  // no partial effects: nothing consumed yet
        }
    }
    // Every check passed: apply the whole recipe `times` times, atomically.
    for (const auto& [item, qty] : r->inputs)
        inv.counts[item] -= qty * times;
    for (const auto& [item, qty] : r->outputs)
        inv.counts[item] += qty * times;
    return true;
}

}  // namespace sim
