// ItemActions.cpp — P0a: carrying, world items, containers (sim/ItemActions.hpp).
#include "sim/ItemActions.hpp"

#include "sim/Actions.hpp"
#include "sim/Character.hpp"
#include "sim/Progression.hpp"
#include "sim/Property.hpp"

#include <algorithm>
#include <climits>

namespace sim {
namespace {

const char* key_for(int code) {
    switch (code) {
        case -1: return "refuse.bad_argument";
        case -2: return "refuse.unknown";
        case -3: return "refuse.bound";
        case -4: return "refuse.too_heavy";
        case -5: return "refuse.full";
        case -6: return "refuse.locked";
        default: return "";
    }
}

ItemResult refuse(int code) {
    ItemResult r;
    r.code = code;
    r.reason = key_for(code);
    return r;
}

Id city_of(const WorldState& w, const Id& place) {
    const std::optional<Row> row = w.db.find("places", place);
    const std::string city = row ? row->get("city") : std::string();
    return city.empty() ? Id("city_of_the_moon") : Id(city);
}

// The ownership rule (header). Mutates the moving part; returns true on theft.
bool claim(WorldState& w, const Id& actor, ItemStack& part, const Id& container_owner, const Id& place,
           const std::vector<Id>& witnesses) {
    const Id rightful = part.owner.empty() ? container_owner : part.owner;
    if (rightful.empty() || rightful == actor) {
        part.owner.clear();
        part.stolen = false;
        return false;
    }
    part.owner = rightful;
    part.stolen = true;
    if (!witnesses.empty()) (void)commit_crime(w, actor, "theft", city_of(w, place), witnesses, rightful);
    return true;
}

long long contents_g(const WorldState& w, const Inventory& inv) {
    long long g = 0;
    for (const ItemStack& s : inv.stacks) g += stack_weight_g(w.db, s);
    return g;
}

}  // namespace


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

int units_that_fit(const WorldState& w, const Id& actor, const ItemStack& s) {
    const std::optional<ItemDef> d = item_def(w.db, s.item);
    const long long unit = d ? d->weight_g : 0;
    if (unit <= 0) return INT_MAX;
    const long long room = capacity_g(w, actor) * kPinnedPct / 100 - carried_g(w, actor);
    return room <= 0 ? 0 : static_cast<int>(std::min<long long>(room / unit, INT_MAX));
}

ItemResult drop_item(WorldState& w, const Id& actor, std::size_t stack_index, int qty,
                     const Id& place, int x_cm, int y_cm, int z_cm) {
    if (actor.empty() || place.empty() || qty <= 0) return refuse(-1);
    const auto found = w.inventories.find(actor);  // look up, never insert: a refusal changes nothing
    if (found == w.inventories.end() || stack_index >= found->second.stacks.size()) return refuse(-2);
    Inventory& inv = found->second;
    if (inv.stacks[stack_index].bound) return refuse(-3);
    std::optional<ItemStack> part = take_from_stack(inv, stack_index, qty);
    if (part->owner.empty()) part->owner = actor;  // dropped goods stay the dropper's
    WorldItem wi;
    wi.id = "witem_" + std::to_string(w.world_items.next_id++);
    wi.stack = std::move(*part);
    wi.place = place;
    wi.x_cm = x_cm;
    wi.y_cm = y_cm;
    wi.z_cm = z_cm;
    wi.dropped_day = w.day;
    wi.dropped_by = actor;
    ItemResult r;
    r.code = wi.stack.qty;
    r.id = wi.id;
    w.world_items.items.push_back(std::move(wi));
    return r;
}

ItemResult pick_up(WorldState& w, const Id& actor, const Id& world_item, int qty,
                   const std::vector<Id>& witnesses) {
    auto it = std::find_if(w.world_items.items.begin(), w.world_items.items.end(),
                           [&](const WorldItem& wi) { return wi.id == world_item; });
    if (it == w.world_items.items.end()) return refuse(-2);
    if (actor.empty() || qty <= 0) return refuse(-1);
    ItemStack part = it->stack;
    part.qty = std::min({qty, it->stack.qty, units_that_fit(w, actor, it->stack)});
    if (part.qty <= 0) return refuse(-4);
    ItemResult r;
    r.theft = claim(w, actor, part, "", it->place, witnesses);
    r.code = part.qty;
    it->stack.qty -= part.qty;
    if (it->stack.qty == 0) w.world_items.items.erase(it);
    add_stack(w.inventories[actor], std::move(part));
    return r;
}

ItemResult add_container(WorldState& w, const Id& id, const Id& place, const std::string& kind,
                         const Id& owner, int capacity_g) {
    if (id.empty() || place.empty() || capacity_g < 0 || w.world_items.containers.count(id)) return refuse(-1);
    Container c;
    c.id = id;
    c.place = place;
    c.kind = kind;
    c.owner = owner;
    c.capacity_g = capacity_g;
    w.world_items.containers.emplace(id, std::move(c));
    return ItemResult{};
}

ItemResult put_in(WorldState& w, const Id& actor, const Id& container, std::size_t stack_index, int qty) {
    if (actor.empty() || qty <= 0) return refuse(-1);
    auto cit = w.world_items.containers.find(container);
    if (cit == w.world_items.containers.end()) return refuse(-2);
    Container& c = cit->second;
    const auto found = w.inventories.find(actor);  // look up, never insert
    if (found == w.inventories.end() || stack_index >= found->second.stacks.size()) return refuse(-2);
    Inventory& inv = found->second;
    if (c.locked) return refuse(-6);
    const ItemStack& s = inv.stacks[stack_index];
    if (s.bound) return refuse(-3);
    int fits = qty;
    if (c.capacity_g > 0) {
        const std::optional<ItemDef> d = item_def(w.db, s.item);
        const long long unit = d ? d->weight_g : 0;
        const long long room = c.capacity_g - contents_g(w, c.contents);
        if (unit > 0) fits = static_cast<int>(std::min<long long>(qty, room <= 0 ? 0 : room / unit));
    }
    if (fits <= 0) return refuse(-5);
    std::optional<ItemStack> part = take_from_stack(inv, stack_index, fits);
    if (part->owner.empty() && c.owner != actor) part->owner = actor;  // your goods stay yours
    ItemResult r;
    r.code = part->qty;
    add_stack(c.contents, std::move(*part));
    return r;
}

ItemResult take_out(WorldState& w, const Id& actor, const Id& container, std::size_t stack_index,
                    int qty, const std::vector<Id>& witnesses) {
    if (actor.empty() || qty <= 0) return refuse(-1);
    auto cit = w.world_items.containers.find(container);
    if (cit == w.world_items.containers.end()) return refuse(-2);
    Container& c = cit->second;
    if (stack_index >= c.contents.stacks.size()) return refuse(-2);
    if (c.locked) return refuse(-6);
    const ItemStack& s = c.contents.stacks[stack_index];
    const int n = std::min({qty, s.qty, units_that_fit(w, actor, s)});
    if (n <= 0) return refuse(-4);
    std::optional<ItemStack> part = take_from_stack(c.contents, stack_index, n);
    ItemResult r;
    r.theft = claim(w, actor, *part, c.owner, c.place, witnesses);
    r.code = part->qty;
    add_stack(w.inventories[actor], std::move(*part));
    return r;
}

std::vector<const WorldItem*> world_items_at(const WorldState& w, const Id& place) {
    std::vector<const WorldItem*> out;
    for (const WorldItem& wi : w.world_items.items)
        if (wi.place == place) out.push_back(&wi);
    return out;
}

}  // namespace sim
