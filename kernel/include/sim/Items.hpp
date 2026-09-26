#pragma once
// Items.hpp — FND-01/02: item stacks and the item catalogue's physical columns.
// Pure: no WorldState. An Inventory is always normalized (sorted by kind,
// equal kinds merged, no empty stack), so save bytes are deterministic.
// Contract: kernel/contracts/module_Items.md.
#include "sim/Db.hpp"
#include "sim/Types.hpp"

#include <climits>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace sim {

constexpr int kQualityPoor = 0;
constexpr int kQualityCommon = 1;
constexpr int kQualityFine = 2;
constexpr int kQualityMasterwork = 3;
constexpr int kMaxStackQty = INT_MAX;  // saturation cap: the C API contract (give_item saturates at INT_MAX)

struct ItemStack {
    Id item;
    int qty = 0;
    int quality = kQualityCommon;
    int condition = 100;     // 0 broken .. 100 new
    Id owner;                // "" = belongs to whoever holds it; else the rightful owner
    bool stolen = false;     // taken without the owner's leave
    DayNumber made_day = 0;  // freshness clock (spoilage reads it)
    bool bound = false;      // quest item: cannot be dropped or sold
    bool same_kind(const ItemStack& o) const {
        return item == o.item && quality == o.quality && condition == o.condition &&
               owner == o.owner && stolen == o.stolen && made_day == o.made_day && bound == o.bound;
    }
    bool operator==(const ItemStack&) const = default;
};

struct Inventory {
    std::vector<ItemStack> stacks;
};

// Sorts by (item, owner, stolen, bound, quality, made_day, condition), merges
// equal kinds (saturating at kMaxStackQty) and drops stacks with qty <= 0.
void normalize(Inventory& inv);
int count_of(const Inventory& inv, const Id& item);
std::map<Id, int> counts(const Inventory& inv);  // item -> total, sorted
// Adds a stack; qty <= 0 or an empty item id is a no-op. Returns the item's count after.
int add_stack(Inventory& inv, ItemStack s);
// Adds qty common, unowned, day-0 units. Returns the item's count after.
int add_items(Inventory& inv, const Id& item, int qty);
// Takes up to qty units of item (never more than held; qty <= 0 takes nothing):
// the oldest made_day first, then the lowest quality, then stack order.
std::vector<ItemStack> take_items(Inventory& inv, const Id& item, int qty);
// Adds or takes the difference so the item's count becomes n (n < 0 acts as 0;
// takes follow take_items' order). For tests and the developer console.
int set_count(Inventory& inv, const Id& item, int n);
// Takes up to qty units from one stack (by index in the normalized order).
std::optional<ItemStack> take_from_stack(Inventory& inv, std::size_t index, int qty);

}  // namespace sim
