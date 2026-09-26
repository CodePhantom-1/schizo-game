// Crafting.cpp — interprets db/canon/recipes.csv against an Inventory.
// No randomness, no wall clock, no static mutable state: load_recipes()
// reads the Db's already-sorted rows() (Db::load parses in file order, which
// is id-ascending for recipes.csv) and returns them as-is, so the result is
// deterministic for a given Db.
#include "sim/Crafting.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>

namespace sim {
namespace {

// "Water" is drawn at the well: free infrastructure, never held or consumed —
// the same rule as Needs::drink (INVENTED; see module_Needs.md).
constexpr const char* kWater = "water";
// Upper bound on one craft call's batch — keeps qty*times far from overflow.
constexpr int kMaxTimes = 1000;

// "item:qty;item:qty" -> pairs, duplicates summed, in first-seen order.
// Returns false on any malformed entry: a typo must not become a free or
// output-less recipe.
bool parse_list(const std::string& field, std::vector<std::pair<Id, int>>& out) {
    std::stringstream ss(field);
    std::string entry;
    while (std::getline(ss, entry, ';')) {
        if (entry.empty()) continue;
        const auto colon = entry.find(':');
        if (colon == std::string::npos) return false;
        const Id item = entry.substr(0, colon);
        const std::string qty_s = entry.substr(colon + 1);
        char* end = nullptr;
        const long qty = std::strtol(qty_s.c_str(), &end, 10);
        if (item.empty() || qty_s.empty() || *end != '\0' || qty <= 0 || qty > kMaxTimes) return false;
        auto it = std::find_if(out.begin(), out.end(), [&](const auto& p) { return p.first == item; });
        if (it != out.end()) it->second += static_cast<int>(qty);
        else out.emplace_back(item, static_cast<int>(qty));
    }
    return true;
}

std::optional<Recipe> recipe_from_row(const Row& row) {
    if (row.get("tag") == "OPEN") return std::nullopt;  // OPEN rows cannot ship (canon_lint.py)
    Recipe r;
    r.id = row.get("id");
    if (!parse_list(row.get("inputs"), r.inputs) || !parse_list(row.get("outputs"), r.outputs) ||
        r.outputs.empty())
        return std::nullopt;  // malformed recipes are absent, never half-applied
    r.station = row.get("station");
    r.hours = std::atoi(row.get("hours").c_str());
    return r;
}

void fail(std::string* reason, const std::string& why) {
    if (reason) *reason = why;
}

// The one checker both can_craft and craft use.
std::optional<Recipe> check(const Db& db, const Inventory& inv, const Id& recipe_id,
                            const std::set<Id>& stations_at_hand, int times, std::string* reason) {
    if (times <= 0 || times > kMaxTimes) {
        fail(reason, "times must be 1.." + std::to_string(kMaxTimes));
        return std::nullopt;
    }
    const std::optional<Row> row = db.find("recipes", recipe_id);
    std::optional<Recipe> r = row ? recipe_from_row(*row) : std::nullopt;
    if (!r) {
        fail(reason, "unknown, OPEN or malformed recipe: " + recipe_id);
        return std::nullopt;
    }
    if (!r->station.empty() && stations_at_hand.find(r->station) == stations_at_hand.end()) {
        fail(reason, "station not at hand: " + r->station);
        return std::nullopt;
    }
    for (const auto& [item, qty] : r->inputs) {
        if (item == kWater) continue;
        const long long have = count_of(inv, item);
        if (have < static_cast<long long>(qty) * times) {
            fail(reason, "missing input: " + item);
            return std::nullopt;
        }
    }
    return r;
}

}  // namespace

std::vector<Recipe> load_recipes(const Db& db) {
    std::vector<Recipe> out;
    for (const Row& row : db.rows("recipes"))
        if (std::optional<Recipe> r = recipe_from_row(row)) out.push_back(std::move(*r));
    std::sort(out.begin(), out.end(),
              [](const Recipe& a, const Recipe& b) { return a.id < b.id; });
    return out;
}

bool can_craft(const Db& db, const Inventory& inv, const Id& recipe_id,
               const std::set<Id>& stations_at_hand, int times) {
    return check(db, inv, recipe_id, stations_at_hand, times, nullptr).has_value();
}

bool craft(const Db& db, Inventory& inv, const Id& recipe_id,
           const std::set<Id>& stations_at_hand, int times, std::string* reason) {
    const std::optional<Recipe> r = check(db, inv, recipe_id, stations_at_hand, times, reason);
    if (!r) return false;  // no partial effects: nothing consumed yet
    for (const auto& [item, qty] : r->inputs)
        if (item != kWater) take_items(inv, item, qty * times);
    for (const auto& [item, qty] : r->outputs)
        add_items(inv, item, qty * times);
    return true;
}

}  // namespace sim
