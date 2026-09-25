// CApiWild.cpp — W4-C: the C ABI over the wild lands (sim/CApiWild.h).
// Thin: every entry point delegates to sim/Wild.hpp / sim/WildActions.hpp,
// and none lets an exception cross into the engine (the CApi.cpp pattern).
#include "sim/CApiWild.h"

#include "sim/WildActions.hpp"

#include <cstring>
#include <string>

#include "CApiInternal.hpp"

using namespace sim;

// The opaque handle. MUST stay token-for-token identical to the definition
// in CApi.cpp (one class, defined in two translation units — legal only while
// identical). At merge, if CApi.cpp's SimWorld ever grows, move both into a
// shared kernel/src/CApiInternal.hpp.
// SimWorld is defined once, in CApiInternal.hpp.

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

std::string i(std::int64_t v) { return std::to_string(v); }

int verb(char* out, int cap, const WildActionResult& r) {
    write_str(out, cap, r.text);
    return r.code;
}

template <class Map>
const typename Map::mapped_type* nth(const Map& m, int index) {
    if (index < 0 || index >= static_cast<int>(m.size())) return nullptr;
    auto it = m.begin();
    std::advance(it, index);
    return &it->second;
}

}  // namespace

extern "C" {

int sim_world_wild_place_count(const SimWorld* world) {
    return guard([&] { return world ? static_cast<int>(world->world.wild.nodes.size()) : -1; });
}

int sim_world_wild_place(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr) return -1;
        const WildNode* n = nth(world->world.wild.nodes, index);
        if (n == nullptr) return -1;
        return write_req(out, cap,
                         n->id + ";" + n->kind + ";" + n->source + ";" + i(n->danger) + ";" + i(n->x_m) +
                             ";" + i(n->y_m) + ";" + (n->camp_site ? "1" : "0") + ";" + n->edge_to + ";" +
                             n->name);
    });
}

int sim_world_wild_link_count(const SimWorld* world) {
    return guard([&] { return world ? static_cast<int>(world->world.wild.links.size()) : -1; });
}

int sim_world_wild_link(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || index < 0 ||
            index >= static_cast<int>(world->world.wild.links.size()))
            return -1;
        const WildLink& l = world->world.wild.links[static_cast<std::size_t>(index)];
        return write_req(out, cap, l.id + ";" + l.a + ";" + l.b + ";" + l.kind + ";" +
                                       i(l.distance_m) + ";" + i(l.danger));
    });
}

int sim_world_player_place(const SimWorld* world, char* out, int cap) {
    return guard([&] { return world ? write_req(out, cap, world->world.wild.player_place) : -1; });
}

int sim_world_set_player_place(SimWorld* world, const char* place) {
    return guard([&] {
        if (world == nullptr || place == nullptr) return -1;
        if (find_node(world->world.wild, place) == nullptr) return -2;
        world->world.wild.player_place = place;
        return 0;
    });
}

int sim_world_weather(const SimWorld* world, char* out, int cap) {
    return guard([&] {
        return world ? write_req(out, cap, weather_on(world->world, world->world.day)) : -1;
    });
}

int sim_world_route_minutes(const SimWorld* world, const char* to, const char* mode) {
    return guard([&] {
        if (world == nullptr || to == nullptr || mode == nullptr) return -1;
        const WorldState& w = world->world;
        const std::vector<Id> path = find_route(w, w.wild.player_place, to, mode);
        if (path.empty()) return -5;
        const auto m = w.wild.modes.find(mode);
        const Id weather = weather_on(w, w.day);
        int minutes = 0;
        for (std::size_t k = 1; k < path.size(); ++k) {
            int best = -1;
            for (const WildLink& l : w.wild.links) {
                if (!((l.a == path[k - 1] && l.b == path[k]) || (l.b == path[k - 1] && l.a == path[k])))
                    continue;
                bool ok = false;
                for (const std::string& kind : m->second.link_kinds) ok = ok || kind == l.kind;
                if (!ok) continue;
                const int mins = leg_minutes(w.wild, l, m->second, weather);
                if (best < 0 || mins < best) best = mins;
            }
            minutes += best < 0 ? 0 : best;
        }
        return minutes;
    });
}

int sim_world_travel(SimWorld* world, const char* to, const char* mode, int start_hour,
                     const char* stance, int companions, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || to == nullptr || mode == nullptr) return -1;
        const TravelResult r = travel(world->world, to, mode, start_hour,
                                      stance && *stance ? stance : "fight", companions);
        write_str(out, cap,
                  "minutes=" + i(r.minutes) + ";stopped_at=" + r.stopped_at + ";halted_by=" +
                      (r.code < 0 ? r.refusal : r.halted_by) + ";encounters=" + i(static_cast<std::int64_t>(r.encounters.size())));
        return r.code;
    });
}

int sim_world_encounter_count(const SimWorld* world) {
    return guard([&] { return world ? static_cast<int>(world->world.wild.encounters.size()) : -1; });
}

int sim_world_encounter(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || index < 0 ||
            index >= static_cast<int>(world->world.wild.encounters.size()))
            return -1;
        const EncounterRecord& e = world->world.wild.encounters[static_cast<std::size_t>(index)];
        return write_req(out, cap, e.id + ";" + i(e.day) + ";" + i(e.hour) + ";" + e.link + ";" +
                                       e.encounter + ";" + e.kind + ";" + e.group + ";" + e.stance +
                                       ";" + e.outcome + ";" + i(e.foes) + ";" + i(e.foes_lost) +
                                       ";" + i(e.allies_lost));
    });
}

int sim_world_camp_count(const SimWorld* world) {
    return guard([&] { return world ? static_cast<int>(world->world.wild.camps.size()) : -1; });
}

int sim_world_camp(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr) return -1;
        const Camp* c = nth(world->world.wild.camps, index);
        if (c == nullptr) return -1;
        const Group* g = find_group(world->world.wild, c->group);
        const bool live = g && g->active && c->status == "occupied";
        int loot = 0;
        for (const auto& [item, units] : c->loot) loot += units;
        return write_req(out, cap,
                         c->place + ";" + c->group + ";" + c->status + ";" + i(live ? g->strength : 0) +
                             ";" + i(live ? g->supplies : 0) + ";" + i(live ? g->morale : 0) + ";" +
                             (live ? g->leader : "") + ";" + i(c->lookout) + ";" + i(loot) + ";" +
                             join(c->captives, '|') + ";" + (c->known_to_player ? "1" : "0") + ";" +
                             (c->scouted ? "1" : "0"));
    });
}

int sim_world_group_count(const SimWorld* world) {
    return guard([&] { return world ? static_cast<int>(world->world.wild.groups.size()) : -1; });
}

int sim_world_group(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr) return -1;
        const Group* g = nth(world->world.wild.groups, index);
        if (g == nullptr) return -1;
        return write_req(out, cap,
                         g->id + ";" + (g->active ? "1" : "0") + ";" + g->camp + ";" + g->leader + ";" +
                             i(g->strength) + ";" + i(g->supplies) + ";" + i(g->morale) + ";" +
                             i(g->grudge_vs_player) + ";" + i(g->paid_until) + ";" +
                             (g->allied ? "1" : "0") + ";" + (g->player_member ? "1" : "0") + ";" +
                             (g->player_led ? "1" : "0") + ";" + i(g->times_cleared));
    });
}

int sim_world_raid_chance(const SimWorld* world, const char* group) {
    return guard([&] {
        if (world == nullptr || group == nullptr) return -1;
        return assess_raid(world->world, group, world->world.day).chance_bp;
    });
}

int sim_world_raid_count(const SimWorld* world) {
    return guard([&] { return world ? static_cast<int>(world->world.wild.raids.size()) : -1; });
}

int sim_world_raid(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || index < 0 || index >= static_cast<int>(world->world.wild.raids.size()))
            return -1;
        const RaidRecord& r = world->world.wild.raids[static_cast<std::size_t>(index)];
        return write_req(out, cap, r.id + ";" + i(r.day) + ";" + r.group + ";" + r.target + ";" +
                                       r.place + ";" + i(r.chance_bp) + ";" + (r.success ? "1" : "0") +
                                       ";" + i(r.goods_taken) + ";" + join(r.slain, '|') + ";" +
                                       join(r.taken, '|') + ";" + r.summary);
    });
}

int sim_world_caravan_count(const SimWorld* world) {
    return guard([&] { return world ? static_cast<int>(world->world.wild.caravans.size()) : -1; });
}

int sim_world_caravan(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || index < 0 ||
            index >= static_cast<int>(world->world.wild.caravans.size()))
            return -1;
        const CaravanRun& run = world->world.wild.caravans[static_cast<std::size_t>(index)];
        std::string name, at;
        for (const CaravanDef& cd : world->world.wild.caravan_defs)
            if (cd.id == run.def) {
                name = cd.name;
                if (run.leg >= 0 && run.leg < static_cast<int>(cd.route.size()))
                    at = cd.route[static_cast<std::size_t>(run.leg)];
            }
        int goods = 0;
        for (const auto& [item, units] : run.goods) goods += units;
        return write_req(out, cap, run.id + ";" + run.def + ";" + name + ";" + at + ";" +
                                       i(run.guards) + ";" + i(goods) + ";" + (run.escorted ? "1" : "0"));
    });
}

int sim_world_npc_fate(const SimWorld* world, const char* npc, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || npc == nullptr) return -1;
        return write_req(out, cap, npc_fate(world->world.wild, npc));
    });
}

int sim_world_hear_rumours(SimWorld* world, const char* npc, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || npc == nullptr) return -1;
        std::string text;
        for (const std::string& r : hear_rumours(world->world, npc)) {
            if (!text.empty()) text += '\n';
            text += r;
        }
        return write_req(out, cap, text);
    });
}

int sim_world_herd_head(const SimWorld* world) {
    return guard([&] { return world ? world->world.wild.herd_head : -1; });
}

int sim_world_player_wounds(const SimWorld* world) {
    return guard([&] { return world ? world->world.wild.player_wounds : -1; });
}

int sim_world_infamy(const SimWorld* world) {
    return guard([&] { return world ? world->world.wild.infamy : -1; });
}

int sim_world_player_group(const SimWorld* world, char* out, int cap) {
    return guard([&] { return world ? write_req(out, cap, world->world.wild.player_group) : -1; });
}

int sim_world_scout_camp(SimWorld* world, const char* place, int hour, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || place == nullptr) return -1;
        return verb(out, cap, scout_camp(world->world, place, hour));
    });
}

int sim_world_attack_camp(SimWorld* world, const char* place, int companions, int hour, char* out,
                          int cap) {
    return guard([&] {
        if (world == nullptr || place == nullptr) return -1;
        return verb(out, cap, attack_camp(world->world, place, companions, hour));
    });
}

int sim_world_infiltrate_camp(SimWorld* world, const char* place, const char* objective, int hour,
                              char* out, int cap) {
    return guard([&] {
        if (world == nullptr || place == nullptr || objective == nullptr) return -1;
        return verb(out, cap, infiltrate_camp(world->world, place, objective, hour));
    });
}

int sim_world_pay_off_group(SimWorld* world, const char* group, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || group == nullptr) return -1;
        return verb(out, cap, pay_off_group(world->world, group));
    });
}

int sim_world_ally_with_group(SimWorld* world, const char* group, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || group == nullptr) return -1;
        return verb(out, cap, ally_with_group(world->world, group));
    });
}

int sim_world_join_group(SimWorld* world, const char* group, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || group == nullptr) return -1;
        return verb(out, cap, join_group(world->world, group));
    });
}

int sim_world_leave_group(SimWorld* world, char* out, int cap) {
    return guard([&] {
        if (world == nullptr) return -1;
        return verb(out, cap, leave_group(world->world));
    });
}

int sim_world_challenge_leader(SimWorld* world, const char* group, int hour, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || group == nullptr) return -1;
        return verb(out, cap, challenge_leader(world->world, group, hour));
    });
}

int sim_world_found_band(SimWorld* world, const char* camp_place, int companions, char* out,
                         int cap) {
    return guard([&] {
        if (world == nullptr || camp_place == nullptr) return -1;
        return verb(out, cap, found_band(world->world, camp_place, companions));
    });
}

int sim_world_lead_raid(SimWorld* world, const char* target, int hour, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || target == nullptr) return -1;
        return verb(out, cap, lead_raid(world->world, target, hour));
    });
}

int sim_world_ransom_captive(SimWorld* world, const char* npc, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || npc == nullptr) return -1;
        return verb(out, cap, ransom_captive(world->world, npc));
    });
}

int sim_world_escort_caravan(SimWorld* world, const char* run, int companions, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || run == nullptr) return -1;
        return verb(out, cap, escort_caravan(world->world, run, companions));
    });
}

int sim_world_abandon_escort(SimWorld* world, char* out, int cap) {
    return guard([&] {
        if (world == nullptr) return -1;
        return verb(out, cap, abandon_escort(world->world));
    });
}

int sim_world_rob_caravan(SimWorld* world, const char* run, int companions, int hour, char* out,
                          int cap) {
    return guard([&] {
        if (world == nullptr || run == nullptr) return -1;
        return verb(out, cap, rob_caravan(world->world, run, companions, hour));
    });
}

int sim_world_search_site(SimWorld* world, const char* place, int hour, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || place == nullptr) return -1;
        return verb(out, cap, search_site(world->world, place, hour));
    });
}

}  // extern "C"
