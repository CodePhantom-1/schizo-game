// WildWorld.cpp — W4-C: the wild's helpers that reach across the WorldState
// (events, rumours, captives, camps, the player's side of a fight). Every
// cross-module write goes through that module's public API.
#include "WildInternal.hpp"

#include "sim/Actions.hpp"

#include <algorithm>

namespace sim::wild {

SkirmishOutcome fight(WorldState& w, const SkirmishRequest& req, Rng& r) {
    const SkirmishResolver resolve = w.wild.skirmish ? w.wild.skirmish : &stub_skirmish;
    SkirmishOutcome out = resolve(req, r);
    out.attacker_losses = std::clamp(out.attacker_losses, 0, std::max(0, req.attacker.fighters));
    out.defender_losses = std::clamp(out.defender_losses, 0, std::max(0, req.defender.fighters));
    return out;
}

Id city_faction_id(const WorldState& w) { return city_faction(w.db, kCity); }

namespace {
bool is_canon_person(const WorldState& w, const Id& npc) {
    const auto row = w.db.find("people", npc);
    return row && row->get("tag") == "CANON";  // named leaders never die in a raid
}
}  // namespace

int watch_alive(const WorldState& w) {
    int n = 0;
    for (const Npc& npc : w.population.npcs)
        if ((npc.role == "watchman" || npc.role == "gatekeeper" || npc.role == "paladin") &&
            !npc_out_of_play(w.wild, npc.id))
            ++n;
    return n;
}

Id pick_npc(const WorldState& w, const std::vector<std::string>& roles, Rng& r) {
    std::vector<Id> pool;
    for (const Npc& npc : w.population.npcs)
        if (std::find(roles.begin(), roles.end(), npc.role) != roles.end() &&
            !npc_out_of_play(w.wild, npc.id) && !is_canon_person(w, npc.id))
            pool.push_back(npc.id);
    if (pool.empty()) return "";
    return pool[static_cast<std::size_t>(r.int_in(0, static_cast<int>(pool.size()) - 1))];
}

void log_event(WorldState& w, const Id& rule_id, const std::string& summary) {
    w.events.fired.push_back(TriggeredEvent{rule_id, w.day, summary});
    ++w.events.fire_count_by_rule[rule_id];
}

void spread_rumour(WorldState& w, const Id& subject, const std::string& fact) {
    // The city's talkers hear it first; tick_population carries it on.
    static const char* const kTellers[] = {"gatekeeper", "tavern keeper", "herdsman", "market trader"};
    for (const char* role : kTellers)
        for (const Npc& npc : w.population.npcs)
            if (npc.role == role && !npc_out_of_play(w.wild, npc.id)) {
                witness(w.population, npc.id, subject, fact, w.day);
                break;
            }
}

namespace {
Id rescue_quest_id(const Id& npc) { return "rescue_" + npc; }

std::string npc_name(const WorldState& w, const Id& npc) {
    const Npc* n = find_npc(w.population, npc);
    return n && !n->name.empty() ? n->name : npc;
}

void remove_from_camps(WorldState& w, const Id& npc) {
    for (auto& [place, camp] : w.wild.camps)
        camp.captives.erase(std::remove(camp.captives.begin(), camp.captives.end(), npc),
                            camp.captives.end());
}
}  // namespace

void take_captive(WorldState& w, const Id& npc, Group& g, Camp& camp) {
    if (npc.empty() || npc_out_of_play(w.wild, npc)) return;
    w.wild.npc_fate[npc] = "captive:" + g.id;
    w.wild.captive_since[npc] = w.day;
    camp.captives.push_back(npc);
    // Living-world §8 "emergent" quest source: the captive's kin plead for a
    // rescue. The def is created on first capture (Snapshot saves QUESTS_DEFS,
    // so it survives a reload); the deadline is the day the captive is sold east.
    const Id qid = rescue_quest_id(npc);
    const bool has_def = std::any_of(w.quests.defs.begin(), w.quests.defs.end(),
                                     [&](const QuestDef& d) { return d.id == qid; });
    if (!has_def) {
        QuestDef def;
        def.id = qid;
        def.kind = "emergent";
        def.deadline_days = kCaptiveDays;
        def.reward_silver = kRescueRewardSilver;
        def.reward_faction = city_faction_id(w);
        def.reward_standing = 5;
        w.quests.defs.push_back(def);
    }
    if (find_active(w.quests, qid) == nullptr) {
        Quest& q = accept(w.quests, qid, w.day);
        q.deadline = w.day + kCaptiveDays;
    }
    const WildNode* node = find_node(w.wild, camp.place);
    advance_stage(w.quests, qid, w.day, "captive",
                  npc_name(w, npc) + " was carried off by " +
                      (find_group_def(w.wild, g.id) ? find_group_def(w.wild, g.id)->name : g.id) +
                      " toward " + (node ? node->name : camp.place));
}

void free_captive(WorldState& w, const Id& npc, bool rescued_by_player) {
    if (w.wild.npc_fate.count(npc) == 0) return;
    w.wild.npc_fate.erase(npc);
    w.wild.captive_since.erase(npc);
    remove_from_camps(w, npc);
    const Id qid = rescue_quest_id(npc);
    if (find_active(w.quests, qid) == nullptr) return;
    if (rescued_by_player) {
        complete_quest(w, qid, w.day);  // pays the rescue reward (Actions)
    } else {
        advance_stage(w.quests, qid, w.day, "came_home", npc_name(w, npc) + " walked home when the camp broke up");
        complete(w.quests, qid, w.day);  // no reward: the player did nothing
    }
}

void kill_npc(WorldState& w, const Id& npc) {
    if (npc.empty()) return;
    const Id qid = rescue_quest_id(npc);
    w.wild.npc_fate[npc] = "slain";
    w.wild.captive_since.erase(npc);
    remove_from_camps(w, npc);
    if (find_active(w.quests, qid) != nullptr) {
        advance_stage(w.quests, qid, w.day, "slain", npc_name(w, npc) + " is dead");
        fail(w.quests, qid);
    }
}

Camp& camp_ref(WorldState& w, const Id& place) {
    Camp& c = w.wild.camps[place];
    c.place = place;
    return c;
}

void clear_camp(WorldState& w, Group& g, bool by_player) {
    if (!g.camp.empty()) {
        Camp& camp = camp_ref(w, g.camp);
        const std::vector<Id> captives = camp.captives;
        for (const Id& npc : captives) free_captive(w, npc, by_player);
        if (by_player)
            for (const auto& [item, units] : camp.loot) give_player(w, item, units);
        camp.loot.clear();
        camp.status = by_player ? "cleared" : "abandoned";
        camp.since = w.day;
        camp.lookout = 0;
        camp.alarm_days = 0;
    }
    g.active = false;
    g.camp.clear();
    g.strength = 0;
    g.cleared_day = w.day;
    g.pressure = 0;
    g.paid_until = 0;
    g.allied = false;
    if (by_player) ++g.times_cleared;
    if (w.wild.player_group == g.id) {
        w.wild.player_group.clear();
    }
    g.player_member = false;
    g.player_led = false;
}

Id free_camp_site(const WorldState& w, const GroupDef& def, const Id& except) {
    for (const Id& site : def.camp_sites) {
        if (site == except) continue;
        const Camp* c = find_camp(w.wild, site);
        if (c && c->status == "occupied" && c->group != def.id) continue;
        return site;
    }
    return "";
}

void move_camp(WorldState& w, Group& g, const std::string& why) {
    const GroupDef* def = find_group_def(w.wild, g.id);
    if (def == nullptr || g.camp.empty()) return;
    const Id to = free_camp_site(w, *def, g.camp);
    if (to.empty()) return;
    Camp& old = camp_ref(w, g.camp);
    Camp& fresh = camp_ref(w, to);
    fresh.group = g.id;
    fresh.status = "occupied";
    fresh.since = w.day;
    fresh.loot = old.loot;          // they carry the pile
    fresh.captives = old.captives;  // and drive the captives before them
    fresh.lookout = old.lookout;
    fresh.alarm_days = 3;
    fresh.known_to_player = false;
    fresh.scouted = false;
    old.loot.clear();
    old.captives.clear();
    old.status = "abandoned";
    old.since = w.day;
    old.lookout = 0;
    g.camp = to;
    g.rumoured = false;
    const WildNode* n = find_node(w.wild, to);
    log_event(w, "wild_camp_moved",
              def->name + " broke camp (" + why + ") and moved to " + (n ? n->name : to));
}

SkirmishSide player_side(const WorldState& w, int companions) {
    SkirmishSide s;
    s.id = kPlayer;
    s.fighters = 1 + std::clamp(companions, 0, 100);
    s.prowess_pct = std::max(40, 100 - 10 * w.wild.player_wounds);
    s.morale = 60;
    s.has_player = true;
    return s;
}

void player_defeated(WorldState& w, const std::string& by) {
    ++w.wild.player_wounds;
    const Silver lost = purse_of(w) / 2;
    if (lost > 0) take_from_purse(w.property, kPlayer, lost);
    Inventory& inv = w.inventories[kPlayer];
    for (auto& [item, count] : inv.counts) count -= count / 3;
    (void)by;
}

Silver purse_of(const WorldState& w) { return purse(w.property, kPlayer); }

void give_player(WorldState& w, const Id& item, int units) {
    if (units <= 0 || item.empty()) return;
    w.inventories[kPlayer].counts[item] += units;
}

void push_raid(WorldState& w, RaidRecord rec) {
    rec.id = "raid_" + std::to_string(w.wild.next_raid++);
    w.wild.raids.push_back(std::move(rec));
    if (w.wild.raids.size() > static_cast<std::size_t>(kMaxRaidLog))
        w.wild.raids.erase(w.wild.raids.begin());
}

void push_encounter(WorldState& w, EncounterRecord rec) {
    rec.id = "enc_" + std::to_string(w.wild.next_encounter++);
    w.wild.encounters.push_back(std::move(rec));
    if (w.wild.encounters.size() > static_cast<std::size_t>(kMaxEncounterLog))
        w.wild.encounters.erase(w.wild.encounters.begin());
}

const CaravanDef* caravan_def(const WorldState& w, const Id& id) {
    for (const CaravanDef& c : w.wild.caravan_defs)
        if (c.id == id) return &c;
    return nullptr;
}

const CaravanRun* caravan_in_territory(const WorldState& w, const GroupDef& def) {
    for (const CaravanRun& run : w.wild.caravans) {
        const CaravanDef* cd = caravan_def(w, run.def);
        if (cd == nullptr || run.leg >= static_cast<int>(cd->route.size())) continue;
        if (in_territory(def, cd->route[static_cast<std::size_t>(run.leg)])) return &run;
    }
    return nullptr;
}

CaravanRun* caravan_in_territory(WorldState& w, const GroupDef& def) {
    return const_cast<CaravanRun*>(caravan_in_territory(static_cast<const WorldState&>(w), def));
}

Id target_place(const WorldState& w, const GroupDef& def, const std::string& target) {
    static const char* const kFields[] = {"fields_beyond_the_gate_place", "canal_head_place",
                                          "date_grove_place"};
    if (target == "fields") {
        for (const char* p : kFields)
            if (in_territory(def, p) && find_node(w.wild, p)) return p;
        return "";
    }
    if (target == "herds") return in_territory(def, "grazing_lands_place") ? "grazing_lands_place" : "";
    if (target == "market") return in_territory(def, kGate) ? "market_square_place" : "";
    if (target == "caravans") {
        const CaravanRun* run = caravan_in_territory(w, def);
        if (run == nullptr) return "";
        const CaravanDef* cd = caravan_def(w, run->def);
        return cd->route[static_cast<std::size_t>(run->leg)];
    }
    return "";
}

}  // namespace sim::wild
