// WildPlayer.cpp — W4-C: what the player can do in the wild (D-021 open
// play: nothing here gates on story or quest progress). Scout, assault,
// infiltrate, bribe, ally with, join or take over a camp; found a band of
// his own and raid; ransom captives; escort or rob caravans; search ruins,
// caves and tombs (tomb robbing is a crime — laws.csv tomb_robbery).
//
// D-022: integer maths only.
#include "WildInternal.hpp"

#include "sim/Actions.hpp"

#include <algorithm>

namespace sim {

using namespace wild;

namespace {

WildActionResult res(int code, std::string text) { return WildActionResult{code, std::move(text)}; }

bool adjacent(const WorldState& w, const Id& a, const Id& b) {
    if (a == b) return true;
    for (const WildLink* l : links_of(w.wild, a))
        if (other_end(*l, a) == b) return true;
    return false;
}

bool night(int hour) { return hour >= 19 || hour <= 5; }

std::string place_name(const WorldState& w, const Id& id) {
    const WildNode* n = find_node(w.wild, id);
    return n ? n->name : id;
}

// The player's own band has no canon row: it may raid anywhere it can reach.
GroupDef band_def(const WorldState& w, const Group& g) {
    GroupDef d;
    d.id = g.id;
    d.name = "The player's band";
    d.kind = "bandits";
    d.camp_sites = {g.camp};
    for (const auto& [id, n] : w.wild.nodes) d.territory.push_back(id);
    d.max_strength = 30;
    d.base_strength = 1;
    d.prowess_pct = 100;
    d.prefers = {{"fields", 1}, {"herds", 1}, {"caravans", 1}, {"market", 1}};
    d.leaders = {"player"};
    return d;
}

const GroupDef* def_or_band(const WorldState& w, const Group& g, GroupDef& scratch) {
    if (const GroupDef* d = find_group_def(w.wild, g.id)) return d;
    scratch = band_def(w, g);
    return &scratch;
}

Group* active_group(WorldState& w, const Id& id) {
    auto it = w.wild.groups.find(id);
    return it == w.wild.groups.end() || !it->second.active ? nullptr : &it->second;
}

Group* camp_group(WorldState& w, const Id& place) {
    const Camp* c = find_camp(w.wild, place);
    if (c == nullptr || c->status != "occupied") return nullptr;
    return active_group(w, c->group);
}

void file_crime(WorldState& w, const Id& law, const std::vector<std::string>& roles, Rng& r,
                int witness_bp) {
    std::vector<Id> witnesses;
    if (roll_bp(r) < witness_bp) {
        const Id npc = pick_npc(w, roles, r);
        if (!npc.empty()) witnesses.push_back(npc);
    }
    (void)commit_crime(w, kPlayer, law, kCity, witnesses);
}

}  // namespace

WildActionResult scout_camp(WorldState& w, const Id& place, int hour) {
    Group* g = camp_group(w, place);
    if (g == nullptr) return res(-2, "no camp there");
    if (!adjacent(w, w.wild.player_place, place)) return res(-3, "too far to see the camp");
    Camp& camp = camp_ref(w, place);
    Rng r = action_stream(w, "scout:" + place);
    const int spotted_bp = std::clamp(camp.lookout * 40 - (night(hour) ? 1000 : 0), 500, 8000);
    camp.scouted = true;
    camp.known_to_player = true;
    int loot = 0;
    for (const auto& [item, units] : camp.loot) loot += units;
    std::string text = std::to_string(g->strength) + " fighters under " + g->leader +
                       "; supplies " + std::to_string(g->supplies) + "; morale " +
                       std::to_string(g->morale) + "; lookout " + std::to_string(camp.lookout) +
                       "; loot pile " + std::to_string(loot) + "; captives " +
                       std::to_string(camp.captives.size());
    if (roll_bp(r) < spotted_bp) {
        camp.alarm_days = 5;
        g->grudge_vs_player = std::min(100, g->grudge_vs_player + 5);
        return res(0, "spotted by the lookout; " + text);
    }
    return res(1, text);
}

WildActionResult attack_camp(WorldState& w, const Id& place, int companions, int hour) {
    Group* g = camp_group(w, place);
    if (g == nullptr) return res(-2, "no camp there");
    if (!adjacent(w, w.wild.player_place, place)) return res(-3, "the camp is not in reach");
    if (g->player_member || g->player_led) return res(-4, "that is your own band");
    GroupDef scratch;
    const GroupDef& def = *def_or_band(w, *g, scratch);
    Camp& camp = camp_ref(w, place);
    w.wild.player_place = place;
    Rng r = action_stream(w, "assault:" + place);

    SkirmishSide us = player_side(w, companions);
    // Friends lend spears: allies who hate this group, and the city watch for a
    // man it trusts (standing >= 50 with the city's faction).
    for (const GroupDef& other : w.wild.group_defs) {
        const Group* h = find_group(w.wild, other.id);
        if (h && h->active && h->allied && h->id != g->id &&
            (std::find(other.grudges.begin(), other.grudges.end(), g->id) != other.grudges.end() ||
             std::find(other.grudges.begin(), other.grudges.end(), def.faction) != other.grudges.end()))
            us.fighters += h->strength / 2;
    }
    if (standing(w.faction, city_faction_id(w)) >= 50 && watch_alive(w) >= 4) us.fighters += 2;

    SkirmishRequest req;
    req.attacker = us;
    const int surprise_lost = (!camp.scouted ? 15 : 0) + (camp.alarm_days > 0 ? 15 : 0);
    req.defender = SkirmishSide{g->id, g->strength, def.prowess_pct + 10 + surprise_lost, g->morale, false};
    req.place = place;
    req.hour = hour;
    req.context = "camp_assault";
    const SkirmishOutcome out = fight(w, req, r);
    if (!out.attacker_won) {
        g->strength = std::max(0, g->strength - out.defender_losses);
        g->morale = std::min(100, g->morale + 10);
        g->grudge_vs_player = std::min(100, g->grudge_vs_player + 20);
        camp.alarm_days = 5;
        player_defeated(w, g->id);
        return res(0, def.name + " threw back the assault");
    }
    if (out.player_wounded) ++w.wild.player_wounds;
    const int muster = g->strength;
    const std::size_t freed = camp.captives.size();
    int loot = 0;
    for (const auto& [item, units] : camp.loot) loot += units;
    const Silver bounty = static_cast<Silver>(kBountyPerFighter) * std::max(1, muster);
    credit_purse(w.property, kPlayer, bounty);
    add_standing(w.faction, city_faction_id(w), 5);
    if (!def.faction.empty()) add_standing(w.faction, def.faction, -10);
    w.wild.player_fear = std::min(100, w.wild.player_fear + 40);
    clear_camp(w, *g, true);  // loot to the player, captives freed (their quests complete)
    const std::string text = def.name + " cleared from " + place_name(w, place) + ": bounty " +
                             std::to_string(bounty) + " silver, " + std::to_string(loot) +
                             " goods taken, " + std::to_string(freed) + " captives freed";
    log_event(w, "wild_camp_cleared", text);
    spread_rumour(w, "camp:" + place, text);
    return res(1, text);
}

WildActionResult infiltrate_camp(WorldState& w, const Id& place, const std::string& objective,
                                 int hour) {
    if (objective != "free_captives" && objective != "steal_loot" && objective != "sabotage" &&
        objective != "learn")
        return res(-1, "unknown objective");
    Group* g = camp_group(w, place);
    if (g == nullptr) return res(-2, "no camp there");
    if (!adjacent(w, w.wild.player_place, place)) return res(-3, "the camp is not in reach");
    GroupDef scratch;
    const GroupDef& def = *def_or_band(w, *g, scratch);
    Camp& camp = camp_ref(w, place);
    Rng r = action_stream(w, "infiltrate:" + place);
    const bool insider = g->player_member || g->player_led;
    const int stealth_bp = insider ? 10000
                                   : std::clamp(6500 - camp.lookout * 40 + (night(hour) ? 1500 : 0) -
                                                    (camp.alarm_days > 0 ? 1500 : 0),
                                                500, 9000);
    w.wild.player_place = place;
    if (roll_bp(r) >= stealth_bp) {
        g->grudge_vs_player = std::min(100, g->grudge_vs_player + 15);
        camp.alarm_days = 5;
        SkirmishRequest req;
        req.attacker = SkirmishSide{g->id, std::max(1, 1 + g->strength / 3), def.prowess_pct, g->morale, false};
        req.defender = player_side(w, 0);
        req.place = place;
        req.hour = hour;
        req.context = "infiltration";
        const SkirmishOutcome out = fight(w, req, r);
        g->strength = std::max(0, g->strength - out.attacker_losses);
        if (out.attacker_won) {
            player_defeated(w, g->id);
            return res(0, "caught in the camp and beaten");
        }
        if (out.player_wounded) ++w.wild.player_wounds;
        return res(0, "caught in the camp, fought free");
    }
    camp.known_to_player = true;
    if (objective == "free_captives") {
        const std::vector<Id> captives = camp.captives;
        for (const Id& npc : captives) free_captive(w, npc, true);
        if (!insider) g->grudge_vs_player = std::min(100, g->grudge_vs_player + 20);
        return res(1, std::to_string(captives.size()) + " captives led out in the dark");
    }
    if (objective == "steal_loot") {
        int taken = 0;
        for (auto& [item, units] : camp.loot) {
            const int n = std::min(units, 10 - taken);
            if (n <= 0) break;
            units -= n;
            taken += n;
            give_player(w, item, n);
        }
        if (!insider) g->grudge_vs_player = std::min(100, g->grudge_vs_player + 15);
        return res(1, std::to_string(taken) + " goods lifted from the loot pile");
    }
    if (objective == "sabotage") {
        g->supplies = std::max(0, g->supplies - 30);
        g->morale = std::max(0, g->morale - 10);
        if (!insider) g->grudge_vs_player = std::min(100, g->grudge_vs_player + 10);
        return res(1, "their stores spoiled and their water fouled");
    }
    camp.scouted = true;
    const RaidAssessment next = assess_raid(w, g->id, w.day + 1);
    return res(1, next.target.empty() ? "they plan no raid"
                                      : "they mean to raid the " + next.target + " at " +
                                            place_name(w, next.place));
}

WildActionResult pay_off_group(WorldState& w, const Id& group) {
    Group* g = active_group(w, group);
    const GroupDef* def = find_group_def(w.wild, group);
    if (g == nullptr || def == nullptr) return res(-2, "no such band abroad");
    if (!adjacent(w, w.wild.player_place, g->camp) && !in_territory(*def, w.wild.player_place))
        return res(-3, "they are not here to parley");
    if (g->player_member || g->player_led) return res(-4, "that is your own band");
    if (g->grudge_vs_player >= 70) return res(-6, "there is blood between you");
    const Silver cost = static_cast<Silver>(kTributePerFighter) * std::max(1, g->strength);
    if (!take_from_purse(w.property, kPlayer, cost)) return res(-5, "not enough silver");
    g->paid_until = w.day + kTributeDays;
    g->supplies = std::min(100, g->supplies + static_cast<int>(cost));
    g->grudge_vs_player = std::max(0, g->grudge_vs_player - 10);
    if (!def->faction.empty()) add_standing(w.faction, def->faction, 3);
    log_event(w, "wild_tribute_paid", def->name + " took " + std::to_string(cost) +
                                          " silver to leave the city in peace for a month");
    return res(0, "paid " + std::to_string(cost) + " silver");
}

WildActionResult ally_with_group(WorldState& w, const Id& group) {
    Group* g = active_group(w, group);
    const GroupDef* def = find_group_def(w.wild, group);
    if (g == nullptr || def == nullptr) return res(-2, "no such band abroad");
    if (!adjacent(w, w.wild.player_place, g->camp)) return res(-3, "go to their camp to swear it");
    if (g->allied) return res(-4, "already allied");
    if (g->grudge_vs_player >= 30) return res(-6, "they do not trust you");
    bool reason = g->paid_until >= w.day || (!def->faction.empty() && standing(w.faction, def->faction) >= 25);
    for (const Id& enemy : def->grudges)  // the enemy of my enemy
        if (const Group* h = find_group(w.wild, enemy); h && h->times_cleared > 0) reason = true;
    if (!reason) return res(-6, "you have given them no reason");
    g->allied = true;
    g->grudge_vs_player = 0;
    if (!def->faction.empty()) add_standing(w.faction, def->faction, 5);
    for (const Id& enemy : def->grudges)
        if (const Group* h = find_group(w.wild, enemy); h && h->active)
            camp_ref(w, h->camp).known_to_player = true;  // they tell you where their enemies sleep
    return res(0, def->name + " will ride with you against their enemies");
}

WildActionResult join_group(WorldState& w, const Id& group) {
    Group* g = active_group(w, group);
    GroupDef scratch;
    if (g == nullptr) return res(-2, "no such band abroad");
    const GroupDef& def = *def_or_band(w, *g, scratch);
    if (!adjacent(w, w.wild.player_place, g->camp)) return res(-3, "go to their camp");
    if (!w.wild.player_group.empty()) return res(-4, "you already ride with a band");
    if (g->grudge_vs_player >= 50) return res(-6, "they would sooner kill you");
    if (def.kind == "partisans" && standing(w.faction, def.faction) < 20)
        return res(-6, "the zealots want proof of your faith");
    g->player_member = true;
    g->allied = true;
    ++g->strength;  // he fights with them now
    w.wild.player_group = g->id;
    w.wild.player_place = g->camp;
    if (!def.faction.empty()) add_standing(w.faction, def.faction, 10);
    log_event(w, "wild_player_joined", "the player now rides with " + def.name);
    return res(0, "you ride with " + def.name + " under " + g->leader);
}

WildActionResult leave_group(WorldState& w) {
    Group* g = active_group(w, w.wild.player_group);
    if (g == nullptr) {
        w.wild.player_group.clear();
        return res(-4, "you ride with no band");
    }
    const GroupDef* def = find_group_def(w.wild, g->id);
    w.wild.player_group.clear();
    g->player_member = false;
    if (def == nullptr) {  // the player's own band breaks up without him
        clear_camp(w, *g, false);
        return res(0, "your band scattered");
    }
    if (g->player_led) {
        g->player_led = false;
        g->leader = def->leaders[static_cast<std::size_t>(g->times_formed) % def->leaders.size()];
    }
    g->strength = std::max(1, g->strength - 1);
    g->grudge_vs_player = std::min(100, g->grudge_vs_player + 10);
    return res(0, "you left " + def->name);
}

WildActionResult challenge_leader(WorldState& w, const Id& group, int hour) {
    Group* g = active_group(w, group);
    GroupDef scratch;
    if (g == nullptr) return res(-2, "no such band abroad");
    const GroupDef& def = *def_or_band(w, *g, scratch);
    if (w.wild.player_place != g->camp) return res(-3, "a chief is challenged in his own camp");
    if (g->player_led) return res(-4, "you already lead them");
    if (!w.wild.player_group.empty() && w.wild.player_group != g->id)
        return res(-4, "you ride with another band");
    Rng r = action_stream(w, "duel:" + g->id);
    SkirmishRequest req;
    req.attacker = player_side(w, 0);
    req.defender = SkirmishSide{g->id + ":leader", 1, def.prowess_pct + 5 * g->strength / 2, g->morale, false};
    req.place = g->camp;
    req.hour = hour;
    req.context = "duel";
    const SkirmishOutcome out = fight(w, req, r);
    if (!out.attacker_won) {
        player_defeated(w, g->id);
        if (g->player_member) {
            g->player_member = false;
            w.wild.player_group.clear();
        }
        g->grudge_vs_player = std::min(100, g->grudge_vs_player + 25);
        return res(0, g->leader + " beat you before the whole camp");
    }
    if (out.player_wounded) ++w.wild.player_wounds;
    const std::string old = g->leader;
    g->leader = "player";
    g->player_led = true;
    g->player_member = true;
    g->allied = true;
    g->grudge_vs_player = 0;
    g->morale = std::max(0, g->morale - 10);
    w.wild.player_group = g->id;
    log_event(w, "wild_leader_overthrown", "the player struck down " + old + " and leads " + def.name);
    spread_rumour(w, "group:" + g->id, "a stranger now leads " + def.name);
    return res(1, "you lead " + def.name);
}

WildActionResult found_band(WorldState& w, const Id& camp_place, int companions) {
    const WildNode* n = find_node(w.wild, camp_place);
    if (n == nullptr || !n->camp_site) return res(-2, "no camp site there");
    if (w.wild.player_place != camp_place) return res(-3, "go there first");
    if (!w.wild.player_group.empty()) return res(-4, "you already ride with a band");
    if (const Camp* c = find_camp(w.wild, camp_place); c && c->status == "occupied")
        return res(-4, "the site is taken");
    Group& g = w.wild.groups[kPlayerBand];
    g.id = kPlayerBand;
    g.active = true;
    g.camp = camp_place;
    g.leader = "player";
    g.strength = 1 + std::clamp(companions, 0, 29);
    g.peak_strength = g.strength;
    g.supplies = 30;
    g.morale = 60;
    g.formed_day = w.day;
    g.player_led = true;
    g.player_member = true;
    g.rumoured = true;
    ++g.times_formed;
    Camp& camp = camp_ref(w, camp_place);
    camp.group = kPlayerBand;
    camp.status = "occupied";
    camp.since = w.day;
    camp.known_to_player = true;
    camp.scouted = true;
    w.wild.player_group = kPlayerBand;
    log_event(w, "wild_band_founded", "the player raised a band at " + n->name);
    return res(0, "your band camps at " + n->name);
}

WildActionResult lead_raid(WorldState& w, const std::string& target, int hour) {
    if (target != "fields" && target != "herds" && target != "caravans" && target != "market")
        return res(-1, "unknown target");
    Group* g = active_group(w, w.wild.player_group);
    if (g == nullptr) return res(-4, "you ride with no band");
    GroupDef scratch;
    const GroupDef& def = *def_or_band(w, *g, scratch);
    RaidAssessment a = assess_target(w, *g, def, target, w.day);
    if (a.place.empty()) return res(-4, "nothing of that kind within reach");
    if (a.blocked) return res(-4, "a sworn treaty forbids this raid");
    Rng r = action_stream(w, "raid:" + target);
    const RaidRecord rec = resolve_raid(w, *g, def, a, r, true, hour);
    ++w.wild.infamy;
    add_standing(w.faction, city_faction_id(w), -5);
    const std::string law = rec.slain.empty() ? "theft" : "murder";
    file_crime(w, law, {"watchman", "gatekeeper", "herdsman", "field hand"}, r,
               night(hour) ? 4000 : 8000);
    if (!rec.success) return res(0, rec.summary);
    // The share: a leader takes half, a follower a fifth.
    Camp& camp = camp_ref(w, g->camp);
    for (const auto& [item, units] : rec.goods) {
        int share = g->player_led ? units / 2 : units / 5;
        if (share <= 0) share = 1;
        if (item == "sheep") {
            credit_purse(w.property, kPlayer, static_cast<Silver>(2) * share);
            continue;
        }
        auto it = camp.loot.find(item);
        if (it == camp.loot.end()) continue;
        share = std::min(share, it->second);
        it->second -= share;
        give_player(w, item, share);
    }
    return res(1, rec.summary);
}

WildActionResult ransom_captive(WorldState& w, const Id& npc) {
    const std::string fate = npc_fate(w.wild, npc);
    if (fate.rfind("captive:", 0) != 0) return res(-2, "not held captive");
    Group* g = active_group(w, fate.substr(8));
    if (g == nullptr) return res(-2, "no one holds them");
    if (g->grudge_vs_player >= 80) return res(-6, "they will not deal with you");
    if (!take_from_purse(w.property, kPlayer, kRansomSilver)) return res(-5, "not enough silver");
    g->supplies = std::min(100, g->supplies + kRansomSilver / 2);
    free_captive(w, npc, true);
    return res(0, npc + " ransomed for " + std::to_string(kRansomSilver) + " silver");
}

WildActionResult escort_caravan(WorldState& w, const Id& run_id, int companions) {
    for (CaravanRun& run : w.wild.caravans) {
        if (run.id != run_id) continue;
        const CaravanDef* cd = caravan_def(w, run.def);
        if (cd == nullptr) return res(-2, "no such caravan");
        if (w.wild.player_place != cd->route[static_cast<std::size_t>(run.leg)])
            return res(-3, "meet the caravan where it stands");
        if (!w.wild.escorting.empty()) return res(-4, "already escorting");
        run.escorted = true;
        run.escort_fighters = 1 + std::clamp(companions, 0, 30);
        w.wild.escorting = run.id;
        return res(0, "you ride guard on " + cd->name);
    }
    return res(-2, "no such caravan on the road");
}

WildActionResult abandon_escort(WorldState& w) {
    if (w.wild.escorting.empty()) return res(-4, "escorting no one");
    for (CaravanRun& run : w.wild.caravans)
        if (run.id == w.wild.escorting) {
            run.escorted = false;
            run.escort_fighters = 0;
        }
    w.wild.escorting.clear();
    return res(0, "you left the caravan");
}

WildActionResult rob_caravan(WorldState& w, const Id& run_id, int companions, int hour) {
    auto it = std::find_if(w.wild.caravans.begin(), w.wild.caravans.end(),
                           [&](const CaravanRun& c) { return c.id == run_id; });
    if (it == w.wild.caravans.end()) return res(-2, "no such caravan on the road");
    const CaravanDef* cd = caravan_def(w, it->def);
    if (cd == nullptr) return res(-2, "no such caravan");
    if (w.wild.player_place != cd->route[static_cast<std::size_t>(it->leg)])
        return res(-3, "the caravan is elsewhere");
    if (w.wild.escorting == run_id) return res(-4, "you are its guard");
    Rng r = action_stream(w, "rob:" + run_id);
    SkirmishRequest req;
    req.attacker = player_side(w, companions);
    if (Group* band = active_group(w, w.wild.player_group)) req.attacker.fighters += band->strength;
    req.defender = SkirmishSide{run_id, it->guards, 100, 55, false};
    req.place = w.wild.player_place;
    req.hour = hour;
    req.context = "raid";
    const SkirmishOutcome out = fight(w, req, r);
    it->guards = std::max(0, it->guards - out.defender_losses);
    if (!out.attacker_won) {
        player_defeated(w, run_id);
        return res(0, "the guards of " + cd->name + " drove you off");
    }
    if (out.player_wounded) ++w.wild.player_wounds;
    int units = 0;
    for (const auto& [item, n] : it->goods) {
        give_player(w, item, n);
        units += n;
    }
    w.wild.caravans.erase(it);
    ++w.wild.caravans_robbed;
    ++w.wild.infamy;
    add_standing(w.faction, city_faction_id(w), -5);
    if (!cd->faction.empty()) add_standing(w.faction, cd->faction, -5);
    file_crime(w, "theft", {"gatekeeper"}, r, 9000);  // the survivors report at the gate
    const std::string text = cd->name + " robbed on the road: " + std::to_string(units) + " goods";
    log_event(w, "wild_caravan_robbed", text);
    spread_rumour(w, "wild:caravan", text);
    return res(1, text);
}

WildActionResult search_site(WorldState& w, const Id& place, int hour) {
    const WildNode* n = find_node(w.wild, place);
    auto sit = w.wild.sites.find(place);
    if (n == nullptr || sit == w.wild.sites.end()) return res(-2, "nothing to search there");
    if (w.wild.player_place != place) return res(-3, "go there first");
    if (const Camp* c = find_camp(w.wild, place);
        c && c->status == "occupied" && c->group != w.wild.player_group)
        return res(-4, "men are camped in it");
    SiteState& site = sit->second;
    Rng r = action_stream(w, "search:" + place);
    ++site.searches;
    site.last_search = w.day;
    std::string text;
    for (const auto& [risk, bp] : n->risks) {
        if (roll_bp(r) >= bp) continue;
        if (risk == "collapse") {
            ++w.wild.player_wounds;
            text += "the roof came down on you; ";
        } else if (risk == "restless_dead") {
            add_favour(w.magic, "underworld_queen", -5);  // D-004 Mythic: the dead notice
            text += "something of the dead followed you out; ";
        } else if (risk == "snake" || risk == "jackals") {
            const Id enc = risk == "snake" ? "venomous_snake" : "jackal_pack";
            for (const EncounterDef& e : w.wild.encounter_defs) {
                if (e.id != enc) continue;
                SkirmishRequest req;
                req.attacker = SkirmishSide{e.id, r.int_in(e.foes_min, e.foes_max), e.prowess_pct, 60, false};
                req.defender = player_side(w, 0);
                req.place = place;
                req.hour = hour;
                req.context = "encounter";
                const SkirmishOutcome out = fight(w, req, r);
                if (out.attacker_won) {
                    player_defeated(w, e.id);
                    return res(0, text + e.name + " drove you out");
                }
                if (out.player_wounded) ++w.wild.player_wounds;
                text += e.name + " fought off; ";
            }
        }
    }
    int taken = 0;
    for (auto& [item, units] : site.loot_left) {
        const int k = std::min(units, 3 - taken);
        if (k <= 0) continue;
        units -= k;
        taken += k;
        give_player(w, item, k);
        text += std::to_string(k) + " " + item + "; ";
    }
    bool empty = true;
    for (const auto& [item, units] : site.loot_left)
        if (units > 0) empty = false;
    if (empty && site.emptied_day == 0) site.emptied_day = w.day;
    if (n->kind == "tomb" && taken > 0) {
        // Canon law (laws.csv tomb_robbery): a crime, witnessed or not.
        file_crime(w, "tomb_robbery", {"herdsman", "gatekeeper", "field hand"}, r,
                   night(hour) ? 1000 : 4000);
        spread_rumour(w, "wild:tombs", "someone has opened graves at " + n->name);
    }
    if (taken == 0) return res(0, text + "nothing left worth carrying");
    return res(1, text);
}

std::vector<std::string> hear_rumours(WorldState& w, const Id& npc) {
    std::vector<std::string> out;
    const Npc* n = find_npc(w.population, npc);
    if (n == nullptr) return out;
    for (const MemoryEntry& m : n->memory) {
        const bool camp = m.subject.rfind("camp:", 0) == 0;
        if (!camp && m.subject.rfind("group:", 0) != 0 && m.subject.rfind("wild:", 0) != 0) continue;
        out.push_back("day " + std::to_string(m.day) + ": " + m.fact);
        if (camp) {
            const Id place = m.subject.substr(5);
            auto it = w.wild.camps.find(place);
            if (it != w.wild.camps.end() && it->second.status == "occupied") it->second.known_to_player = true;
        }
    }
    return out;
}

}  // namespace sim
