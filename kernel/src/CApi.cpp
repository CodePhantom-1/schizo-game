// CApi.cpp — the C ABI implementation (coordinator-owned). Thin: everything
// here delegates to the kernel's own API; no logic lives at the boundary.
#include "sim/CApi.h"

#include "sim/World.hpp"

#include <cstring>
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
