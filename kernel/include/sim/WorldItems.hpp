#pragma once
// WorldItems.hpp — FND-04: goods lying in the world and goods in containers,
// as kernel state (saved), so dropping, picking up, looting, theft evidence
// and saving all work the same way. Data only; the verbs are in
// sim/ItemActions.hpp. Contract: kernel/contracts/module_Items.md.
#include "sim/Items.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace sim {

struct WorldItem {
    Id id;                  // "witem_<n>"
    ItemStack stack;        // owner "" = anyone's; a dropped stack keeps its dropper as owner
    Id place;               // places.csv id (or a wild place)
    int x_cm = 0, y_cm = 0, z_cm = 0;  // resting position (the engine reports it)
    DayNumber dropped_day = 0;
    Id dropped_by;
};

struct Container {
    Id id;                  // chosen by whoever places it (building seeding, the engine)
    Id place;
    std::string kind;       // jar | chest | basket | granary | stall | body
    Id owner;               // "" = anyone's
    bool locked = false;    // locks are WLD-06 (P1); a locked container refuses today
    Id key_item;
    int capacity_g = 0;     // 0 = no limit
    Inventory contents;
};

struct WorldItemsState {
    std::vector<WorldItem> items;         // in order of creation
    std::map<Id, Container> containers;
    std::int64_t next_id = 1;
};

}  // namespace sim
