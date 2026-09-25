// CApiVerbs.cpp — W6-A: the C ABI over the player's carried goods (sim/CApiVerbs.h).
// Thin and read-only: enumeration and best-carried-food/drink queries over
// WorldState::inventories plus Needs' category table (Needs.hpp's *_restore_of).
// No exception crosses into the engine (the CApiFaction.cpp pattern), and
// nothing here writes — the write verbs (give_item, eat, drink, rest) already
// live in CApi.cpp and stay the only writers.
#include "sim/CApiVerbs.h"

#include "sim/Needs.hpp"

#include <cstring>
#include <string>

#include "CApiInternal.hpp"

using namespace sim;

namespace {

int write_str(char* out, int cap, const std::string& s) {
    if (out == nullptr || cap <= 0) return static_cast<int>(s.size());  // text is optional for verbs
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
int guard(F&& f) {
    try {
        return f();
    } catch (...) {
        return -1;
    }
}

// The held item whose restore amount (`restore_of`) is highest; "" when the
// actor holds nothing with a positive restore. Iteration is over the
// inventory's std::map (sorted by id) with a strict >, so ties break to the
// first id in sorted order — deterministic.
template <class RestoreOf>
std::string best_held(const WorldState& w, const Id& actor, RestoreOf restore_of) {
    const auto ait = w.inventories.find(actor);
    if (ait == w.inventories.end()) return std::string();
    const std::string* best = nullptr;
    int best_restore = 0;
    for (const auto& [item, count] : ait->second.counts) {
        if (count <= 0) continue;  // give_item clamps at 0; zero rows never win
        const int restore = restore_of(w.db, item);
        if (restore > best_restore) {
            best_restore = restore;
            best = &item;
        }
    }
    return best ? *best : std::string();
}

}  // namespace

extern "C" {

int sim_world_inventory(const SimWorld* world, const char* actor, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || actor == nullptr) return -1;
        const auto ait = world->world.inventories.find(Id(actor));
        if (ait == world->world.inventories.end()) return write_req(out, cap, std::string());
        std::string joined;
        for (const auto& [item, count] : ait->second.counts) {
            if (count <= 0) continue;  // consumed-to-zero rows are not carried goods
            if (!joined.empty()) joined += ';';
            joined += item + ":" + std::to_string(count);
        }
        return write_req(out, cap, joined);
    });
}

int sim_world_best_food(const SimWorld* world, const char* actor, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || actor == nullptr) return -1;
        return write_req(out, cap, best_held(world->world, Id(actor), hunger_restore_of));
    });
}

int sim_world_best_drink(const SimWorld* world, const char* actor, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || actor == nullptr) return -1;
        return write_req(out, cap, best_held(world->world, Id(actor), thirst_restore_of));
    });
}

}  // extern "C"

int sim_world_set_need(SimWorld* world, const char* actor, const char* need, int value) {
    return guard([&] {
        if (world == nullptr || actor == nullptr || need == nullptr) return -1;
        const std::string which = need;
        if (which != "hunger" && which != "thirst" && which != "fatigue") return -1;
        Needs& n = needs_of(world->world.needs, Id(actor));
        const int v = value < 0 ? 0 : (value > 100 ? 100 : value);
        if (which == "hunger") n.hunger = v;
        else if (which == "thirst") n.thirst = v;
        else n.fatigue = v;
        return 0;
    });
}
