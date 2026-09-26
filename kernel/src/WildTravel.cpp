// WildTravel.cpp — W4-C: travel between places (mechanics.md rows 31, 35, 36).
// No fast travel: every leg costs game-minutes by transport mode, terrain and
// weather; needs rise hour by hour (thirst faster in heat and on the desert
// tracks); each started hour rolls for an encounter weighted by the link's
// danger, the season, the weather and the time of day. Fights go through the
// combat seam (Wild.hpp SkirmishResolver).
//
// D-022: integer maths only.
#include "WildInternal.hpp"

#include "sim/Actions.hpp"

#include <algorithm>
#include <set>

namespace sim {

using namespace wild;

namespace {

bool mode_allows(const TransportMode& m, const std::string& link_kind) {
    return std::find(m.link_kinds.begin(), m.link_kinds.end(), link_kind) != m.link_kinds.end();
}

const WildLink* best_link(const WorldState& w, const Id& a, const Id& b, const TransportMode& m,
                          const Id& weather) {
    const WildLink* best = nullptr;
    int best_min = 0;
    for (const WildLink& l : w.wild.links) {
        if (!((l.a == a && l.b == b) || (l.a == b && l.b == a)) || !mode_allows(m, l.kind)) continue;
        const int mins = leg_minutes(w.wild, l, m, weather);
        if (best == nullptr || mins < best_min) {
            best = &l;
            best_min = mins;
        }
    }
    return best;
}

bool hour_matches(const std::string& hours, int hour) {
    if (hours == "day") return hour >= 6 && hour <= 18;
    if (hours == "night") return hour >= 19 || hour <= 5;
    if (hours == "dusk_night") return hour >= 17 || hour <= 6;
    return true;
}

bool terrain_matches(const EncounterDef& e, const WildLink& l, const WildNode* a, const WildNode* b) {
    if (e.terrains.empty()) return true;
    for (const std::string& t : e.terrains)
        if (t == l.kind || (a && a->kind == t) || (b && b->kind == t)) return true;
    return false;
}

bool group_friendly(const WorldState& w, const Group& g, const GroupDef& def) {
    if (g.player_member || g.player_led || g.allied || g.paid_until >= w.day) return true;
    if (def.kind == "partisans")  // they stop Empire men, not every traveller
        return standing(w.faction, "the_empire") < 50 && g.grudge_vs_player < 30;
    return false;
}

struct Pick {
    const EncounterDef* def = nullptr;
    Id group;
};

Pick pick_encounter(const WorldState& w, const WildLink& l, int hour, Rng& r) {
    const WildNode* a = find_node(w.wild, l.a);
    const WildNode* b = find_node(w.wild, l.b);
    const std::string& season = w.cal.season_id(w.day);
    const int drought = std::clamp(w.facts.drought_stage, 0, 20);
    const int war = std::clamp(w.facts.war_stage, 0, 20);
    std::vector<std::pair<Pick, int>> pool;
    int total = 0;
    for (const EncounterDef& e : w.wild.encounter_defs) {
        if (e.danger_min > l.danger || !hour_matches(e.hours, hour) || !terrain_matches(e, l, a, b)) continue;
        if (!e.seasons.empty() &&
            std::find(e.seasons.begin(), e.seasons.end(), season) == e.seasons.end())
            continue;
        int weight = e.weight * std::max(0, 100 + drought * e.drought_pct + war * e.war_pct) / 100;
        Pick p;
        p.def = &e;
        if (e.kind == "bandits" || e.kind == "raiders") {
            int best_strength = 0;
            for (const GroupDef& gd : w.wild.group_defs) {
                const Group* g = find_group(w.wild, gd.id);
                const bool kind_ok = e.kind == "bandits" ? gd.kind == "bandits" : gd.kind != "bandits";
                if (!g || !g->active || !kind_ok) continue;
                if (!in_territory(gd, l.a) && !in_territory(gd, l.b)) continue;
                if (g->strength > best_strength) {
                    best_strength = g->strength;
                    p.group = gd.id;
                }
            }
            weight = p.group.empty() ? weight / 2 : weight + best_strength * 5;
        }
        if (weight <= 0) continue;
        pool.emplace_back(p, weight);
        total += weight;
    }
    if (total <= 0) return {};
    int roll = r.int_in(0, total - 1);
    for (const auto& [p, wt] : pool) {
        if (roll < wt) return p;
        roll -= wt;
    }
    return {};
}

// Resolves one encounter; returns false when the player is beaten (travel halts).
bool resolve_encounter(WorldState& w, const Pick& pick, const WildLink& l, int hour,
                       const std::string& stance, int companions, const TransportMode& mode, Rng& r,
                       EncounterRecord& rec) {
    const EncounterDef& e = *pick.def;
    rec.day = w.day;
    rec.hour = hour;
    rec.link = l.id;
    rec.encounter = e.id;
    rec.kind = e.kind;
    rec.group = pick.group;
    rec.foes = r.int_in(e.foes_min, e.foes_max);
    Group* g = pick.group.empty() ? nullptr : &w.wild.groups[pick.group];
    const GroupDef* gd = pick.group.empty() ? nullptr : find_group_def(w.wild, pick.group);
    if (g) rec.foes = std::clamp(rec.foes, 1, std::max(1, g->strength));
    int prowess = gd ? gd->prowess_pct : e.prowess_pct;

    if (g && gd && group_friendly(w, *g, *gd)) {
        rec.stance = "pass";
        rec.outcome = "passed";
        return true;
    }

    auto do_fight = [&](bool player_attacks) -> bool {
        SkirmishRequest req;
        SkirmishSide them{g ? g->id : e.id, rec.foes, prowess, g ? g->morale : 60, false};
        SkirmishSide us = player_side(w, companions);
        req.attacker = player_attacks ? us : them;
        req.defender = player_attacks ? them : us;
        req.place = l.id;
        req.hour = hour;
        req.context = "encounter";
        const SkirmishOutcome out = fight(w, req, r);
        const bool won = player_attacks ? out.attacker_won : !out.attacker_won;
        rec.foes_lost = player_attacks ? out.defender_losses : out.attacker_losses;
        rec.allies_lost = player_attacks ? out.attacker_losses : out.defender_losses;
        if (won && out.player_wounded) ++w.wild.player_wounds;
        if (g && gd) {
            g->strength = std::max(0, g->strength - rec.foes_lost);
            g->grudge_vs_player = std::min(100, g->grudge_vs_player + 10);
            if (g->strength <= 0) {
                log_event(w, "wild_camp_broken", gd->name + " was broken on the road by the player");
                credit_purse(w.property, kPlayer, static_cast<Silver>(kBountyPerFighter) * gd->base_strength);
                clear_camp(w, *g, true);
            }
        }
        if (won && (e.kind == "bandits" || e.kind == "raiders"))
            w.wild.player_fear = std::min(100, w.wild.player_fear + 2 * rec.foes_lost);
        if (!won) player_defeated(w, rec.group.empty() ? e.id : rec.group);
        return won;
    };

    if (e.hostile) {
        rec.stance = stance;
        if (stance == "pay" && e.kind != "animals") {
            const Silver toll = 3 * rec.foes;
            if (purse_of(w) >= toll) {
                take_from_purse(w.property, kPlayer, toll);
                if (g) g->supplies = std::min(100, g->supplies + static_cast<int>(toll));
                rec.outcome = "paid";
                return true;
            }
        }
        if (stance == "flee" || (stance == "pay" && e.kind == "animals")) {
            const int flee_bp =
                std::clamp(4000 + (mode.speed_m_per_h - 4000) / 2 - rec.foes * 200 -
                               (prowess > 120 ? 1500 : 0),
                           1000, 8000);
            if (roll_bp(r) < flee_bp) {
                rec.outcome = "fled";
                return true;
            }
        }
        rec.stance = "fight";
        const bool won = do_fight(false);
        rec.outcome = won ? "won" : "lost";
        return won;
    }

    // Refugees and merchants.
    if (stance == "rob") {
        rec.stance = "rob";
        const bool won = do_fight(true);
        rec.outcome = won ? "robbed_them" : "lost";
        if (won) {
            if (e.kind == "merchants") {
                credit_purse(w.property, kPlayer, static_cast<Silver>(8) * rec.foes);
                give_player(w, "salt", 2);
            } else {
                give_player(w, "grain", std::max(1, rec.foes / 3));
                add_standing(w.faction, city_faction_id(w), -3);
            }
            ++w.wild.infamy;
            const std::vector<std::string> gate = {"gatekeeper"};
            const Id witness_npc = pick_npc(w, gate, r);
            (void)commit_crime(w, kPlayer, "theft", kCity,
                               witness_npc.empty() ? std::vector<Id>{} : std::vector<Id>{witness_npc});
            spread_rumour(w, "wild:robbery", "travellers were robbed on " + l.id + " by a lone brigand");
        }
        return won;
    }
    if (stance == "give" && e.kind == "refugees") {
        Inventory& inv = w.inventories[kPlayer];
        for (const char* food : {"bread", "grain", "dates"}) {
            if (!take_items(inv, food, 1).empty()) {
                add_standing(w.faction, city_faction_id(w), 1);
                rec.stance = "give";
                rec.outcome = "gave";
                return true;
            }
        }
    }
    rec.stance = "pass";
    rec.outcome = "met";
    // Travellers talk: merchants warn of the nearest camp, refugees of the east.
    if (e.kind == "merchants") {
        for (auto& [place, camp] : w.wild.camps)
            if (camp.status == "occupied" && !camp.known_to_player) {
                camp.known_to_player = true;
                break;
            }
    }
    return true;
}

}  // namespace

std::vector<Id> find_route(const WorldState& w, const Id& from, const Id& to, const Id& mode_id) {
    const auto mit = w.wild.modes.find(mode_id);
    if (mit == w.wild.modes.end() || !find_node(w.wild, from) || !find_node(w.wild, to)) return {};
    const TransportMode& m = mit->second;
    const Id weather = weather_on(w, w.day);
    std::map<Id, int> dist;
    std::map<Id, Id> prev;
    std::set<std::pair<int, Id>> open;
    dist[from] = 0;
    open.insert({0, from});
    while (!open.empty()) {
        const auto [d, node] = *open.begin();
        open.erase(open.begin());
        if (node == to) break;
        if (d > dist[node]) continue;
        for (const WildLink* l : links_of(w.wild, node)) {
            if (!mode_allows(m, l->kind)) continue;
            const Id next = other_end(*l, node);
            const int nd = d + leg_minutes(w.wild, *l, m, weather);
            const auto it = dist.find(next);
            if (it == dist.end() || nd < it->second) {
                if (it != dist.end()) open.erase({it->second, next});
                dist[next] = nd;
                prev[next] = node;
                open.insert({nd, next});
            }
        }
    }
    if (!dist.count(to)) return {};
    std::vector<Id> path{to};
    while (path.back() != from) path.push_back(prev[path.back()]);
    std::reverse(path.begin(), path.end());
    return path;
}

TravelResult travel(WorldState& w, const Id& to, const Id& mode_id, int start_hour,
                    const std::string& stance, int companions) {
    TravelResult res;
    res.from = w.wild.player_place;
    res.to = to;
    res.mode = mode_id;
    res.stopped_at = res.from;
    res.path.push_back(res.from);
    auto refuse = [&](int code, const char* why) {
        res.code = code;
        res.refusal = why;
        return res;
    };
    if (to.empty() || mode_id.empty()) return refuse(-1, "bad_argument");
    if (!find_node(w.wild, to)) return refuse(-2, "unknown_place");
    const auto mit = w.wild.modes.find(mode_id);
    if (mit == w.wild.modes.end()) return refuse(-3, "unknown_mode");
    const TransportMode& mode = mit->second;
    if (!mode.requires_item.empty()) {
        const auto inv = w.inventories.find(kPlayer);
        const bool has = inv != w.inventories.end() && count_of(inv->second, mode.requires_item) > 0;
        if (!has) return refuse(-4, "needs_item");
    }
    if (to == w.wild.player_place) return refuse(-6, "already_there");
    if (!w.wild.escorting.empty()) return refuse(-7, "escorting");
    const std::vector<Id> path = find_route(w, w.wild.player_place, to, mode_id);
    if (path.empty()) return refuse(-5, "no_route");

    const Id weather = weather_on(w, w.day);
    int thirst_extra = 0;
    for (const WeatherDef& d : w.wild.weather_defs)
        if (d.id == weather) thirst_extra = d.thirst_per_hour;
    int enc_weather = 0;
    for (const WeatherDef& d : w.wild.weather_defs)
        if (d.id == weather) enc_weather = d.encounter_bp;
    Rng r = action_stream(w, "travel:" + to);
    const int hour0 = std::clamp(start_hour, 0, 23);
    const int drought = std::clamp(w.facts.drought_stage, 0, 20);
    const int war = std::clamp(w.facts.war_stage, 0, 20);

    for (std::size_t i = 1; i < path.size(); ++i) {
        const WildLink* l = best_link(w, path[i - 1], path[i], mode, weather);
        if (l == nullptr) break;
        const int leg = leg_minutes(w.wild, *l, mode, weather);
        const int before = res.minutes;
        const int after = before + leg;
        const bool desert = l->kind == "desert_track";
        const bool water_here = l->kind == "river" || l->kind == "marsh_path";

        // Needs, hour by hour (Needs.hpp; the drought bites through the heat).
        for (int h = before / 60; h < after / 60; ++h) {
            advance_needs(w.needs, kPlayer, 1, false);
            Needs& n = needs_of(w.needs, kPlayer);
            n.thirst = std::min(100, n.thirst + thirst_extra + (desert ? 1 : 0) + drought / 2);
            Inventory& inv = w.inventories[kPlayer];
            if (n.thirst >= 60) {
                if (water_here) {
                    drink(w.db, w.needs, kPlayer, "water");
                } else if (count_of(inv, "water") > 0) {
                    drink(w.db, w.needs, kPlayer, "water");
                    take_items(inv, "water", 1);
                }
            }
            if (n.hunger >= 60)
                for (const auto& [item, count] : counts(inv))
                    if (count > 0 && item != "water" && eat(w.db, w.needs, kPlayer, item)) {
                        take_items(inv, item, 1);
                        break;
                    }
            if (needs_of(w.needs, kPlayer).thirst >= 100) {
                ++w.wild.player_wounds;
                res.code = 1;
                res.halted_by = "collapse";
                res.minutes = (h + 1) * 60;  // collapsed at the end of that hour
                w.wild.player_place = res.stopped_at;
                return res;
            }
        }

        // Encounters: one roll per started hour of the leg.
        const int rolls = std::min(24, std::max(1, (leg + 59) / 60));
        const int max_encounters = 1 + leg / 600;
        int met = 0;
        for (int k = 0; k < rolls && met < max_encounters; ++k) {
            const int hour = (hour0 + (before + k * 60) / 60) % 24;
            const bool night = hour >= 19 || hour <= 5;
            const int chance = std::clamp(300 + l->danger * 150 + (night ? 300 : 0) + drought * 50 +
                                              war * 50 + enc_weather,
                                          0, 6000);
            if (roll_bp(r) >= chance) continue;
            const Pick pick = pick_encounter(w, *l, hour, r);
            if (pick.def == nullptr) continue;
            ++met;
            EncounterRecord rec;
            const bool ok = resolve_encounter(w, pick, *l, hour, stance, companions, mode, r, rec);
            push_encounter(w, rec);
            res.encounters.push_back(w.wild.encounters.back());
            if (!ok) {
                res.code = 1;
                res.halted_by = "defeat";
                res.minutes = before + (k + 1) * 60;
                w.wild.player_place = res.stopped_at;
                return res;
            }
        }
        res.minutes = after;
        res.stopped_at = path[i];
        res.path.push_back(path[i]);
        w.wild.player_place = path[i];
    }
    res.code = res.stopped_at == to ? 0 : 1;
    return res;
}

}  // namespace sim
