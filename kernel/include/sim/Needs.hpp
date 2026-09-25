#pragma once
// Needs.hpp — T3: the body's needs (hunger, thirst, fatigue).
// Imported mechanic: docs/mechanics.md rows 20 and 28 -> parent
// ../docs/rpg-systems.md §1.2 (Conditions) and §6 (Food and eating).
// The engine owns the sub-day clock; this module only advances needs by a
// whole number of game-hours it is handed — no wall clock, no internal timer.
//
// Invariants:
//  - deterministic; no wall-clock, no std::rand, no unordered iteration into
//    outputs (NeedsState::by_actor is a std::map, so iteration order is
//    already stable if the engine ever needs to enumerate it)
//  - writes ONLY NeedsState
//  - reads items.csv through Db (read-only) to decide what is edible/drinkable
#include "sim/Types.hpp"

#include <map>
#include <string>
#include <vector>

namespace sim {

class Db;  // sim/Db.hpp — read-only canon access

struct Needs {
    int hunger = 0;   // 0 = sated .. 100 = critical
    int thirst = 0;   // 0 = sated .. 100 = critical
    int fatigue = 0;  // 0 = rested .. 100 = critical
};

struct NeedsState {
    std::map<Id, Needs> by_actor;
};

// Returns the actor's Needs, creating a fresh (0/0/0) entry the first time
// that actor is seen.
Needs& needs_of(NeedsState& s, const Id& actor);

// Advances one actor's needs by `hours` game-hours. Hunger and thirst always
// climb; fatigue climbs while awake and drains while `sleeping` is true.
// `severity` is the accessibility needs-severity multiplier (mechanics row
// 37 / rpg-systems §12; INVENTED default 1.0 = the designed rate) applied to
// the hunger/thirst/waking-fatigue climb only — sleep always restores at the
// full rate regardless of difficulty.
void advance_needs(NeedsState& s, const Id& actor, int hours, bool sleeping,
                    double severity = 1.0);

// Eats an item by its db/canon/items.csv id. Only categories the parent doc's
// diet covers (see Needs.cpp's kHungerRestoreByCategory) count as food;
// anything else is refused with a reason and hunger is untouched.
bool eat(const Db& db, NeedsState& s, const Id& actor, const Id& item_id,
          std::string* reason = nullptr);

// Drinks an item by its db/canon/items.csv id, or the virtual id "water"
// (always drinkable, not a canon row — documented in Needs.cpp). Anything
// else outside the drink categories is refused with a reason.
bool drink(const Db& db, NeedsState& s, const Id& actor, const Id& item_id,
           std::string* reason = nullptr);

// Named condition effects at the current thresholds (INVENTED numeric
// thresholds — the parent doc names the conditions but not the numbers).
// Sorted alphabetically for determinism.
std::vector<std::string> need_effects(const Needs& n);

}  // namespace sim
