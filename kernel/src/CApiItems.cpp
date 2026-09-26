// CApiItems.cpp — P0a: the C ABI over item stacks, carrying, world items,
// containers, timed actions and notices (sim/CApiItems.h).
#include "sim/CApiItems.h"

#include "sim/ItemActions.hpp"

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

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

std::vector<Id> split_ids(const char* list) {
    std::vector<Id> out;
    if (list == nullptr) return out;
    std::stringstream ss{std::string(list)};
    for (std::string s; std::getline(ss, s, ';');)
        if (!s.empty()) out.push_back(s);
    return out;
}

// Records the verb's reason and returns its code.
int finish(SimWorld* world, const ItemResult& r) {
    world->last_reason = r.reason;
    return r.code;
}

int refuse_null(SimWorld* world) {
    if (world != nullptr) world->last_reason = "refuse.bad_argument";
    return -1;
}

std::size_t index_or_max(int index) {
    return index < 0 ? static_cast<std::size_t>(-1) : static_cast<std::size_t>(index);
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

int sim_world_drop(SimWorld* world, const char* actor, int stack_index, int qty, const char* place,
                   int x_cm, int y_cm, int z_cm, char* id_out, int cap) {
    return guard([&] {
        if (world == nullptr || actor == nullptr || place == nullptr) return refuse_null(world);
        const ItemResult r = drop_item(world->world, Id(actor), index_or_max(stack_index), qty, Id(place),
                                       x_cm, y_cm, z_cm);
        if (r.code >= 0 && id_out != nullptr && cap > 0) write_str(id_out, cap, r.id);
        return finish(world, r);
    });
}

int sim_world_pick_up(SimWorld* world, const char* actor, const char* world_item, int qty,
                      const char* witnesses_semicolon) {
    return guard([&] {
        if (world == nullptr || actor == nullptr || world_item == nullptr) return refuse_null(world);
        return finish(world, pick_up(world->world, Id(actor), Id(world_item), qty,
                                     split_ids(witnesses_semicolon)));
    });
}

int sim_world_world_item_count(const SimWorld* world, const char* place) {
    return guard([&] {
        if (world == nullptr || place == nullptr) return -1;
        return static_cast<int>(world_items_at(world->world, Id(place)).size());
    });
}

int sim_world_world_item_at(const SimWorld* world, const char* place, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || place == nullptr || out == nullptr || cap <= 0) return -1;
        const std::vector<const WorldItem*> at = world_items_at(world->world, Id(place));
        if (index < 0 || static_cast<std::size_t>(index) >= at.size()) return -2;
        const WorldItem& wi = *at[static_cast<std::size_t>(index)];
        return write_req(out, cap, wi.id + ';' + std::to_string(wi.x_cm) + ';' + std::to_string(wi.y_cm) + ';' +
                                       std::to_string(wi.z_cm) + ';' + stack_record(wi.stack));
    });
}

int sim_world_add_container(SimWorld* world, const char* id, const char* place, const char* kind,
                            const char* owner, int capacity_g) {
    return guard([&] {
        if (world == nullptr || id == nullptr || place == nullptr || kind == nullptr || owner == nullptr)
            return refuse_null(world);
        return finish(world, add_container(world->world, Id(id), Id(place), kind, Id(owner), capacity_g));
    });
}

int sim_world_container_stack_count(const SimWorld* world, const char* container) {
    return guard([&] {
        if (world == nullptr || container == nullptr) return -1;
        const auto it = world->world.world_items.containers.find(Id(container));
        if (it == world->world.world_items.containers.end()) return -2;
        return static_cast<int>(it->second.contents.stacks.size());
    });
}

int sim_world_container_stack_at(const SimWorld* world, const char* container, int index, char* out,
                                 int cap) {
    return guard([&] {
        if (world == nullptr || container == nullptr || out == nullptr || cap <= 0) return -1;
        const auto it = world->world.world_items.containers.find(Id(container));
        if (it == world->world.world_items.containers.end()) return -2;
        const std::vector<ItemStack>& stacks = it->second.contents.stacks;
        if (index < 0 || static_cast<std::size_t>(index) >= stacks.size()) return -2;
        return write_req(out, cap, stack_record(stacks[static_cast<std::size_t>(index)]));
    });
}

int sim_world_put_in(SimWorld* world, const char* actor, const char* container, int stack_index, int qty) {
    return guard([&] {
        if (world == nullptr || actor == nullptr || container == nullptr) return refuse_null(world);
        return finish(world, put_in(world->world, Id(actor), Id(container), index_or_max(stack_index), qty));
    });
}

int sim_world_take_out(SimWorld* world, const char* actor, const char* container, int stack_index,
                       int qty, const char* witnesses_semicolon) {
    return guard([&] {
        if (world == nullptr || actor == nullptr || container == nullptr) return refuse_null(world);
        return finish(world, take_out(world->world, Id(actor), Id(container), index_or_max(stack_index), qty,
                                      split_ids(witnesses_semicolon)));
    });
}

}  // extern "C"
