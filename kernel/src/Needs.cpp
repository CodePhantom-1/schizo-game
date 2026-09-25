// Needs.cpp — hunger, thirst, fatigue (mechanics rows 20 & 28; parent
// rpg-systems §1.2 "Hunger, thirst, fatigue, heat and cold" and §6 "Two
// meters: Hunger (calories) and Thirst (water)"). Heat/cold, zonal wounds,
// illness, purity and intoxication (the rest of §1.2) are NOT this module —
// they are their own future tracks; T3 is scoped to hunger/thirst/fatigue
// exactly as the coordinator's required surface lists.
//
// INVENTED machinery (the parent doc gives the two meters and the named
// conditions but no numbers or per-item values — items.csv/foods.csv give no
// nutrition columns either):
//  - hourly decay rates: hunger +2/h, thirst +3/h awake (climate: "the
//    Levantine summer doubles water needs" rpg-systems §1.2 — the doubling
//    itself is a future heat-model hook, not modeled here), fatigue +4/h
//    awake, fatigue -12/h asleep (sleep restores faster than waking tires)
//  - per-item-category restore amounts (eat/drink), keyed off items.csv's
//    `category` column since no per-item nutrition table exists
//  - "water" as a virtual always-drinkable id (rpg-systems §6 doesn't list
//    plain water as a tradeable good; it is infrastructure, not a market
//    item) — restores thirst only, no db lookup
//  - effect thresholds (need_effects): hunger 40 "hungry" / 75 "starving",
//    thirst 40 "parched", fatigue 70 "exhausted"
//  - the severity multiplier (mechanics row 37, rpg-systems §12 "needs
//    severity" accessibility toggle) scales the waking climb only; the
//    parent doc names the toggle but not its range, so 1.0 = designed rate
//    is the only value this module assumes
#include "sim/Needs.hpp"

#include "sim/Db.hpp"

#include <algorithm>
#include <cmath>

namespace sim {
namespace {

constexpr int kHungerPerHour = 2;
constexpr int kThirstPerHour = 3;
constexpr int kFatigueAwakePerHour = 4;
constexpr int kFatigueAsleepRestorePerHour = 12;

constexpr int kHungryAt = 40;
constexpr int kStarvingAt = 75;
constexpr int kParchedAt = 40;
constexpr int kExhaustedAt = 70;

int clamp0_100(int v) { return std::max(0, std::min(100, v)); }

// items.csv `category` -> hunger points restored by eating one unit.
// Only these categories count as food; everything else is refused.
int hunger_restore_for_category(const std::string& category) {
    if (category == "food and offering material") return 35;  // fish
    if (category == "staple") return 30;                      // grain, wheat
    if (category == "ration") return 25;                      // drought_ration_measure
    if (category == "staple food") return 20;                 // dates
    if (category == "staple baked food") return 30;           // bread (T4) — a staple, same as grain
    return 0;
}

// items.csv `category` -> thirst points restored by drinking one unit.
int thirst_restore_for_category(const std::string& category) {
    if (category == "staple drink") return 20;  // beer
    return 0;
}

bool is_food_category(const std::string& category) {
    return hunger_restore_for_category(category) > 0;
}

bool is_drink_category(const std::string& category) {
    return thirst_restore_for_category(category) > 0;
}

}  // namespace

Needs& needs_of(NeedsState& s, const Id& actor) { return s.by_actor[actor]; }

void advance_needs(NeedsState& s, const Id& actor, int hours, bool sleeping, double severity) {
    if (hours <= 0) return;
    Needs& n = needs_of(s, actor);

    const int hunger_climb = static_cast<int>(std::llround(hours * kHungerPerHour * severity));
    const int thirst_climb = static_cast<int>(std::llround(hours * kThirstPerHour * severity));
    n.hunger = clamp0_100(n.hunger + hunger_climb);
    n.thirst = clamp0_100(n.thirst + thirst_climb);

    if (sleeping) {
        n.fatigue = clamp0_100(n.fatigue - hours * kFatigueAsleepRestorePerHour);
    } else {
        const int fatigue_climb = static_cast<int>(std::llround(hours * kFatigueAwakePerHour * severity));
        n.fatigue = clamp0_100(n.fatigue + fatigue_climb);
    }
}

bool eat(const Db& db, NeedsState& s, const Id& actor, const Id& item_id, std::string* reason) {
    const std::optional<Row> item = db.find("items", item_id);
    if (!item) {
        if (reason) *reason = "unknown_item";
        return false;
    }
    const std::string category = item->get("category");
    if (!is_food_category(category)) {
        if (reason) *reason = "not_food";
        return false;
    }
    Needs& n = needs_of(s, actor);
    n.hunger = clamp0_100(n.hunger - hunger_restore_for_category(category));
    return true;
}

bool drink(const Db& db, NeedsState& s, const Id& actor, const Id& item_id, std::string* reason) {
    Needs& n = needs_of(s, actor);
    if (item_id == "water") {
        // Always drinkable — infrastructure, not a canon/market good (INVENTED, see header).
        n.thirst = clamp0_100(n.thirst - 40);
        return true;
    }
    const std::optional<Row> item = db.find("items", item_id);
    if (!item) {
        if (reason) *reason = "unknown_item";
        return false;
    }
    const std::string category = item->get("category");
    if (!is_drink_category(category)) {
        if (reason) *reason = "not_drink";
        return false;
    }
    n.thirst = clamp0_100(n.thirst - thirst_restore_for_category(category));
    if (category == "staple drink") {
        // "Beer and wine give some water and calories" (rpg-systems §6).
        n.hunger = clamp0_100(n.hunger - 5);
    }
    return true;
}

std::vector<std::string> need_effects(const Needs& n) {
    std::vector<std::string> out;
    if (n.hunger >= kHungryAt) out.push_back("hungry");
    if (n.hunger >= kStarvingAt) out.push_back("starving");
    if (n.thirst >= kParchedAt) out.push_back("parched");
    if (n.fatigue >= kExhaustedAt) out.push_back("exhausted");
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace sim
