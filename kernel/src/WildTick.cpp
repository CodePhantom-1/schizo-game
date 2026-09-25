// WildTick.cpp — W4-C: the wild's daily tick — weather, caravans on their
// routes, the herds, the raid formula (mechanics.md row 11: hunger +
// opportunity - defence - fear -> raid chance) and the life of every camp:
// forming, eating, recruiting, deserting, feuding, moving, raiding and
// re-forming while the conditions that made it persist.
//
// D-022: integer maths only; chances in basis points.
#include "WildInternal.hpp"

#include "sim/Actions.hpp"
#include "sim/Rites.hpp"

#include <algorithm>

namespace sim {

using namespace wild;

namespace {

const WeatherDef* weather_def(const WorldState& w, const Id& id) {
    for (const WeatherDef& d : w.wild.weather_defs)
        if (d.id == id) return &d;
    return nullptr;
}

int watch_baseline(const WorldState& w) {
    int n = 0;
    for (const Npc& npc : w.population.npcs)
        if (npc.role == "watchman" || npc.role == "gatekeeper" || npc.role == "paladin") ++n;
    return n;
}

bool grudges_against(const GroupDef& def, const Id& faction_or_group) {
    return !faction_or_group.empty() &&
           std::find(def.grudges.begin(), def.grudges.end(), faction_or_group) != def.grudges.end();
}

bool territories_overlap(const GroupDef& a, const GroupDef& b) {
    for (const Id& p : a.territory)
        if (in_territory(b, p)) return true;
    return false;
}

std::string place_name(const WorldState& w, const Id& id) {
    if (const WildNode* n = find_node(w.wild, id)) return n->name;
    if (auto row = w.db.find("places", id)) return row->get("name", id);
    return id;
}

void form_group(WorldState& w, Group& g, const GroupDef& def, const Id& site) {
    g.active = true;
    g.camp = site;
    g.leader = def.leaders[static_cast<std::size_t>(g.times_formed) % def.leaders.size()];
    g.strength = std::min(def.max_strength, def.base_strength + w.facts.drought_stage / 2);
    g.peak_strength = g.strength;
    g.supplies = def.start_supplies;
    g.morale = def.start_morale;
    g.formed_day = w.day;
    g.pressure = 0;
    g.rumoured = false;
    g.grudge_vs_player = g.times_cleared > 0 ? 30 : 0;  // the new chief remembers who broke the last
    ++g.times_formed;
    Camp& camp = camp_ref(w, site);
    camp.group = g.id;
    camp.status = "occupied";
    camp.since = w.day;
    camp.lookout = 30 + g.morale / 3;
    camp.alarm_days = 0;
    camp.scouted = false;
    camp.known_to_player = false;
    log_event(w, g.times_formed > 1 ? "wild_camp_reoccupied" : "wild_camp_formed",
              def.name + " under " + g.leader + " made camp at " + place_name(w, site));
}

}  // namespace

// --- weather ---------------------------------------------------------------------

Id weather_on(const WorldState& w, DayNumber day) {
    const std::string& season = w.cal.season_id(day);
    const int drought = std::clamp(w.facts.drought_stage, 0, 20);
    int total = 0;
    std::vector<int> weights;
    for (const WeatherDef& d : w.wild.weather_defs) {
        const auto it = d.season_weights.find(season);
        int wt = (it == d.season_weights.end() ? 0 : it->second) + d.drought_weight * drought;
        wt = std::max(0, wt);
        weights.push_back(wt);
        total += wt;
    }
    if (total <= 0) return "clear";
    Rng r = stream(w, day, "weather");
    int pick = r.int_in(0, total - 1);
    for (std::size_t i = 0; i < weights.size(); ++i) {
        if (pick < weights[i]) return w.wild.weather_defs[i].id;
        pick -= weights[i];
    }
    return "clear";
}

// --- the raid formula ---------------------------------------------------------------

namespace wild {

RaidAssessment assess_target(const WorldState& w, const Group& g, const GroupDef& def,
                             const std::string& target, DayNumber day) {
    RaidAssessment a;
    a.group = g.id;
    a.target = target;
    a.place = target_place(w, def, target);
    const int drought = std::clamp(w.facts.drought_stage, 0, 20);
    const int war = std::clamp(w.facts.war_stage, 0, 20);
    a.hunger = drought * 12 + war * 6 + (100 - std::clamp(g.supplies, 0, 100)) / 2;
    a.fear = std::clamp(w.wild.player_fear, 0, 100) / 2 + (100 - std::clamp(g.morale, 0, 100)) / 5;
    const int alive = watch_alive(w);
    int opp = std::max(0, watch_baseline(w) - alive) * 6;  // a weak watch invites them
    opp += std::clamp(g.strength, 0, 30);                  // more men, more within reach
    int def_pts = alive * 5 + standing(w.faction, city_faction_id(w)) / 4;
    if (const WeatherDef* wd = weather_def(w, weather_on(w, day))) opp += wd->raid_opportunity;
    const auto pref = def.prefers.find(target);
    opp += (pref == def.prefers.end() ? 0 : pref->second) / 10;
    const std::string& season = w.cal.season_id(day);
    if (target == "fields") {
        opp += season == "harvest" ? 25 : season == "sowing" ? 10 : 5;
    } else if (target == "herds") {
        opp += 10 + std::min(20, w.wild.herd_head / 4);
    } else if (target == "market") {
        opp += 15;
        def_pts += 20;  // the walls
    } else if (target == "caravans") {
        if (const CaravanRun* run = caravan_in_territory(w, def)) {
            const CaravanDef* cd = caravan_def(w, run->def);
            opp += 30 + (cd && grudges_against(def, cd->faction) ? 20 : 0);
            def_pts += run->guards * 4 + run->escort_fighters * 5;
        }
    }
    if (!a.place.empty() && ward_holds(w.rite_effects, a.place, day)) def_pts += 20;  // K-1 wards
    a.opportunity = opp;
    a.defence = def_pts;
    a.score = a.hunger + a.opportunity - a.defence - a.fear;
    a.chance_bp = std::clamp(a.score * kRaidBpPerPoint, 0, kRaidMaxBp);
    if (a.place.empty()) a.chance_bp = 0;
    return a;
}

}  // namespace wild

RaidAssessment assess_raid(const WorldState& w, const Id& group_id, DayNumber day) {
    RaidAssessment best;
    best.group = group_id;
    const Group* g = find_group(w.wild, group_id);
    const GroupDef* def = find_group_def(w.wild, group_id);
    if (g == nullptr || def == nullptr || !g->active || g->player_led || g->strength < 2) return best;
    const WeatherDef* wd = weather_def(w, weather_on(w, day));
    const bool bribed = g->paid_until >= day;
    bool first = true;
    for (const char* target : {"fields", "herds", "caravans", "market"}) {
        const auto pref = def->prefers.find(target);
        if (pref == def->prefers.end() || pref->second <= 0) continue;
        if (bribed && std::string(target) != "caravans") continue;  // a month's peace was bought
        if (w.wild.herd_head <= 0 && std::string(target) == "herds") continue;
        RaidAssessment a = assess_target(w, *g, *def, target, day);
        if (a.place.empty()) continue;
        if (std::string(target) == "caravans") {
            const CaravanRun* run = caravan_in_territory(w, *def);
            const CaravanDef* cd = run ? caravan_def(w, run->def) : nullptr;
            // Partisans fall only on the caravans of those they fight; nobody
            // bribed falls on a caravan the player escorts.
            if (def->kind == "partisans" && !(cd && grudges_against(*def, cd->faction))) continue;
            if (bribed && run && run->escorted) continue;
        }
        if (first || a.score > best.score) {
            best = a;
            first = false;
        }
    }
    if (wd && !wd->raids) best.chance_bp = 0;  // nobody rides in a sandstorm
    return best;
}

// --- raid resolution ------------------------------------------------------------------

namespace wild {

RaidRecord resolve_raid(WorldState& w, Group& g, const GroupDef& def, const RaidAssessment& a,
                        Rng& r, bool player_led, int hour) {
    RaidRecord rec;
    rec.day = w.day;
    rec.group = g.id;
    rec.target = a.target;
    rec.place = a.place;
    rec.chance_bp = a.chance_bp;
    Camp& camp = camp_ref(w, g.camp);
    std::vector<std::string> victims_roles;

    if (a.target == "caravans") {
        CaravanRun* run = caravan_in_territory(w, def);
        if (run == nullptr) return rec;
        SkirmishRequest req;
        req.attacker = SkirmishSide{g.id, g.strength + (player_led ? 1 : 0), def.prowess_pct, g.morale,
                                    player_led};
        req.defender = SkirmishSide{run->id, run->guards + run->escort_fighters, 100, 55, run->escorted};
        req.place = a.place;
        req.hour = hour;
        req.context = "raid";
        const SkirmishOutcome out = fight(w, req, r);
        g.strength = std::max(0, g.strength - out.attacker_losses);
        int guard_losses = out.defender_losses;
        if (run->escorted) {
            const int escort_lost = std::min(guard_losses, std::max(0, run->escort_fighters - 1));
            run->escort_fighters -= escort_lost;
            guard_losses -= escort_lost;
            // A lost defence is player_defeated below; a won one may still cost a wound.
            if (!out.attacker_won && out.player_wounded) ++w.wild.player_wounds;
        }
        run->guards = std::max(0, run->guards - guard_losses);
        rec.success = out.attacker_won;
        if (rec.success) {
            for (const auto& [item, units] : run->goods) {
                rec.goods[item] += units;
                rec.goods_taken += units;
            }
            if (run->escorted) {
                player_defeated(w, g.id);
                w.wild.escorting.clear();
            }
            const Id run_id = run->id;
            w.wild.caravans.erase(std::remove_if(w.wild.caravans.begin(), w.wild.caravans.end(),
                                                 [&](const CaravanRun& c) { return c.id == run_id; }),
                                  w.wild.caravans.end());
            ++w.wild.caravans_robbed;
        }
    } else {
        const int success_bp = std::clamp(4000 + g.strength * 400 - a.defence * 50, 1000, 9000);
        rec.success = roll_bp(r) < success_bp;
        if (rec.success) {
            auto take = [&](const Id& item, std::int64_t want) {
                const auto key = std::string(kCity) + "/" + item;
                const auto it = w.economy.stock_by_city_item.find(key);
                const std::int64_t have = it == w.economy.stock_by_city_item.end() ? 0 : it->second;
                const int units = static_cast<int>(std::clamp<std::int64_t>(want, 0, have));
                if (units <= 0) return;
                consume(w.economy, kCity, item, units);  // Economy's theft/raid hook
                rec.goods[item] += units;
                rec.goods_taken += units;
            };
            if (a.target == "fields") {
                take("grain", 2 * g.strength);
                victims_roles = {"field hand"};
            } else if (a.target == "herds") {
                const int head = std::min(w.wild.herd_head, 1 + g.strength / 4);
                w.wild.herd_head -= head;
                rec.goods["sheep"] += head;
                rec.goods_taken += head;
                take("wool", g.strength);
                victims_roles = {"herdsman"};
            } else if (a.target == "market") {
                take("grain", 3 * g.strength);
                Id richest;
                std::int64_t most = 0;
                for (const auto& [key, units] : w.economy.stock_by_city_item) {
                    const std::string prefix = std::string(kCity) + "/";
                    if (key.rfind(prefix, 0) != 0) continue;
                    const Id item = key.substr(prefix.size());
                    if (item != "grain" && units > most) {
                        most = units;
                        richest = item;
                    }
                }
                if (!richest.empty()) take(richest, g.strength);
                victims_roles = {"market trader", "watchman"};
            }
        }
    }

    if (rec.success) {
        for (const auto& [item, units] : rec.goods)
            if (item != "sheep") camp.loot[item] += units;
        g.supplies = std::min(100, g.supplies + std::min(50, rec.goods_taken * 3));
        g.morale = std::min(100, g.morale + 10);
        if (!player_led) g.strength = std::min(def.max_strength, g.strength + 1);  // success draws recruits
        if (!victims_roles.empty() && roll_bp(r) < 3000) {
            const Id victim = pick_npc(w, victims_roles, r);
            if (!victim.empty()) {
                if (r.int_in(0, 1) == 0 && camp.captives.size() < 5) {
                    take_captive(w, victim, g, camp);
                    rec.taken.push_back(victim);
                } else {
                    kill_npc(w, victim);
                    rec.slain.push_back(victim);
                }
            }
        }
    } else {
        g.strength = std::max(0, g.strength - 1 - r.int_in(0, 1));
        g.morale = std::max(0, g.morale - 15);
        camp.alarm_days = std::max(camp.alarm_days, 3);
    }
    g.peak_strength = std::max(g.peak_strength, g.strength);
    ++g.raids;

    std::string who = (player_led ? std::string("The player's band") : def.name);
    std::string text = who + (rec.success ? " raided the " : " was driven off from the ") + a.target +
                       " at " + place_name(w, a.place);
    if (rec.success) text += ": " + std::to_string(rec.goods_taken) + " goods carried off";
    for (const Id& v : rec.slain) text += "; " + v + " slain";
    for (const Id& v : rec.taken) text += "; " + v + " taken captive";
    rec.summary = text;
    log_event(w, "wild_raid_" + a.target, text);
    // Only blood travels as rumour (every rumour reaches the whole city along
    // the knows graph, so ordinary thefts stay in the event log).
    if (!rec.slain.empty() || !rec.taken.empty()) spread_rumour(w, "group:" + g.id, text);
    push_raid(w, rec);
    return rec;
}

}  // namespace wild

// --- the daily tick ------------------------------------------------------------------

namespace {

void tick_caravans(WorldState& w, const Id& weather) {
    const DayNumber day = w.day;
    const bool storm = weather == "sandstorm";
    std::vector<CaravanRun> still;
    for (CaravanRun& run : w.wild.caravans) {
        const CaravanDef* cd = caravan_def(w, run.def);
        if (cd == nullptr) continue;
        if (!storm) run.leg += kCaravanLegsPerDay;
        const int last = static_cast<int>(cd->route.size()) - 1;
        if (run.leg >= last) {
            for (const auto& [item, units] : run.goods) deliver(w.economy, kCity, item, units);
            ++w.wild.caravans_arrived;
            if (run.escorted && w.wild.escorting == run.id) {
                credit_purse(w.property, kPlayer, kEscortFee);
                add_standing(w.faction, city_faction_id(w), 2);
                w.wild.player_place = cd->route.back();
                w.wild.escorting.clear();
            }
            log_event(w, "wild_caravan_arrived", cd->name + " came in through the gate");
            continue;
        }
        if (run.escorted && w.wild.escorting == run.id)
            w.wild.player_place = cd->route[static_cast<std::size_t>(run.leg)];
        still.push_back(run);
    }
    w.wild.caravans = std::move(still);
    for (const CaravanDef& cd : w.wild.caravan_defs) {
        if (day < 1 + cd.offset_days || (day - 1 - cd.offset_days) % cd.every_days != 0) continue;
        CaravanRun run;
        run.id = cd.id + "#" + std::to_string(day);
        run.def = cd.id;
        run.leg = 0;
        run.guards = cd.guards;
        for (const auto& [item, units] : cd.goods) run.goods[item] += units;
        run.departed = day;
        w.wild.caravans.push_back(run);
    }
}

void tick_group(WorldState& w, Group& g, const GroupDef& def) {
    const DayNumber day = w.day;
    const int drought = std::clamp(w.facts.drought_stage, 0, 20);
    const int war = std::clamp(w.facts.war_stage, 0, 20);
    Rng r = stream(w, day, "group:" + g.id);

    if (!g.active) {
        if (g.player_led) return;  // a broken player band stays broken until refounded
        g.pressure = conditions_hold(def.forms_when, w.facts) ? g.pressure + 1 : 0;
        const bool due = g.times_formed == 0
                             ? g.pressure > def.form_days
                             : g.pressure > def.form_days && day - g.cleared_day >= def.repop_days;
        if (!due) return;
        const Id site = free_camp_site(w, def, "");
        if (!site.empty()) form_group(w, g, def, site);
        return;
    }

    Camp& camp = camp_ref(w, g.camp);
    if (!g.rumoured) {
        spread_rumour(w, "camp:" + g.camp,
                      "men of " + def.name + " are camping at " + place_name(w, g.camp));
        g.rumoured = true;
    }

    // Upkeep: the drought and the war empty the larder; in good years the
    // country feeds them.
    const int drain = (drought + war + 2) / 3;  // 0 in good years, 1 at drought 1..3, 2 at 4..6
    g.supplies = std::max(0, g.supplies - drain);
    if (g.supplies == 0) {
        g.morale = std::max(0, g.morale - 3);
        if ((day - g.formed_day) % 5 == 0) g.strength = std::max(0, g.strength - 1);  // desertion
    } else if (g.supplies > 30 && g.morale < def.start_morale) {
        ++g.morale;
    }
    // Recruitment: hungry men join whoever eats.
    if (!g.player_led && (drought >= 1 || war >= 1) && g.supplies > 10 && day > g.formed_day &&
        (day - g.formed_day) % 15 == 0)
        g.strength = std::min(def.max_strength, g.strength + 1 + drought / 2);
    g.peak_strength = std::max(g.peak_strength, g.strength);

    camp.lookout = std::clamp(30 + g.morale / 3 + (camp.alarm_days > 0 ? 30 : 0), 0, 100);
    if (camp.alarm_days > 0) --camp.alarm_days;
    if (day % 10 == 0 && g.grudge_vs_player > 0) --g.grudge_vs_player;

    if (g.strength <= 0 || g.morale <= 0) {
        log_event(w, "wild_camp_abandoned", def.name + " scattered from " + place_name(w, g.camp));
        clear_camp(w, g, false);
        return;
    }

    // Threatened camps move: the player found them and hates them, or they
    // have been bled below their muster.
    if (!g.player_led && day >= camp.since + 10) {
        const bool hunted = camp.scouted && g.grudge_vs_player >= 40;
        const bool bled = g.strength * 3 < g.peak_strength && g.strength < def.base_strength;
        const bool feared = w.wild.player_fear >= 60 && g.strength < def.base_strength;
        if (hunted || bled || feared) move_camp(w, g, hunted ? "hunted" : bled ? "bled" : "afraid");
    }

    // Feuds between groups that hate each other and share ground.
    for (const GroupDef& other : w.wild.group_defs) {
        if (other.id <= def.id) continue;  // each pair once a day
        Group& h = w.wild.groups[other.id];
        if (!h.active || !territories_overlap(def, other)) continue;
        const bool hate = grudges_against(def, other.id) || grudges_against(def, other.faction) ||
                          grudges_against(other, def.id) || grudges_against(other, def.faction);
        if (!hate || roll_bp(r) >= 300) continue;
        SkirmishRequest req;
        req.attacker = SkirmishSide{g.id, g.strength, def.prowess_pct, g.morale, g.player_led};
        req.defender = SkirmishSide{h.id, h.strength, other.prowess_pct, h.morale, h.player_led};
        req.place = h.camp;
        req.context = "feud";
        const SkirmishOutcome out = fight(w, req, r);
        g.strength = std::max(0, g.strength - out.attacker_losses);
        h.strength = std::max(0, h.strength - out.defender_losses);
        (out.attacker_won ? h : g).morale = std::max(0, (out.attacker_won ? h : g).morale - 10);
        log_event(w, "wild_feud", def.name + " and " + other.name + " fought over " +
                                      place_name(w, h.camp));
        if (h.strength <= 0) clear_camp(w, h, false);
        if (g.strength <= 0) {
            clear_camp(w, g, false);
            return;
        }
    }

    if (g.player_led) return;  // the player decides when his band rides
    const RaidAssessment a = assess_raid(w, g.id, day);
    if (a.chance_bp > 0 && roll_bp(r) < a.chance_bp) {
        resolve_raid(w, g, def, a, r, false, r.int_in(0, 23));
        if (g.strength <= 0) {
            log_event(w, "wild_camp_abandoned", def.name + " was broken raiding");
            clear_camp(w, g, false);
        }
    }
}

}  // namespace

void tick_wild(WorldState& w) {
    const DayNumber day = w.day;
    const Id weather = weather_on(w, day);

    tick_caravans(w, weather);

    // The city's breeders and buyers restock the flocks unless the drought is ruinous.
    if (day % 5 == 0 && w.facts.drought_stage < 4 && w.wild.herd_head < kHerdMax) ++w.wild.herd_head;

    for (auto& [place, site] : w.wild.sites) {
        const WildNode* n = find_node(w.wild, place);
        if (n == nullptr || n->restock_days <= 0 || site.emptied_day <= 0) continue;
        if (day - site.emptied_day < n->restock_days) continue;
        for (const auto& [item, units] : n->loot) site.loot_left[item] = units;  // the sand gives up more
        site.emptied_day = 0;
    }

    for (const GroupDef& def : w.wild.group_defs) tick_group(w, w.wild.groups[def.id], def);
    // The player's own band (not a canon group) eats and can starve too.
    if (auto it = w.wild.groups.find(kPlayerBand); it != w.wild.groups.end() && it->second.active) {
        Group& g = it->second;
        g.supplies = std::max(0, g.supplies - 1 - w.facts.drought_stage / 2);
        if (g.supplies == 0 && day % 5 == 0) g.strength = std::max(0, g.strength - 1);
        if (g.strength <= 0) clear_camp(w, g, false);
    }

    // Captives nobody came for are sold east (the rescue quest's deadline).
    std::vector<Id> sold;
    for (const auto& [npc, since] : w.wild.captive_since)
        if (day - since >= kCaptiveDays) sold.push_back(npc);
    for (const Id& npc : sold) {
        for (auto& [place, camp] : w.wild.camps)
            camp.captives.erase(std::remove(camp.captives.begin(), camp.captives.end(), npc),
                                camp.captives.end());
        w.wild.captive_since.erase(npc);
        w.wild.npc_fate[npc] = "sold_east";
        log_event(w, "wild_captive_sold", npc + " was sold to the east");
    }

    if (day % 2 == 0 && w.wild.player_fear > 0) --w.wild.player_fear;
}

}  // namespace sim
