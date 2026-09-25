// test_wild.cpp — W4-C: the wild lands. Data integrity, the raid formula,
// travel and encounters, camps and captives, the player's verbs, caravans,
// sites, weather, the skirmish stub, save round-trips and D-022 (no float).
#include "sim/Snapshot.hpp"
#include "sim/Test.hpp"
#include "sim/WildActions.hpp"

#include "../src/WildInternal.hpp"  // test-only: force a capture

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

using namespace sim;

namespace {

constexpr const char* kCanon = "../db/canon";

WorldState fresh(std::uint64_t seed = 11) {
    WorldState w;
    w.init(kCanon, seed);
    return w;
}

int count_events(const WorldState& w, const std::string& prefix) {
    int n = 0;
    for (const auto& [id, c] : w.events.fire_count_by_rule)
        if (id.rfind(prefix, 0) == 0) n += c;
    return n;
}

bool has_open_crime(const WorldState& w, const std::string& law) {
    for (const Crime& c : w.justice.open_crimes)
        if (c.criminal == "player" && c.law_row == law) return true;
    for (const Hearing& h : w.justice.verdicts) (void)h;
    return false;
}

}  // namespace

// --- data -------------------------------------------------------------------------

static bool test_region_data_is_whole() {
    WorldState w = fresh();
    const WildState& s = w.wild;
    int wild = 0, minx = 0, maxx = 0, miny = 0, maxy = 0;
    std::set<std::string> kinds;
    for (const auto& [id, n] : s.nodes) {
        if (n.source != "wild") continue;
        ++wild;
        kinds.insert(n.kind);
        minx = std::min(minx, n.x_m);
        maxx = std::max(maxx, n.x_m);
        miny = std::min(miny, n.y_m);
        maxy = std::max(maxy, n.y_m);
        SIM_CHECK(n.danger >= 0 && n.danger <= 10);
        for (const auto& [item, units] : n.loot) SIM_CHECK(w.db.has("items", item) && units > 0);
    }
    SIM_CHECK(wild >= 12);
    // The spec's terrain: fields, riverbank, marsh, desert edge, roads, ruins, caves, tombs
    // (field/pasture also come from places.csv's fields_beyond_the_gate/grazing_lands).
    for (const char* k : {"field", "riverbank", "marsh", "desert_edge", "road", "ruin", "cave", "tomb"})
        SIM_CHECK(kinds.count(k) == 1);
    // D-002: about 20 km² (bounding box of the wild places, within 25 km²).
    const long long area = static_cast<long long>(maxx - minx) * (maxy - miny);
    SIM_CHECK(area > 10'000'000LL && area <= 25'000'000LL);
    // The places.csv ids the roads start from are reused, not duplicated.
    for (const char* p : {"moon_gate_place", "fields_beyond_the_gate_place", "grazing_lands_place",
                          "lighthouse_wharf_place"}) {
        const WildNode* n = find_node(s, p);
        SIM_CHECK(n != nullptr && n->source == "place");
        SIM_CHECK(!w.db.has("wild_places", p));
    }
    // Every canon link survived loading (no dangling endpoint).
    SIM_CHECK_EQ(s.links.size(), w.db.rows("wild_links").size());
    for (const WildLink& l : s.links) SIM_CHECK(l.distance_m > 0 && l.danger >= 0);
    // Every node reachable from the gate by foot or boat.
    for (const auto& [id, n] : s.nodes) {
        if (id == "moon_gate_place") continue;
        const bool foot = !find_route(w, "moon_gate_place", id, "foot").empty();
        const bool boat = !find_route(w, "lighthouse_wharf_place", id, "reed_boat").empty();
        SIM_CHECK(foot || boat);
    }
    return true;
}

static bool test_groups_caravans_encounters_reference_canon() {
    WorldState w = fresh();
    const WildState& s = w.wild;
    SIM_CHECK(s.group_defs.size() >= 5);
    std::set<std::string> kinds;
    for (const GroupDef& g : s.group_defs) {
        kinds.insert(g.kind);
        SIM_CHECK(g.faction.empty() || w.db.has("factions", g.faction));
        for (const Id& site : g.camp_sites) SIM_CHECK(find_node(s, site) && find_node(s, site)->camp_site);
        for (const Id& p : g.territory) SIM_CHECK(find_node(s, p) != nullptr);
        for (const Id& gr : g.grudges)
            SIM_CHECK(w.db.has("factions", gr) || find_group_def(s, gr) != nullptr);
        SIM_CHECK(!g.leaders.empty() && g.max_strength >= g.base_strength);
    }
    SIM_CHECK(kinds.count("bandits") && kinds.count("raiders") && kinds.count("partisans"));
    // Canon's eastern raiders, the Suti and the southern rebellion all field a band.
    std::set<Id> factions;
    for (const GroupDef& g : s.group_defs) factions.insert(g.faction);
    SIM_CHECK(factions.count("the_barbarians") && factions.count("suti") &&
              factions.count("neo_sumerian_rebellion"));
    for (const CaravanDef& c : s.caravan_defs) {
        for (std::size_t i = 1; i < c.route.size(); ++i) {
            bool linked = false;
            for (const WildLink* l : links_of(s, c.route[i - 1]))
                linked = linked || other_end(*l, c.route[i - 1]) == c.route[i];
            SIM_CHECK(linked);  // real routes: consecutive stops share a road
        }
        SIM_CHECK_EQ(c.route.back(), Id("moon_gate_place"));
        for (const auto& [item, units] : c.goods) SIM_CHECK(w.db.has("items", item) && units > 0);
    }
    std::set<std::string> enc_kinds;
    for (const EncounterDef& e : s.encounter_defs) enc_kinds.insert(e.kind);
    for (const char* k : {"bandits", "raiders", "refugees", "merchants", "animals"})
        SIM_CHECK(enc_kinds.count(k) == 1);
    SIM_CHECK(s.modes.count("foot") && s.modes.count("reed_boat") && s.modes.count("horse"));
    SIM_CHECK(s.weather_defs.size() >= 5);
    return true;
}

static bool test_day_one_camp_and_herds() {
    WorldState w = fresh();
    const Group* jackals = find_group(w.wild, "cavern_jackals");
    SIM_CHECK(jackals != nullptr && !jackals->active);  // forms on the first tick
    w.advance_days(1);
    jackals = find_group(w.wild, "cavern_jackals");
    SIM_CHECK(jackals->active);
    const Camp* camp = find_camp(w.wild, jackals->camp);
    SIM_CHECK(camp != nullptr && camp->status == "occupied" && camp->lookout > 0);
    SIM_CHECK(!find_group(w.wild, "eastern_warband")->active);  // needs the drought
    SIM_CHECK_EQ(w.wild.herd_head, kHerdStart);
    SIM_CHECK_EQ(w.wild.player_place, Id("moon_gate_place"));
    // Rumours in the city point at the camp.
    bool heard = false;
    w.advance_days(3);
    for (const Npc& n : w.population.npcs)
        for (const MemoryEntry& m : n.memory) heard = heard || m.subject == "camp:" + jackals->camp;
    SIM_CHECK(heard);
    return true;
}

// --- the raid formula -------------------------------------------------------------

static bool test_raid_formula_moves_with_hunger_defence_and_bribes() {
    WorldState w = fresh();
    w.advance_days(1);
    Group& g = w.wild.groups["cavern_jackals"];
    g.strength = 6;
    g.supplies = 50;
    const RaidAssessment calm = assess_raid(w, g.id, w.day);
    SIM_CHECK_EQ(calm.score, calm.hunger + calm.opportunity - calm.defence - calm.fear);
    w.facts.drought_stage = 4;
    w.facts.war_stage = 2;
    const RaidAssessment dry = assess_raid(w, g.id, w.day);
    SIM_CHECK(dry.hunger > calm.hunger);
    SIM_CHECK(dry.chance_bp >= calm.chance_bp);
    g.supplies = 0;  // an empty larder
    const RaidAssessment starving = assess_raid(w, g.id, w.day);
    SIM_CHECK(starving.hunger > dry.hunger && starving.chance_bp >= dry.chance_bp);
    SIM_CHECK(starving.score > 0 && starving.chance_bp >= 0 && starving.chance_bp <= kRaidMaxBp);
    SIM_CHECK(starving.chance_bp == std::min(kRaidMaxBp, starving.score * kRaidBpPerPoint) || weather_on(w, w.day) == "sandstorm");
    // Defence: the player's reputation with the city.
    add_standing(w.faction, wild::city_faction_id(w), 100);
    const RaidAssessment guarded = assess_raid(w, g.id, w.day);
    SIM_CHECK(guarded.defence > starving.defence);
    // Fear: a player who has broken camps.
    w.wild.player_fear = 100;
    const RaidAssessment afraid = assess_raid(w, g.id, w.day);
    SIM_CHECK(afraid.fear > guarded.fear && afraid.chance_bp <= guarded.chance_bp);
    // A bought month of peace: nothing but caravans.
    g.paid_until = w.day + 10;
    const RaidAssessment bribed = assess_raid(w, g.id, w.day);
    SIM_CHECK(bribed.target.empty() || bribed.target == "caravans");
    return true;
}

static bool test_a_raid_takes_goods_through_the_economy_hook() {
    WorldState w = fresh();
    w.advance_days(1);
    Group& g = w.wild.groups["cavern_jackals"];
    g.strength = 20;
    const GroupDef& def = *find_group_def(w.wild, g.id);
    const std::int64_t before = w.economy.stock_by_city_item["city_of_the_moon/grain"];
    int successes = 0;
    for (int k = 0; k < 20 && successes == 0; ++k) {
        RaidAssessment a = wild::assess_target(w, g, def, "fields", w.day);
        SIM_CHECK(!a.place.empty());
        Rng r = wild::action_stream(w, "test");
        const RaidRecord rec = wild::resolve_raid(w, g, def, a, r, false, 3);
        if (rec.success) {
            ++successes;
            SIM_CHECK(rec.goods.count("grain") == 1);
            SIM_CHECK_EQ(w.economy.stock_by_city_item["city_of_the_moon/grain"],
                         before - rec.goods.at("grain"));
            SIM_CHECK(w.wild.camps[g.camp].loot["grain"] >= rec.goods.at("grain"));
        }
        g.strength = 20;
    }
    SIM_CHECK(successes == 1);
    SIM_CHECK(count_events(w, "wild_raid_fields") >= 1);
    SIM_CHECK(!w.wild.raids.empty());
    return true;
}

// --- travel -------------------------------------------------------------------------

static bool test_travel_costs_time_and_refuses_cleanly() {
    WorldState w = fresh();
    w.advance_days(1);
    SIM_CHECK_EQ(travel(w, "nowhere_place", "foot", 8).code, -2);
    SIM_CHECK_EQ(travel(w, "date_grove_place", "wings", 8).code, -3);
    SIM_CHECK_EQ(travel(w, "date_grove_place", "horse", 8).code, -4);  // no horse
    SIM_CHECK_EQ(travel(w, "moon_gate_place", "foot", 8).code, -6);
    SIM_CHECK_EQ(travel(w, "riverbank_reeds_place", "reed_boat", 8).code, -5);  // no river at the gate
    const TravelResult t = travel(w, "fields_beyond_the_gate_place", "foot", 8, "flee");
    SIM_CHECK(t.code == 0 || t.code == 1);
    SIM_CHECK(t.minutes > 0);
    SIM_CHECK_EQ(w.wild.player_place, t.stopped_at);
    // A horse doubles the pace of a man on foot.
    w.inventories["player"].counts["riding_horse"] = 1;
    w.wild.player_place = "moon_gate_place";
    const WildLink* road = nullptr;
    for (const WildLink& l : w.wild.links)
        if (l.id == "moon_gate__fields_beyond_the_gate") road = &l;
    SIM_CHECK(road != nullptr);
    const Id weather = weather_on(w, w.day);
    SIM_CHECK(leg_minutes(w.wild, *road, w.wild.modes.at("horse"), weather) * 2 <=
              leg_minutes(w.wild, *road, w.wild.modes.at("foot"), weather) + 1);
    return true;
}

static bool test_a_journey_takes_hours_and_thirst() {
    WorldState w = fresh(21);
    w.advance_days(1);
    w.wild.player_place = "canal_head_place";
    w.inventories["player"].counts["water"] = 20;
    w.inventories["player"].counts["bread"] = 5;
    needs_of(w.needs, "player").thirst = 55;
    const TravelResult t = travel(w, "city_of_the_sun", "foot", 6, "flee", 30);
    SIM_CHECK(t.minutes >= 600);  // 45 km on foot: a long day and more
    SIM_CHECK_EQ(t.code, 0);
    SIM_CHECK_EQ(w.wild.player_place, Id("city_of_the_sun"));  // an off-map venture
    SIM_CHECK(w.inventories["player"].counts["water"] < 20);  // the road drinks
    // Without water, in the drought, the long road east kills.
    WorldState d = fresh(22);
    d.advance_days(1);
    d.facts.drought_stage = 6;
    d.wild.player_place = "eastern_waystation_place";
    const TravelResult t2 = travel(d, "city_of_the_warrior_spirit", "foot", 10, "flee", 50);
    SIM_CHECK_EQ(t2.code, 1);
    SIM_CHECK_EQ(t2.halted_by, std::string("collapse"));
    SIM_CHECK(d.wild.player_wounds >= 1);
    return true;
}

static bool test_encounters_respect_terrain_and_log() {
    WorldState w = fresh(5);
    w.advance_days(1);
    w.facts.drought_stage = 3;
    w.inventories["player"].counts["water"] = 1000;
    std::set<std::string> kinds;
    for (int k = 0; k < 120; ++k) {
        w.wild.player_place = "desert_edge_place";
        w.wild.player_wounds = 0;
        credit_purse(w.property, "player", 50);
        (void)travel(w, "desert_caverns_place", "foot", 22, "flee", 30);
    }
    SIM_CHECK(!w.wild.encounters.empty());
    for (const EncounterRecord& e : w.wild.encounters) {
        kinds.insert(e.encounter);
        SIM_CHECK(e.encounter != "lion_in_the_reeds");  // lions keep to the reeds
        SIM_CHECK(e.encounter != "caravan_merchants");  // merchants travel by day
        SIM_CHECK(!e.outcome.empty());
    }
    SIM_CHECK(w.wild.encounters.size() <= static_cast<std::size_t>(kMaxEncounterLog));
    return true;
}

// --- camps, captives, the player's verbs ----------------------------------------------

static bool test_clearing_a_camp_pays_and_frees() {
    WorldState w = fresh();
    w.advance_days(1);
    Group& g = w.wild.groups["cavern_jackals"];
    Camp& camp = w.wild.camps[g.camp];
    const Id camp_place = g.camp;
    const Npc& victim = w.population.npcs.back();
    wild::take_captive(w, victim.id, g, camp);
    SIM_CHECK(npc_fate(w.wild, victim.id) == "captive:cavern_jackals");
    SIM_CHECK(find_active(w.quests, "rescue_" + victim.id) != nullptr);  // the rescue quest starts
    camp.loot["gold"] = 2;
    SIM_CHECK_EQ(attack_camp(w, camp_place, 2, 12).code, -3);  // not there yet
    w.wild.player_place = camp_place;
    const Silver purse_before = purse(w.property, "player");
    const int standing_before = standing(w.faction, wild::city_faction_id(w));
    WildActionResult r;
    for (int k = 0; k < 5; ++k) {
        r = attack_camp(w, camp_place, 60, 12);
        if (r.code == 1) break;
    }
    SIM_CHECK_EQ(r.code, 1);
    SIM_CHECK(!find_group(w.wild, "cavern_jackals")->active);
    SIM_CHECK_EQ(find_camp(w.wild, camp_place)->status, std::string("cleared"));
    SIM_CHECK(purse(w.property, "player") > purse_before);  // bounty + rescue reward
    SIM_CHECK(standing(w.faction, wild::city_faction_id(w)) > standing_before);
    SIM_CHECK(w.inventories["player"].counts["gold"] >= 2);  // the loot pile
    SIM_CHECK(npc_fate(w.wild, victim.id).empty());
    SIM_CHECK(std::find(w.quests.completed.begin(), w.quests.completed.end(),
                        "rescue_" + victim.id) != w.quests.completed.end());
    SIM_CHECK(count_events(w, "wild_camp_cleared") == 1);
    SIM_CHECK(w.wild.player_fear > 0);
    return true;
}

static bool test_unrescued_captives_are_sold_east() {
    WorldState w = fresh();
    w.advance_days(1);
    Group& g = w.wild.groups["cavern_jackals"];
    const Npc& victim = w.population.npcs.back();
    wild::take_captive(w, victim.id, g, w.wild.camps[g.camp]);
    w.advance_days(kCaptiveDays + 2);
    SIM_CHECK_EQ(npc_fate(w.wild, victim.id), std::string("sold_east"));
    SIM_CHECK(find_active(w.quests, "rescue_" + victim.id) == nullptr);
    SIM_CHECK(std::find(w.quests.failed_list.begin(), w.quests.failed_list.end(),
                        "rescue_" + victim.id) != w.quests.failed_list.end());
    return true;
}

static bool test_join_take_over_and_ride_as_a_bandit() {
    WorldState w = fresh();
    w.advance_days(1);
    const Id camp = w.wild.groups["cavern_jackals"].camp;
    SIM_CHECK_EQ(join_group(w, "cavern_jackals").code, -3);
    w.wild.player_place = camp;
    SIM_CHECK_EQ(join_group(w, "cavern_jackals").code, 0);
    SIM_CHECK_EQ(w.wild.player_group, Id("cavern_jackals"));
    // Insiders slip about the camp unseen.
    SIM_CHECK_EQ(infiltrate_camp(w, camp, "learn", 12).code, 1);
    // Members are not ambushed by their own.
    // Take the band: duel its chief (retry: each duel is a fresh roll).
    WildActionResult duel;
    for (int k = 0; k < 20; ++k) {
        w.wild.player_wounds = 0;
        if (w.wild.player_group.empty()) (void)join_group(w, "cavern_jackals");
        duel = challenge_leader(w, "cavern_jackals", 12);
        if (duel.code == 1) break;
    }
    SIM_CHECK_EQ(duel.code, 1);
    SIM_CHECK(find_group(w.wild, "cavern_jackals")->player_led);
    // Lead a raid: loot or not, it is a crime and it is infamy.
    const int infamy = w.wild.infamy;
    const WildActionResult raid = lead_raid(w, "caravans", 2);
    SIM_CHECK(raid.code == -4 || raid.code >= 0);  // no caravan in reach is a clean refusal
    const WildActionResult fields = lead_raid(w, "fields", 2);
    SIM_CHECK(fields.code == 0 || fields.code == 1);
    SIM_CHECK(w.wild.infamy > infamy);
    SIM_CHECK(has_open_crime(w, "theft") || has_open_crime(w, "murder") ||
              !w.justice.verdicts.empty());
    SIM_CHECK_EQ(leave_group(w).code, 0);
    SIM_CHECK(w.wild.player_group.empty());
    SIM_CHECK(!find_group(w.wild, "cavern_jackals")->player_led);
    return true;
}

static bool test_found_a_band_and_bribe_and_ally() {
    WorldState w = fresh();
    w.advance_days(1);
    w.wild.player_place = "shrine_ruins_place";
    SIM_CHECK_EQ(found_band(w, "shrine_ruins_place", 5).code, 0);
    const Group* band = find_group(w.wild, wild::kPlayerBand);
    SIM_CHECK(band && band->active && band->player_led && band->strength == 6);
    SIM_CHECK_EQ(found_band(w, "shrine_ruins_place", 5).code, -4);
    const WildActionResult raid = lead_raid(w, "herds", 3);
    SIM_CHECK(raid.code == 0 || raid.code == 1);
    SIM_CHECK_EQ(leave_group(w).code, 0);  // the band breaks up without him
    SIM_CHECK(!find_group(w.wild, wild::kPlayerBand)->active);

    // Bribe the jackals: silver buys a month of peace.
    const Id camp = w.wild.groups["cavern_jackals"].camp;
    w.wild.player_place = camp;
    SIM_CHECK_EQ(pay_off_group(w, "cavern_jackals").code, -5);  // no silver yet
    credit_purse(w.property, "player", 500);
    SIM_CHECK_EQ(pay_off_group(w, "cavern_jackals").code, 0);
    SIM_CHECK(find_group(w.wild, "cavern_jackals")->paid_until >= w.day + kTributeDays - 1);
    SIM_CHECK_EQ(ally_with_group(w, "cavern_jackals").code, 0);  // paid friends can be sworn
    SIM_CHECK(find_group(w.wild, "cavern_jackals")->allied);
    return true;
}

static bool test_threatened_camps_move() {
    WorldState w = fresh();
    w.advance_days(1);
    Group& g = w.wild.groups["cavern_jackals"];
    const Id first = g.camp;
    w.wild.camps[first].scouted = true;
    g.grudge_vs_player = 90;
    w.advance_days(12);
    const Group* after = find_group(w.wild, "cavern_jackals");
    SIM_CHECK(after->active && after->camp != first);
    SIM_CHECK_EQ(find_camp(w.wild, first)->status, std::string("abandoned"));
    SIM_CHECK(count_events(w, "wild_camp_moved") >= 1);
    return true;
}

// --- caravans ---------------------------------------------------------------------------

static bool test_caravans_run_their_routes() {
    WorldState w = fresh(3);
    const std::int64_t tin0 = w.economy.stock_by_city_item["city_of_the_moon/tin"];
    w.advance_days(30);
    SIM_CHECK(w.wild.caravans_arrived + w.wild.caravans_robbed >= 3);
    const std::int64_t tin = w.economy.stock_by_city_item["city_of_the_moon/tin"];
    SIM_CHECK(tin >= tin0);  // imports land on the market when they arrive
    SIM_CHECK(w.wild.caravans_arrived >= 1);
    // Escort one: meet it where it stands and ride with it to the gate.
    for (int k = 0; k < 40 && w.wild.escorting.empty(); ++k) {
        for (const CaravanRun& run : w.wild.caravans) {
            const CaravanDef* cd = wild::caravan_def(w, run.def);
            w.wild.player_place = cd->route[static_cast<std::size_t>(run.leg)];
            if (escort_caravan(w, run.id, 4).code == 0) break;
        }
        if (w.wild.escorting.empty()) w.advance_days(1);
    }
    SIM_CHECK(!w.wild.escorting.empty());
    SIM_CHECK_EQ(travel(w, "date_grove_place", "foot", 8).code, -7);
    const Silver before = purse(w.property, "player");
    w.advance_days(4);
    SIM_CHECK(w.wild.escorting.empty());
    SIM_CHECK(purse(w.property, "player") > before || w.wild.player_wounds > 0);
    // Rob one.
    WildActionResult robbed{-2, ""};
    for (int k = 0; k < 40 && robbed.code != 1; ++k) {
        for (const CaravanRun& run : w.wild.caravans) {
            const CaravanDef* cd = wild::caravan_def(w, run.def);
            w.wild.player_place = cd->route[static_cast<std::size_t>(run.leg)];
            robbed = rob_caravan(w, run.id, 40, 3);
            if (robbed.code == 1) break;
        }
        if (robbed.code != 1) w.advance_days(1);
    }
    SIM_CHECK_EQ(robbed.code, 1);
    SIM_CHECK(w.wild.infamy >= 1);
    SIM_CHECK(has_open_crime(w, "theft") || !w.justice.verdicts.empty());
    return true;
}

// --- sites ------------------------------------------------------------------------------

static bool test_tomb_robbing_is_a_crime_and_tombs_do_not_refill() {
    WorldState w = fresh(8);
    w.advance_days(1);
    w.wild.player_place = "old_tombs_place";
    int taken = 0;
    for (int k = 0; k < 20; ++k) {
        w.wild.player_wounds = 0;
        const WildActionResult r = search_site(w, "old_tombs_place", 2);
        SIM_CHECK(r.code == 0 || r.code == 1);
        if (r.code == 1) ++taken;
    }
    SIM_CHECK(taken >= 1);
    bool filed = false;
    for (const Crime& c : w.justice.open_crimes) filed = filed || c.law_row == "tomb_robbery";
    for (const Hearing& h : w.justice.verdicts) filed = filed || !h.crime_id.empty();
    SIM_CHECK(filed);
    const SiteState& site = w.wild.sites.at("old_tombs_place");
    int left = 0;
    for (const auto& [item, units] : site.loot_left) left += units;
    SIM_CHECK_EQ(left, 0);
    w.advance_days(200);
    left = 0;
    for (const auto& [item, units] : w.wild.sites.at("old_tombs_place").loot_left) left += units;
    SIM_CHECK_EQ(left, 0);  // a robbed tomb stays robbed
    SIM_CHECK_EQ(search_site(w, "moon_gate_place", 2).code, -2);
    return true;
}

// --- weather ----------------------------------------------------------------------------

static bool test_weather_follows_the_seasons() {
    WorldState w = fresh(4);
    std::map<Id, int> seen;
    for (DayNumber d = 1; d <= 360; ++d) {
        const Id wx = weather_on(w, d);
        ++seen[wx];
        const std::string& season = w.cal.season_id(d);
        if (wx == "rain") SIM_CHECK(season == "rains" || season == "vintage");
        SIM_CHECK_EQ(wx, weather_on(w, d));  // a pure function of the day
    }
    SIM_CHECK(seen.size() >= 4);
    w.facts.drought_stage = 5;
    int rain = 0, scorch = 0;
    for (DayNumber d = 1; d <= 360; ++d) {
        rain += weather_on(w, d) == "rain";
        scorch += weather_on(w, d) == "scorching";
    }
    SIM_CHECK(rain < seen["rain"] || seen["rain"] == 0);
    SIM_CHECK(scorch > seen["scorching"]);
    return true;
}

// --- the combat seam --------------------------------------------------------------------

static SkirmishOutcome always_defender(const SkirmishRequest&, Rng&) {
    SkirmishOutcome o;
    o.attacker_won = false;
    o.attacker_losses = 1;
    o.note = "hooked";
    return o;
}

static bool test_skirmish_stub_and_hook() {
    Rng r(99);
    int big_wins = 0;
    for (int k = 0; k < 200; ++k) {
        SkirmishRequest req;
        req.attacker = SkirmishSide{"a", 10, 100, 60, false};
        req.defender = SkirmishSide{"d", 3, 100, 60, false};
        const SkirmishOutcome o = stub_skirmish(req, r);
        big_wins += o.attacker_won;
        SIM_CHECK(o.attacker_losses >= 0 && o.attacker_losses <= 10);
        SIM_CHECK(o.defender_losses >= 0 && o.defender_losses <= 3);
    }
    SIM_CHECK(big_wins == 200);
    // The W4-B seam: a resolver set on the state is the one used.
    WorldState w = fresh();
    w.advance_days(1);
    w.wild.skirmish = &always_defender;
    const Id camp = w.wild.groups["cavern_jackals"].camp;
    w.wild.player_place = camp;
    SIM_CHECK_EQ(attack_camp(w, camp, 500, 12).code, 0);  // the hook said no
    return true;
}

// --- save / load / determinism -------------------------------------------------------------

static bool test_snapshot_round_trips_the_wild() {
    WorldState a = fresh(31);
    a.facts.drought_stage = 3;
    a.facts.war_stage = 2;
    a.advance_days(150);
    a.wild.player_place = a.wild.groups["cavern_jackals"].active ? a.wild.groups["cavern_jackals"].camp
                                                                 : "moon_gate_place";
    credit_purse(a.property, "player", 300);
    (void)pay_off_group(a, "cavern_jackals");
    (void)search_site(a, "old_tombs_place", 3);
    const std::string s1 = save_world(a);
    SIM_CHECK(s1.find("\nWILD\t") != std::string::npos);
    WorldState b;
    load_world(b, kCanon, s1);
    SIM_CHECK_EQ(save_world(b), s1);
    a.advance_days(90);
    b.advance_days(90);
    SIM_CHECK_EQ(save_world(a), save_world(b));
    SIM_CHECK(!a.wild.raids.empty());
    // A save from before W4-C (no WILD section) still loads; the fresh wild stands.
    const std::string old = s1.substr(0, s1.find("\nWILD\t") + 1);
    WorldState c;
    load_world(c, kCanon, old);
    SIM_CHECK(c.wild.raids.empty());
    SIM_CHECK_EQ(c.wild.herd_head, kHerdStart);
    SIM_CHECK(!c.wild.nodes.empty());
    // A malformed wild row throws and leaves the target untouched.
    std::string bad = s1;
    const auto pos = bad.find("\nG\t");
    SIM_CHECK(pos != std::string::npos);
    bad.insert(pos + 3, "x\t");
    WorldState d = fresh(1);
    bool threw = false;
    try {
        load_world(d, kCanon, bad);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    SIM_CHECK(threw);
    SIM_CHECK_EQ(d.day, DayNumber{1});
    return true;
}

static bool test_same_seed_same_wild() {
    auto run = [] {
        WorldState w;
        w.init(kCanon, 77);
        w.facts.drought_stage = 2;
        w.facts.war_stage = 2;
        w.advance_days(200);
        return wild_save_rows(w.wild);
    };
    SIM_CHECK(run() == run());
    return true;
}

// D-022: no float in any wild value that affects state or rolls.
static bool test_no_float_in_wild_maths() {
    for (const char* f : {"src/Wild.cpp", "src/WildTick.cpp", "src/WildTravel.cpp", "src/WildPlayer.cpp",
                          "src/WildWorld.cpp", "src/WildSave.cpp", "include/sim/Wild.hpp",
                          "include/sim/WildActions.hpp"}) {
        std::ifstream in(f);
        SIM_CHECK(in.good());
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string src = ss.str();
        for (const char* banned : {"double ", "float ", "double>", "float>", "(double)", "(float)"})
            SIM_CHECK(src.find(banned) == std::string::npos);
        SIM_CHECK(src.find(".unit()") == std::string::npos);
    }
    return true;
}

SIM_MAIN(test_region_data_is_whole, test_groups_caravans_encounters_reference_canon,
         test_day_one_camp_and_herds, test_raid_formula_moves_with_hunger_defence_and_bribes,
         test_a_raid_takes_goods_through_the_economy_hook, test_travel_costs_time_and_refuses_cleanly,
         test_a_journey_takes_hours_and_thirst, test_encounters_respect_terrain_and_log,
         test_clearing_a_camp_pays_and_frees, test_unrescued_captives_are_sold_east,
         test_join_take_over_and_ride_as_a_bandit, test_found_a_band_and_bribe_and_ally,
         test_threatened_camps_move, test_caravans_run_their_routes,
         test_tomb_robbing_is_a_crime_and_tombs_do_not_refill, test_weather_follows_the_seasons,
         test_skirmish_stub_and_hook, test_snapshot_round_trips_the_wild, test_same_seed_same_wild,
         test_no_float_in_wild_maths)
