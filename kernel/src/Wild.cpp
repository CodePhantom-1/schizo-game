// Wild.cpp — W4-C: the wild lands' static data, pure queries, the stub
// skirmish resolver and the parsing helpers the other Wild*.cpp files share.
// See sim/Wild.hpp and kernel/contracts/module_Wild.md.
//
// D-022: integer maths only. Every rng draw is an integer (Rng::int_in).
#include "sim/Wild.hpp"

#include "WildInternal.hpp"
#include "sim/Db.hpp"

#include <algorithm>
#include <cstdlib>

namespace sim {

namespace wild {

std::uint64_t salt_of(const std::string& s) {
    std::uint64_t h = 1469598103934665603ull;  // FNV-1a 64
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

Rng stream(const WorldState& w, DayNumber day, const std::string& what) {
    return w.rng.fork(static_cast<std::uint64_t>(day) * 0x9E3779B1ull ^ salt_of("wild:" + what));
}

Rng action_stream(WorldState& w, const std::string& what) {
    ++w.wild.action_serial;
    return w.rng.fork(static_cast<std::uint64_t>(w.day) * 0x9E3779B1ull ^
                      salt_of("wild-act:" + what + ":" + std::to_string(w.wild.action_serial)));
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else if (!(cur.empty() && c == ' ')) {
            cur += c;
        }
    }
    while (!cur.empty() && cur.back() == ' ') cur.pop_back();
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int to_int(const std::string& s, int fallback) {
    if (s.empty()) return fallback;
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str()) return fallback;
    return static_cast<int>(std::clamp<long>(v, -1000000000L, 1000000000L));
}

std::vector<std::pair<Id, int>> parse_pairs(const std::string& s) {
    std::vector<std::pair<Id, int>> out;
    for (const std::string& part : split(s, ';')) {
        const auto colon = part.find(':');
        if (colon == std::string::npos) {
            out.emplace_back(part, 1);
        } else {
            out.emplace_back(part.substr(0, colon), to_int(part.substr(colon + 1), 0));
        }
    }
    return out;
}

}  // namespace wild

using namespace wild;

// --- the stub skirmish resolver (the W4-B seam) --------------------------------

SkirmishOutcome stub_skirmish(const SkirmishRequest& req, Rng& rng) {
    auto power = [](const SkirmishSide& s) -> std::int64_t {
        const std::int64_t f = std::max(0, s.fighters);
        const std::int64_t p = std::clamp(s.prowess_pct, 1, 1000);
        const std::int64_t m = 50 + std::clamp(s.morale, 0, 100);
        return f * p * m;
    };
    const std::int64_t pa = power(req.attacker) * rng.int_in(75, 125) / 100;
    const std::int64_t pd = power(req.defender) * rng.int_in(75, 125) / 100;
    SkirmishOutcome out;
    out.attacker_won = pa > pd;  // ties go to the defender
    const SkirmishSide& win = out.attacker_won ? req.attacker : req.defender;
    const SkirmishSide& lose = out.attacker_won ? req.defender : req.attacker;
    const std::int64_t pw = std::max<std::int64_t>(1, out.attacker_won ? pa : pd);
    const std::int64_t pl = out.attacker_won ? pd : pa;
    int loser_losses = std::max(1, lose.fighters * rng.int_in(30, 60) / 100);
    loser_losses = std::min(loser_losses, std::max(0, lose.fighters));
    const std::int64_t closeness = std::min<std::int64_t>(100, pl * 100 / pw);  // 0..100
    int winner_losses = static_cast<int>(static_cast<std::int64_t>(win.fighters) *
                                         rng.int_in(0, 25) * closeness / 10000);
    winner_losses = std::clamp(winner_losses, 0, std::max(0, win.fighters - 1));
    (out.attacker_won ? out.defender_losses : out.attacker_losses) = loser_losses;
    (out.attacker_won ? out.attacker_losses : out.defender_losses) = winner_losses;
    const bool player_lost = (req.attacker.has_player && !out.attacker_won) ||
                             (req.defender.has_player && out.attacker_won);
    const bool player_in = req.attacker.has_player || req.defender.has_player;
    out.player_wounded = player_in && (player_lost || rng.int_in(0, 9999) < 1500);
    out.note = "stub";
    return out;
}

// --- static data -----------------------------------------------------------------

namespace {

bool is_true(const std::string& s) { return s == "true" || s == "1" || s == "yes"; }

void add_node_from_canon(const Db& db, WildState& s, const Id& id) {
    if (s.nodes.count(id)) return;
    if (auto p = db.find("places", id); p && p->get("tag") != "OPEN") {
        WildNode n;
        n.id = id;
        n.name = p->get("name");
        n.kind = p->get("kind");
        n.source = "place";
        n.danger = 1;
        s.nodes[id] = n;
        return;
    }
    if (auto c = db.find("cities", id); c && c->get("tag") != "OPEN") {
        WildNode n;
        n.id = id;
        n.name = c->get("name");
        n.kind = "city";
        n.source = "city";
        n.danger = 0;
        s.nodes[id] = n;
    }
}

}  // namespace

void init_wild(const Db& db, WildState& s) {
    s = WildState{};
    for (const Row& r : db.rows("wild_places")) {
        if (r.get("tag") == "OPEN") continue;
        WildNode n;
        n.id = r.at("id");
        n.name = r.get("name");
        n.kind = r.get("kind");
        n.source = "wild";
        n.x_m = to_int(r.get("x_m"));
        n.y_m = to_int(r.get("y_m"));
        n.danger = std::clamp(to_int(r.get("danger")), 0, 10);
        n.camp_site = is_true(r.get("camp_site"));
        n.edge_to = r.get("edge_to");
        n.loot = parse_pairs(r.get("loot"));
        n.risks = parse_pairs(r.get("risks"));
        n.restock_days = std::max(0, to_int(r.get("restock_days")));
        s.nodes[n.id] = n;
    }
    for (const Row& r : db.rows("wild_links")) {
        if (r.get("tag") == "OPEN") continue;
        WildLink l;
        l.id = r.at("id");
        l.a = r.get("from");
        l.b = r.get("to");
        l.kind = r.get("kind");
        l.distance_m = std::max(1, to_int(r.get("distance_m"), 1));
        l.danger = std::clamp(to_int(r.get("danger")), 0, 10);
        add_node_from_canon(db, s, l.a);
        add_node_from_canon(db, s, l.b);
        if (!s.nodes.count(l.a) || !s.nodes.count(l.b)) continue;  // dangling: never a road to nowhere
        s.links.push_back(l);
    }
    for (const Row& r : db.rows("transport_modes")) {
        if (r.get("tag") == "OPEN") continue;
        TransportMode m;
        m.id = r.at("id");
        m.name = r.get("name");
        m.speed_m_per_h = std::max(1, to_int(r.get("speed_m_per_h"), 4000));
        m.requires_item = r.get("requires_item");
        m.link_kinds = split(r.get("link_kinds"), ';');
        s.modes[m.id] = m;
    }
    for (const Row& r : db.rows("wild_encounters")) {
        if (r.get("tag") == "OPEN") continue;
        EncounterDef e;
        e.id = r.at("id");
        e.name = r.get("name");
        e.kind = r.get("kind");
        e.hostile = is_true(r.get("hostile"));
        e.weight = std::max(0, to_int(r.get("weight")));
        e.danger_min = to_int(r.get("danger_min"));
        e.terrains = split(r.get("terrains"), ';');
        e.hours = r.get("hours", "any");
        e.seasons = split(r.get("seasons"), ';');
        e.drought_pct = to_int(r.get("drought_pct"));
        e.war_pct = to_int(r.get("war_pct"));
        e.foes_min = std::max(1, to_int(r.get("foes_min"), 1));
        e.foes_max = std::max(e.foes_min, to_int(r.get("foes_max"), e.foes_min));
        e.prowess_pct = std::max(1, to_int(r.get("prowess_pct"), 100));
        s.encounter_defs.push_back(e);
    }
    for (const Row& r : db.rows("wild_groups")) {
        if (r.get("tag") == "OPEN") continue;
        GroupDef g;
        g.id = r.at("id");
        g.name = r.get("name");
        g.kind = r.get("kind");
        g.faction = r.get("faction");
        g.leaders = split(r.get("leaders"), ';');
        g.camp_sites = split(r.get("camp_sites"), ';');
        g.territory = split(r.get("territory"), ';');
        g.forms_when = split(r.get("forms_when"), ';');
        g.form_days = std::max(0, to_int(r.get("form_days")));
        g.repop_days = std::max(1, to_int(r.get("repop_days"), 60));
        g.base_strength = std::max(1, to_int(r.get("base_strength"), 4));
        g.max_strength = std::max(g.base_strength, to_int(r.get("max_strength"), 12));
        g.start_supplies = std::clamp(to_int(r.get("start_supplies"), 50), 0, 100);
        g.start_morale = std::clamp(to_int(r.get("start_morale"), 50), 0, 100);
        g.prowess_pct = std::max(1, to_int(r.get("prowess_pct"), 100));
        for (const auto& [t, wgt] : parse_pairs(r.get("prefers"))) g.prefers[t] = std::max(0, wgt);
        g.grudges = split(r.get("grudges"), ';');
        if (g.leaders.empty()) g.leaders.push_back("a nameless chief");
        if (g.camp_sites.empty()) continue;  // a group needs somewhere to live
        s.group_defs.push_back(g);
    }
    for (const Row& r : db.rows("caravans")) {
        if (r.get("tag") == "OPEN") continue;
        CaravanDef c;
        c.id = r.at("id");
        c.name = r.get("name");
        c.faction = r.get("faction");
        c.route = split(r.get("route"), ';');
        c.goods = parse_pairs(r.get("goods"));
        c.every_days = std::max(1, to_int(r.get("every_days"), 10));
        c.offset_days = std::max(0, to_int(r.get("offset_days")));
        c.guards = std::max(0, to_int(r.get("guards"), 3));
        if (c.route.size() < 2) continue;
        s.caravan_defs.push_back(c);
    }
    for (const Row& r : db.rows("weather")) {
        if (r.get("tag") == "OPEN") continue;
        WeatherDef d;
        d.id = r.at("id");
        d.name = r.get("name");
        for (const auto& [season, wgt] : parse_pairs(r.get("season_weights")))
            d.season_weights[season] = std::max(0, wgt);
        d.drought_weight = to_int(r.get("drought_weight"));
        d.travel_pct = std::max(1, to_int(r.get("travel_pct"), 100));
        d.desert_travel_pct = std::max(1, to_int(r.get("desert_travel_pct"), 100));
        d.thirst_per_hour = std::max(0, to_int(r.get("thirst_per_hour")));
        d.encounter_bp = to_int(r.get("encounter_bp"));
        d.raid_opportunity = to_int(r.get("raid_opportunity"));
        d.raids = r.get("raids", "true") != "false";
        s.weather_defs.push_back(d);
    }
    // W5-B: sworn treaties (treaties.csv). A row needs two parties to bind
    // anyone; the kernel-read term is the "no_raids" token in `terms`.
    for (const Row& r : db.rows("treaties")) {
        if (r.get("tag") == "OPEN") continue;
        TreatyDef t;
        t.id = r.at("id");
        t.name = r.get("name");
        t.parties = split(r.get("parties"), ';');
        t.terms = r.get("terms");
        if (t.parties.size() < 2) continue;  // a treaty binds no one alone
        s.treaty_defs.push_back(t);
    }

    // --- dynamic seed ---
    s.herd_head = kHerdStart;
    for (const auto& [id, n] : s.nodes)
        if (!n.loot.empty()) {
            SiteState st;
            for (const auto& [item, units] : n.loot) st.loot_left[item] = units;
            s.sites[id] = st;
        }
    for (const GroupDef& def : s.group_defs) {
        Group g;
        g.id = def.id;
        s.groups[def.id] = g;
    }
}

// --- queries -----------------------------------------------------------------------

const WildNode* find_node(const WildState& s, const Id& id) {
    const auto it = s.nodes.find(id);
    return it == s.nodes.end() ? nullptr : &it->second;
}

const GroupDef* find_group_def(const WildState& s, const Id& id) {
    for (const GroupDef& g : s.group_defs)
        if (g.id == id) return &g;
    return nullptr;
}

const Group* find_group(const WildState& s, const Id& id) {
    const auto it = s.groups.find(id);
    return it == s.groups.end() ? nullptr : &it->second;
}

const Camp* find_camp(const WildState& s, const Id& place) {
    const auto it = s.camps.find(place);
    return it == s.camps.end() ? nullptr : &it->second;
}

std::vector<const WildLink*> links_of(const WildState& s, const Id& node) {
    std::vector<const WildLink*> out;
    for (const WildLink& l : s.links)
        if (l.a == node || l.b == node) out.push_back(&l);
    return out;
}

Id other_end(const WildLink& l, const Id& node) {
    if (l.a == node) return l.b;
    if (l.b == node) return l.a;
    return "";
}

bool in_territory(const GroupDef& g, const Id& place) {
    return std::find(g.territory.begin(), g.territory.end(), place) != g.territory.end() ||
           std::find(g.camp_sites.begin(), g.camp_sites.end(), place) != g.camp_sites.end();
}

const TreatyDef* live_treaty_between(const WorldState& w, const Id& a, const Id& b) {
    // "Live" (W5-B reading, ledgered): both factions named in `parties`, the
    // sworn "no_raids" term among the terms, and neither side outlawed — an
    // outlawed party's law is broken, so its sworn peace is void.
    if (a.empty() || b.empty() || a == b) return nullptr;
    for (const TreatyDef& t : w.wild.treaty_defs) {
        bool has_a = false, has_b = false;
        for (const Id& p : t.parties) {
            has_a = has_a || p == a;
            has_b = has_b || p == b;
        }
        if (!has_a || !has_b) continue;
        bool no_raids = false;
        for (const std::string& term : split(t.terms, ';'))
            no_raids = no_raids || term == "no_raids";
        if (!no_raids) continue;
        if (w.faction.outlawed_factions.count(a) > 0 ||
            w.faction.outlawed_factions.count(b) > 0)
            continue;  // a declared-outlaw party voids the pact (VERIFY-A B7.11:
                       // the player's own exile must NOT void factions' treaties)
        return &t;
    }
    return nullptr;
}

std::string npc_fate(const WildState& s, const Id& npc) {
    const auto it = s.npc_fate.find(npc);
    return it == s.npc_fate.end() ? std::string() : it->second;
}

bool npc_out_of_play(const WildState& s, const Id& npc) { return !npc_fate(s, npc).empty(); }

int leg_minutes(const WildState& s, const WildLink& l, const TransportMode& m, const Id& weather) {
    int pct = 100;
    for (const WeatherDef& d : s.weather_defs)
        if (d.id == weather) {
            const WildNode* a = find_node(s, l.a);
            const WildNode* b = find_node(s, l.b);
            const bool desert = l.kind == "desert_track" || (a && a->kind == "desert_edge") ||
                                (b && b->kind == "desert_edge");
            pct = desert ? d.desert_travel_pct : d.travel_pct;
        }
    const std::int64_t minutes = static_cast<std::int64_t>(l.distance_m) * 60 * pct /
                                 (static_cast<std::int64_t>(m.speed_m_per_h) * 100);
    return static_cast<int>(std::clamp<std::int64_t>(minutes, 1, 1000000));
}

bool conditions_hold(const std::vector<std::string>& when, const WorldFacts& facts) {
    for (const std::string& c : when) {
        if (c == "start") continue;
        const auto colon = c.find(':');
        const std::string kind = c.substr(0, colon);
        const int arg = colon == std::string::npos ? 0 : to_int(c.substr(colon + 1));
        if (kind == "drought_gte" && facts.drought_stage < arg) return false;
        if (kind == "war_gte" && facts.war_stage < arg) return false;
        if (kind != "drought_gte" && kind != "war_gte") return false;  // unknown: never holds
    }
    return true;
}

}  // namespace sim
