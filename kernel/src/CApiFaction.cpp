// CApiFaction.cpp — W5-B: the C ABI over faction politics (sim/CApiFaction.h).
// Thin: every entry point delegates to sim/Faction.hpp / sim/WildActions.hpp,
// and none lets an exception cross into the engine (the CApiWild.cpp pattern).
// CApi.cpp is not touched: standing query/add already live there
// (sim_world_standing / sim_world_add_standing); this file carries the rest.
#include "sim/CApiFaction.h"

#include "sim/Faction.hpp"
#include "sim/WildActions.hpp"

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

std::string join(const std::vector<Id>& v, char sep) {
    std::string out;
    for (const Id& x : v) {
        if (!out.empty()) out += sep;
        out += x;
    }
    return out;
}

}  // namespace

extern "C" {

int sim_world_faction_tier(const SimWorld* world, const char* faction, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || faction == nullptr) return -1;
        return write_req(out, cap, tier_of(standing(world->world.faction, Id(faction))));
    });
}

int sim_world_oath_count(const SimWorld* world) {
    return guard([&] {
        return world ? static_cast<int>(world->world.faction.oaths.size()) : -1;
    });
}

int sim_world_oath(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || index < 0 ||
            index >= static_cast<int>(world->world.faction.oaths.size()))
            return -1;
        const Oath& o = world->world.faction.oaths[static_cast<std::size_t>(index)];
        return write_req(out, cap, o.id + ";" + o.swearer + ";" + o.to_faction + ";" +
                                       std::to_string(o.day) + ";" + (o.broken ? "1" : "0"));
    });
}

int sim_world_oath_breaker_curse(const SimWorld* world) {
    return guard([&] { return world ? (world->world.faction.oath_breaker_curse ? 1 : 0) : -1; });
}

int sim_world_is_outlawed(const SimWorld* world, const char* faction) {
    return guard([&] {
        if (world == nullptr || faction == nullptr) return -1;
        return is_outlawed(world->world.faction, Id(faction)) ? 1 : 0;
    });
}

int sim_world_treaty_count(const SimWorld* world) {
    return guard([&] {
        return world ? static_cast<int>(world->world.wild.treaty_defs.size()) : -1;
    });
}

int sim_world_treaty(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || index < 0 ||
            index >= static_cast<int>(world->world.wild.treaty_defs.size()))
            return -1;
        const TreatyDef& t = world->world.wild.treaty_defs[static_cast<std::size_t>(index)];
        return write_req(out, cap, t.id + ";" + t.name + ";" + join(t.parties, '|') + ";" + t.terms);
    });
}

int sim_world_treaty_live_between(const SimWorld* world, const char* a, const char* b) {
    return guard([&] {
        if (world == nullptr || a == nullptr || b == nullptr) return -1;
        return live_treaty_between(world->world, Id(a), Id(b)) != nullptr ? 1 : 0;
    });
}

int sim_world_band_politics(const SimWorld* world, const char* group, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || group == nullptr) return -1;
        const BandPolitics p = band_politics(world->world, Id(group));
        return write_req(out, cap, p.faction + ";" + std::to_string(p.net_pts) + ";" +
                                       (p.blocked ? "1" : "0") + ";" + p.note);
    });
}

}  // extern "C"
