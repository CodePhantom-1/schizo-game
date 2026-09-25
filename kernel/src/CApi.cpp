// CApi.cpp — the C ABI implementation (coordinator-owned). Thin: everything
// here delegates to the kernel's own API; no logic lives at the boundary.
#include "sim/CApi.h"

#include "sim/World.hpp"
#include "sim/Snapshot.hpp"
#include "sim/Schedule.hpp"

#include <cstring>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>

using namespace sim;

struct SimWorld {
    WorldState world;
};

namespace {
int write_str(char* out, int cap, const std::string& s) {
    if (out == nullptr || cap <= 0) return -1;
    const int len = static_cast<int>(s.size());
    const int copy = (cap - 1 < len) ? cap - 1 : len;
    std::memcpy(out, s.data(), static_cast<std::size_t>(copy));
    out[copy] = '\0';
    return len;
}
}  // namespace

SimWorld* sim_world_create(const char* canon_dir, uint64_t seed) {
    if (canon_dir == nullptr) return nullptr;
    SimWorld* w = new (std::nothrow) SimWorld();
    if (w == nullptr) return nullptr;
    try {
        w->world.init(canon_dir, seed);
    } catch (...) {
        delete w;
        return nullptr;
    }
    return w;
}

void sim_world_destroy(SimWorld* world) { delete world; }

void sim_world_advance_days(SimWorld* world, int days) {
    if (world != nullptr && days > 0) world->world.advance_days(days);
}

int64_t sim_world_day(const SimWorld* world) { return world ? world->world.day : -1; }

int sim_world_season(const SimWorld* world, char* out, int cap) {
    if (world == nullptr) return -1;
    return write_str(out, cap, world->world.cal.season_id(world->world.day));
}

void sim_world_set_drought(SimWorld* world, int stage) {
    if (world != nullptr) world->world.facts.drought_stage = stage;
}

int sim_world_drought(const SimWorld* world) { return world ? world->world.facts.drought_stage : -1; }

void sim_world_set_war(SimWorld* world, int stage) {
    if (world != nullptr) world->world.facts.war_stage = stage;
}

int sim_world_war(const SimWorld* world) { return world ? world->world.facts.war_stage : -1; }

int64_t sim_world_price(const SimWorld* world, const char* city, const char* item) {
    if (world == nullptr || city == nullptr || item == nullptr) return -1;
    return price_of(world->world.economy, Id(city), Id(item));
}

int sim_world_standing(const SimWorld* world, const char* faction) {
    if (world == nullptr || faction == nullptr) return -1;
    return standing(world->world.faction, Id(faction));
}

void sim_world_add_standing(SimWorld* world, const char* faction, int delta) {
    if (world != nullptr && faction != nullptr) add_standing(world->world.faction, Id(faction), delta);
}

int sim_world_favour(const SimWorld* world, const char* deity) {
    if (world == nullptr || deity == nullptr) return -1;
    return favour(world->world.magic, Id(deity));
}

void sim_world_add_favour(SimWorld* world, const char* deity, int delta) {
    if (world != nullptr && deity != nullptr) add_favour(world->world.magic, Id(deity), delta);
}

int sim_world_verdict_count(const SimWorld* world) {
    return world ? static_cast<int>(world->world.justice.verdicts.size()) : -1;
}

int sim_world_event_count(const SimWorld* world) {
    return world ? static_cast<int>(world->world.events.fired.size()) : -1;
}

int sim_world_npc_count(const SimWorld* world) {
    return world ? static_cast<int>(world->world.population.npcs.size()) : -1;
}

// Save / load ----------------------------------------------------------------
int sim_world_save(const SimWorld* world, const char* path) {
    if (world == nullptr || path == nullptr) return -1;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return -2;
    f << save_world(world->world);
    return f.good() ? 0 : -2;
}

SimWorld* sim_world_load(const char* canon_dir, const char* path) {
    if (canon_dir == nullptr || path == nullptr) return nullptr;
    std::ifstream f(path, std::ios::binary);
    if (!f) return nullptr;
    std::ostringstream ss;
    ss << f.rdbuf();
    SimWorld* w = new (std::nothrow) SimWorld();
    if (w == nullptr) return nullptr;
    try {
        load_world(w->world, canon_dir, ss.str());
    } catch (...) {
        delete w;
        return nullptr;
    }
    return w;
}

int sim_world_save_to_buffer(const SimWorld* world, char* out, int cap) {
    if (world == nullptr) return -1;
    return write_str(out, cap, save_world(world->world));
}

SimWorld* sim_world_load_from_buffer(const char* canon_dir, const char* data) {
    if (canon_dir == nullptr || data == nullptr) return nullptr;
    SimWorld* w = new (std::nothrow) SimWorld();
    if (w == nullptr) return nullptr;
    try {
        load_world(w->world, canon_dir, std::string(data));
    } catch (...) {
        delete w;
        return nullptr;
    }
    return w;
}

// Needs ------------------------------------------------------------------
int sim_world_hunger(const SimWorld* world, const char* actor) {
    if (world == nullptr || actor == nullptr) return -1;
    auto it = world->world.needs.by_actor.find(actor);
    return it == world->world.needs.by_actor.end() ? 0 : it->second.hunger;
}

int sim_world_thirst(const SimWorld* world, const char* actor) {
    if (world == nullptr || actor == nullptr) return -1;
    auto it = world->world.needs.by_actor.find(actor);
    return it == world->world.needs.by_actor.end() ? 0 : it->second.thirst;
}

int sim_world_fatigue(const SimWorld* world, const char* actor) {
    if (world == nullptr || actor == nullptr) return -1;
    auto it = world->world.needs.by_actor.find(actor);
    return it == world->world.needs.by_actor.end() ? 0 : it->second.fatigue;
}

void sim_world_advance_needs(SimWorld* world, const char* actor, int hours, int sleeping) {
    if (world == nullptr || actor == nullptr) return;
    advance_needs(world->world.needs, Id(actor), hours, sleeping != 0);
}

int sim_world_eat(SimWorld* world, const char* actor, const char* item) {
    if (world == nullptr || actor == nullptr || item == nullptr) return -1;
    Inventory& inv = world->world.inventories[actor];
    auto it = inv.counts.find(item);
    const int have = it == inv.counts.end() ? 0 : it->second;
    if (have < 1) return -2;
    std::string reason;
    if (!eat(world->world.db, world->world.needs, Id(actor), Id(item), &reason)) return -3;
    inv.counts[item] = have - 1;
    return 0;
}

int sim_world_drink(SimWorld* world, const char* actor, const char* item) {
    if (world == nullptr || actor == nullptr || item == nullptr) return -1;
    const bool is_water = std::strcmp(item, "water") == 0;
    Inventory& inv = world->world.inventories[actor];
    int have = 0;
    if (!is_water) {
        auto it = inv.counts.find(item);
        have = it == inv.counts.end() ? 0 : it->second;
        if (have < 1) return -2;
    }
    std::string reason;
    if (!drink(world->world.db, world->world.needs, Id(actor), Id(item), &reason)) return -3;
    if (!is_water) inv.counts[item] = have - 1;
    return 0;
}

int sim_world_need_effects(const SimWorld* world, const char* actor, char* out, int cap) {
    if (world == nullptr || actor == nullptr) return -1;
    auto it = world->world.needs.by_actor.find(actor);
    const Needs n = it == world->world.needs.by_actor.end() ? Needs{} : it->second;
    const std::vector<std::string> effects = need_effects(n);
    std::string joined;
    for (std::size_t i = 0; i < effects.size(); ++i) {
        if (i != 0) joined += ';';
        joined += effects[i];
    }
    return write_str(out, cap, joined);
}

// Inventory ------------------------------------------------------------------
int sim_world_item_count(const SimWorld* world, const char* actor, const char* item) {
    if (world == nullptr || actor == nullptr || item == nullptr) return -1;
    auto ait = world->world.inventories.find(actor);
    if (ait == world->world.inventories.end()) return 0;
    auto it = ait->second.counts.find(item);
    return it == ait->second.counts.end() ? 0 : it->second;
}

int sim_world_give_item(SimWorld* world, const char* actor, const char* item, int qty) {
    if (world == nullptr || actor == nullptr || item == nullptr) return -1;
    Inventory& inv = world->world.inventories[actor];
    int cur = inv.counts[item] + qty;
    if (cur < 0) cur = 0;
    inv.counts[item] = cur;
    return cur;
}

// Crafting ---------------------------------------------------------------
int sim_world_craft(SimWorld* world, const char* actor, const char* recipe,
                     const char* stations_semicolon_list, int times) {
    if (world == nullptr || actor == nullptr || recipe == nullptr || times <= 0) return -1;
    std::set<Id> stations;
    if (stations_semicolon_list != nullptr) {
        std::stringstream ss{std::string(stations_semicolon_list)};
        std::string entry;
        while (std::getline(ss, entry, ';'))
            if (!entry.empty()) stations.insert(entry);
    }
    Inventory& inv = world->world.inventories[actor];
    std::string reason;
    if (craft(world->world.db, inv, Id(recipe), stations, times, &reason)) return 0;
    if (reason.rfind("unknown, OPEN or malformed recipe", 0) == 0) return -2;
    if (reason.rfind("station not at hand", 0) == 0) return -3;
    if (reason.rfind("missing input", 0) == 0) return -4;
    return -1;  // times <= 0/out of range guarded above and by check(); anything else falls back here
}

// Schedule ---------------------------------------------------------------
int sim_world_task_at(const SimWorld* world, const char* role, int hour, char* out, int cap) {
    if (world == nullptr || role == nullptr) return -1;
    const std::optional<ScheduledTask> t =
        task_at(world->world.db, world->world.cal, role, world->world.day, hour);
    if (!t) return -1;
    return write_str(out, cap, t->task);
}
