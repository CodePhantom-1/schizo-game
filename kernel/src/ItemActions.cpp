// ItemActions.cpp — P0a: carrying, world items, containers (sim/ItemActions.hpp).
#include "sim/ItemActions.hpp"

#include "sim/Character.hpp"
#include "sim/Progression.hpp"
#include "sim/Property.hpp"

namespace sim {

long long capacity_g(const WorldState& w, const Id& actor) {
    int str = actor_attribute(w, actor, "strength");
    if (str < 1) str = kAttrStart;
    return kBaseCarryG + static_cast<long long>(kCarryPerStrengthG) * str;
}

long long carried_g(const WorldState& w, const Id& actor) {
    long long g = 0;
    if (const auto it = w.inventories.find(actor); it != w.inventories.end())
        for (const ItemStack& s : it->second.stacks) g += stack_weight_g(w.db, s);
    return g + purse(w.property, actor) * kSilverGPerShekel / kGrainsPerShekel;
}

int encumbrance(const WorldState& w, const Id& actor) {
    const long long cap = capacity_g(w, actor);
    const long long have = carried_g(w, actor);
    if (have * 100 <= cap * kBurdenedPct) return 0;
    if (have * 100 <= cap * kPinnedPct) return 1;
    return 2;
}

}  // namespace sim
