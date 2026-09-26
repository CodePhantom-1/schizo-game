// CApiItems.cpp — P0a: the C ABI over item stacks, carrying, world items,
// containers, timed actions and notices (sim/CApiItems.h).
#include "sim/CApiItems.h"

#include "sim/ItemActions.hpp"

#include <cstring>
#include <string>

#include "CApiInternal.hpp"

using namespace sim;

namespace {

int write_str(char* out, int cap, const std::string& s) {
    if (out == nullptr || cap <= 0) return static_cast<int>(s.size());
    const int len = static_cast<int>(s.size());
    const int copy = (cap - 1 < len) ? cap - 1 : len;
    std::memcpy(out, s.data(), static_cast<std::size_t>(copy));
    out[copy] = '\0';
    return len;
}

// Getter variant: a null buffer is an error (-1), as in CApi.cpp.
int write_req(char* out, int cap, const std::string& s) {
    if (out == nullptr || cap <= 0) return -1;
    return write_str(out, cap, s);
}

template <class F>
auto guard(F&& f) -> decltype(f()) {
    try {
        return f();
    } catch (...) {
        return -1;
    }
}

// "item;qty;quality;condition;owner;stolen;made_day;bound" — shared by
// stacks, world items and containers.
std::string stack_record(const ItemStack& s) {
    return s.item + ';' + std::to_string(s.qty) + ';' + std::to_string(s.quality) + ';' +
           std::to_string(s.condition) + ';' + s.owner + ';' + (s.stolen ? "1" : "0") + ';' +
           std::to_string(s.made_day) + ';' + (s.bound ? "1" : "0");
}

const Inventory* inventory_of(const SimWorld* world, const char* actor) {
    const auto it = world->world.inventories.find(Id(actor));
    return it == world->world.inventories.end() ? nullptr : &it->second;
}

}  // namespace

extern "C" {

int sim_world_last_reason(const SimWorld* world, char* out, int cap) {
    return guard([&] {
        if (world == nullptr) return -1;
        return write_req(out, cap, world->last_reason);
    });
}

int64_t sim_world_capacity_g(const SimWorld* world, const char* actor) {
    return guard([&]() -> int64_t {
        if (world == nullptr || actor == nullptr) return -1;
        return capacity_g(world->world, Id(actor));
    });
}

int64_t sim_world_carried_g(const SimWorld* world, const char* actor) {
    return guard([&]() -> int64_t {
        if (world == nullptr || actor == nullptr) return -1;
        return carried_g(world->world, Id(actor));
    });
}

int sim_world_encumbrance(const SimWorld* world, const char* actor) {
    return guard([&] {
        if (world == nullptr || actor == nullptr) return -1;
        return encumbrance(world->world, Id(actor));
    });
}

int sim_world_stack_count(const SimWorld* world, const char* actor) {
    return guard([&] {
        if (world == nullptr || actor == nullptr) return -1;
        const Inventory* inv = inventory_of(world, actor);
        return inv ? static_cast<int>(inv->stacks.size()) : 0;
    });
}

int sim_world_stack_at(const SimWorld* world, const char* actor, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || actor == nullptr || out == nullptr || cap <= 0) return -1;
        const Inventory* inv = inventory_of(world, actor);
        if (inv == nullptr || index < 0 || static_cast<std::size_t>(index) >= inv->stacks.size()) return -2;
        return write_req(out, cap, stack_record(inv->stacks[static_cast<std::size_t>(index)]));
    });
}

int sim_world_item_def(const SimWorld* world, const char* item, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || item == nullptr || out == nullptr || cap <= 0) return -1;
        const std::optional<ItemDef> d = item_def(world->world.db, Id(item));
        if (!d) return -2;
        std::string flags;
        for (const std::string& f : d->flags) flags += (flags.empty() ? "" : "|") + f;
        return write_req(out, cap, d->ui_category + ';' + std::to_string(d->weight_g) + ';' +
                                       std::to_string(d->stack_max) + ';' + std::to_string(d->spoil_days) +
                                       ';' + flags);
    });
}

}  // extern "C"
