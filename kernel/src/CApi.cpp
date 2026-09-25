// CApi.cpp — the C ABI implementation (coordinator-owned). Thin: everything
// here delegates to the kernel's own API; no logic lives at the boundary.
//
// No C++ exception may cross into the engine (bug review 2026-09-25): every
// entry point catches everything and returns its documented error value
// (-1 / nullptr / no-op). A throw mid-advance leaves the world as far as it
// got; the handle stays valid.
#include "sim/CApi.h"

#include "sim/World.hpp"
#include "sim/Snapshot.hpp"
#include "sim/Schedule.hpp"

#include <algorithm>
#include <climits>
#include <cstring>
#include <filesystem>
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
// A canon directory is one that holds seasons.csv (WorldState::init builds the
// calendar from it). Db::load skips absent tables, so without this check a
// wrong path yields a valid-but-empty world instead of the documented nullptr.
bool canon_present(const char* canon_dir) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(canon_dir) / "seasons.csv", ec);
}

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
    try {
        if (canon_dir == nullptr || !canon_present(canon_dir)) return nullptr;
        SimWorld* w = new (std::nothrow) SimWorld();
        if (w == nullptr) return nullptr;
        try {
            w->world.init(canon_dir, seed);
        } catch (...) {
            delete w;
            return nullptr;
        }
        return w;
    } catch (...) {
        return nullptr;
    }
}

void sim_world_destroy(SimWorld* world) { delete world; }

void sim_world_advance_days(SimWorld* world, int days) {
    try {
        if (world != nullptr && days > 0) world->world.advance_days(days);
    } catch (...) {
        return;
    }
}

int64_t sim_world_day(const SimWorld* world) {
    try {
        return world ? world->world.day : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_season(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, world->world.cal.season_id(world->world.day));
    } catch (...) {
        return -1;
    }
}

void sim_world_set_drought(SimWorld* world, int stage) {
    try {
        if (world != nullptr) world->world.facts.drought_stage = stage;
    } catch (...) {
        return;
    }
}

int sim_world_drought(const SimWorld* world) {
    try {
        return world ? world->world.facts.drought_stage : -1;
    } catch (...) {
        return -1;
    }
}

void sim_world_set_war(SimWorld* world, int stage) {
    try {
        if (world != nullptr) world->world.facts.war_stage = stage;
    } catch (...) {
        return;
    }
}

int sim_world_war(const SimWorld* world) {
    try {
        return world ? world->world.facts.war_stage : -1;
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_price(const SimWorld* world, const char* city, const char* item) {
    try {
        if (world == nullptr || city == nullptr || item == nullptr) return -1;
        return price_of(world->world.economy, Id(city), Id(item));
    } catch (...) {
        return -1;
    }
}

int sim_world_standing(const SimWorld* world, const char* faction) {
    try {
        if (world == nullptr || faction == nullptr) return -1;
        return standing(world->world.faction, Id(faction));
    } catch (...) {
        return -1;
    }
}

void sim_world_add_standing(SimWorld* world, const char* faction, int delta) {
    try {
        if (world != nullptr && faction != nullptr) add_standing(world->world.faction, Id(faction), delta);
    } catch (...) {
        return;
    }
}

int sim_world_favour(const SimWorld* world, const char* deity) {
    try {
        if (world == nullptr || deity == nullptr) return -1;
        return favour(world->world.magic, Id(deity));
    } catch (...) {
        return -1;
    }
}

void sim_world_add_favour(SimWorld* world, const char* deity, int delta) {
    try {
        if (world != nullptr && deity != nullptr) add_favour(world->world.magic, Id(deity), delta);
    } catch (...) {
        return;
    }
}

int sim_world_verdict_count(const SimWorld* world) {
    try {
        return world ? static_cast<int>(world->world.justice.verdicts.size()) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_event_count(const SimWorld* world) {
    try {
        return world ? static_cast<int>(world->world.events.fired.size()) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_npc_count(const SimWorld* world) {
    try {
        return world ? static_cast<int>(world->world.population.npcs.size()) : -1;
    } catch (...) {
        return -1;
    }
}

// Save / load ----------------------------------------------------------------
int sim_world_save(const SimWorld* world, const char* path) {
    try {
        if (world == nullptr || path == nullptr) return -1;
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return -2;
        f << save_world(world->world);
        return f.good() ? 0 : -2;
    } catch (...) {
        return -1;
    }
}

SimWorld* sim_world_load(const char* canon_dir, const char* path) {
    try {
        if (canon_dir == nullptr || path == nullptr || !canon_present(canon_dir)) return nullptr;
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
    } catch (...) {
        return nullptr;
    }
}

int sim_world_save_to_buffer(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, save_world(world->world));
    } catch (...) {
        return -1;
    }
}

SimWorld* sim_world_load_from_buffer(const char* canon_dir, const char* data) {
    try {
        if (canon_dir == nullptr || data == nullptr || !canon_present(canon_dir)) return nullptr;
        SimWorld* w = new (std::nothrow) SimWorld();
        if (w == nullptr) return nullptr;
        try {
            load_world(w->world, canon_dir, std::string(data));
        } catch (...) {
            delete w;
            return nullptr;
        }
        return w;
    } catch (...) {
        return nullptr;
    }
}

// Needs ------------------------------------------------------------------
int sim_world_hunger(const SimWorld* world, const char* actor) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        auto it = world->world.needs.by_actor.find(actor);
        return it == world->world.needs.by_actor.end() ? 0 : it->second.hunger;
    } catch (...) {
        return -1;
    }
}

int sim_world_thirst(const SimWorld* world, const char* actor) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        auto it = world->world.needs.by_actor.find(actor);
        return it == world->world.needs.by_actor.end() ? 0 : it->second.thirst;
    } catch (...) {
        return -1;
    }
}

int sim_world_fatigue(const SimWorld* world, const char* actor) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        auto it = world->world.needs.by_actor.find(actor);
        return it == world->world.needs.by_actor.end() ? 0 : it->second.fatigue;
    } catch (...) {
        return -1;
    }
}

void sim_world_advance_needs(SimWorld* world, const char* actor, int hours, int sleeping) {
    try {
        if (world == nullptr || actor == nullptr) return;
        advance_needs(world->world.needs, Id(actor), hours, sleeping != 0);
    } catch (...) {
        return;
    }
}

int sim_world_eat(SimWorld* world, const char* actor, const char* item) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        // Look up, never insert: a refused call must not add an inventory (and
        // so change the save bytes) for an actor the world has never seen.
        const int have = sim_world_item_count(world, actor, item);
        if (have < 0) return -1;  // internal failure, not "none held"
        if (have < 1) return -2;
        std::string reason;
        if (!eat(world->world.db, world->world.needs, Id(actor), Id(item), &reason)) return -3;
        world->world.inventories[actor].counts[item] = have - 1;
        return 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_drink(SimWorld* world, const char* actor, const char* item) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        const bool is_water = std::strcmp(item, "water") == 0;
        int have = 0;
        if (!is_water) {
            have = sim_world_item_count(world, actor, item);  // look up, never insert
            if (have < 0) return -1;  // internal failure, not "none held"
            if (have < 1) return -2;
        }
        std::string reason;
        if (!drink(world->world.db, world->world.needs, Id(actor), Id(item), &reason)) return -3;
        if (!is_water) world->world.inventories[actor].counts[item] = have - 1;
        return 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_need_effects(const SimWorld* world, const char* actor, char* out, int cap) {
    try {
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
    } catch (...) {
        return -1;
    }
}

// Inventory ------------------------------------------------------------------
int sim_world_item_count(const SimWorld* world, const char* actor, const char* item) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        auto ait = world->world.inventories.find(actor);
        if (ait == world->world.inventories.end()) return 0;
        auto it = ait->second.counts.find(item);
        return it == ait->second.counts.end() ? 0 : it->second;
    } catch (...) {
        return -1;
    }
}

int sim_world_give_item(SimWorld* world, const char* actor, const char* item, int qty) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        Inventory& inv = world->world.inventories[actor];
        // Widened and saturated: count + qty must not overflow int.
        const long long sum = static_cast<long long>(inv.counts[item]) + qty;
        const int cur = static_cast<int>(std::clamp<long long>(sum, 0, INT_MAX));
        inv.counts[item] = cur;
        return cur;
    } catch (...) {
        return -1;
    }
}

// Crafting ---------------------------------------------------------------
int sim_world_craft(SimWorld* world, const char* actor, const char* recipe,
                     const char* stations_semicolon_list, int times) {
    try {
        if (world == nullptr || actor == nullptr || recipe == nullptr || times <= 0) return -1;
        std::set<Id> stations;
        if (stations_semicolon_list != nullptr) {
            std::stringstream ss{std::string(stations_semicolon_list)};
            std::string entry;
            while (std::getline(ss, entry, ';'))
                if (!entry.empty()) stations.insert(entry);
        }
        // Craft on a copy and write back only on success: a refused craft must
        // not add an empty inventory for an unseen actor (craft() itself leaves
        // the inventory untouched on failure).
        const auto found = world->world.inventories.find(actor);
        Inventory inv = found == world->world.inventories.end() ? Inventory{} : found->second;
        std::string reason;
        if (craft(world->world.db, inv, Id(recipe), stations, times, &reason)) {
            world->world.inventories[actor] = std::move(inv);
            return 0;
        }
        if (reason.rfind("unknown, OPEN or malformed recipe", 0) == 0) return -2;
        if (reason.rfind("station not at hand", 0) == 0) return -3;
        if (reason.rfind("missing input", 0) == 0) return -4;
        return -1;  // times <= 0/out of range guarded above and by check(); anything else falls back here
    } catch (...) {
        return -1;
    }
}

// Schedule ---------------------------------------------------------------
int sim_world_task_at(const SimWorld* world, const char* role, int hour, char* out, int cap) {
    try {
        if (world == nullptr || role == nullptr) return -1;
        const std::optional<ScheduledTask> t =
            task_at(world->world.db, world->world.cal, role, world->world.day, hour);
        if (!t) return -1;
        return write_str(out, cap, t->task);
    } catch (...) {
        return -1;
    }
}
