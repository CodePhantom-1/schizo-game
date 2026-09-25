// WildSave.cpp — W4-C: the dynamic WildState as snapshot rows. Snapshot.cpp
// writes them as one trailing, optional "WILD\t<n>" section (a save written
// before W4-C simply has none, and the fresh init_wild state stands).
//
// Row kinds (first field): S scalars · G group · C camp · V caravan run ·
// R raid · E encounter · F npc fate · T lootable site.
#include "sim/Wild.hpp"

#include <stdexcept>

namespace sim {

namespace {

std::string i64(std::int64_t v) { return std::to_string(v); }
std::string b(bool v) { return v ? "1" : "0"; }

std::string pairs(const std::map<Id, int>& m) {
    std::string out;
    for (const auto& [k, v] : m) {
        if (!out.empty()) out += ';';
        out += k + ":" + std::to_string(v);
    }
    return out;
}

std::string list(const std::vector<Id>& v) {
    std::string out;
    for (const Id& x : v) {
        if (!out.empty()) out += ';';
        out += x;
    }
    return out;
}

std::int64_t num(const std::string& s) {
    try {
        std::size_t pos = 0;
        const long long v = std::stoll(s, &pos);
        if (pos != s.size()) throw std::runtime_error("trailing");
        return v;
    } catch (...) {
        throw std::runtime_error("snapshot: bad wild integer '" + s + "'");
    }
}
int n32(const std::string& s) { return static_cast<int>(num(s)); }

std::vector<Id> unlist(const std::string& s) {
    std::vector<Id> out;
    std::string cur;
    for (char c : s) {
        if (c == ';') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::map<Id, int> unpairs(const std::string& s) {
    std::map<Id, int> out;
    for (const Id& part : unlist(s)) {
        const auto colon = part.rfind(':');
        if (colon == std::string::npos) throw std::runtime_error("snapshot: bad wild pair '" + part + "'");
        out[part.substr(0, colon)] = n32(part.substr(colon + 1));
    }
    return out;
}

void need(const std::vector<std::string>& r, std::size_t n) {
    if (r.size() != n) throw std::runtime_error("snapshot: bad wild row " + (r.empty() ? "" : r[0]));
}

}  // namespace

std::vector<std::vector<std::string>> wild_save_rows(const WildState& s) {
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"S", s.player_place, s.player_group, s.escorting, i64(s.herd_head),
                    i64(s.player_fear), i64(s.player_wounds), i64(s.infamy), i64(s.next_raid),
                    i64(s.next_encounter), i64(s.action_serial), i64(s.caravans_arrived),
                    i64(s.caravans_robbed)});
    for (const auto& [id, g] : s.groups)
        rows.push_back({"G", id, b(g.active), g.camp, g.leader, i64(g.strength), i64(g.peak_strength),
                        i64(g.supplies), i64(g.morale), i64(g.pressure), i64(g.formed_day),
                        i64(g.cleared_day), i64(g.times_formed), i64(g.times_cleared), i64(g.raids),
                        i64(g.grudge_vs_player), i64(g.paid_until), b(g.allied), b(g.player_member),
                        b(g.player_led), b(g.rumoured)});
    for (const auto& [place, c] : s.camps)
        rows.push_back({"C", place, c.group, c.status, i64(c.lookout), i64(c.alarm_days),
                        b(c.known_to_player), b(c.scouted), i64(c.since), pairs(c.loot), list(c.captives)});
    for (const CaravanRun& v : s.caravans)
        rows.push_back({"V", v.id, v.def, i64(v.leg), i64(v.guards), pairs(v.goods), b(v.escorted),
                        i64(v.escort_fighters), i64(v.departed)});
    for (const RaidRecord& r : s.raids)
        rows.push_back({"R", r.id, i64(r.day), r.group, r.target, r.place, i64(r.chance_bp),
                        b(r.success), i64(r.goods_taken), pairs(r.goods), list(r.slain), list(r.taken),
                        r.summary});
    for (const EncounterRecord& e : s.encounters)
        rows.push_back({"E", e.id, i64(e.day), i64(e.hour), e.link, e.encounter, e.kind, e.group,
                        e.stance, e.outcome, i64(e.foes), i64(e.foes_lost), i64(e.allies_lost)});
    for (const auto& [npc, fate] : s.npc_fate) {
        const auto it = s.captive_since.find(npc);
        rows.push_back({"F", npc, fate, i64(it == s.captive_since.end() ? -1 : it->second)});
    }
    for (const auto& [place, t] : s.sites)
        rows.push_back({"T", place, pairs(t.loot_left), i64(t.searches), i64(t.last_search),
                        i64(t.emptied_day)});
    return rows;
}

void wild_load_rows(WildState& s, const std::vector<std::vector<std::string>>& rows) {
    s.groups.clear();
    s.camps.clear();
    s.caravans.clear();
    s.raids.clear();
    s.encounters.clear();
    s.npc_fate.clear();
    s.captive_since.clear();
    s.sites.clear();
    for (const auto& r : rows) {
        if (r.empty()) throw std::runtime_error("snapshot: empty wild row");
        const std::string& k = r[0];
        if (k == "S") {
            need(r, 13);
            s.player_place = r[1];
            s.player_group = r[2];
            s.escorting = r[3];
            s.herd_head = n32(r[4]);
            s.player_fear = n32(r[5]);
            s.player_wounds = n32(r[6]);
            s.infamy = n32(r[7]);
            s.next_raid = n32(r[8]);
            s.next_encounter = n32(r[9]);
            s.action_serial = n32(r[10]);
            s.caravans_arrived = n32(r[11]);
            s.caravans_robbed = n32(r[12]);
        } else if (k == "G") {
            need(r, 21);
            Group g;
            g.id = r[1];
            g.active = r[2] == "1";
            g.camp = r[3];
            g.leader = r[4];
            g.strength = n32(r[5]);
            g.peak_strength = n32(r[6]);
            g.supplies = n32(r[7]);
            g.morale = n32(r[8]);
            g.pressure = n32(r[9]);
            g.formed_day = num(r[10]);
            g.cleared_day = num(r[11]);
            g.times_formed = n32(r[12]);
            g.times_cleared = n32(r[13]);
            g.raids = n32(r[14]);
            g.grudge_vs_player = n32(r[15]);
            g.paid_until = num(r[16]);
            g.allied = r[17] == "1";
            g.player_member = r[18] == "1";
            g.player_led = r[19] == "1";
            g.rumoured = r[20] == "1";
            s.groups[g.id] = g;
        } else if (k == "C") {
            need(r, 11);
            Camp c;
            c.place = r[1];
            c.group = r[2];
            c.status = r[3];
            c.lookout = n32(r[4]);
            c.alarm_days = n32(r[5]);
            c.known_to_player = r[6] == "1";
            c.scouted = r[7] == "1";
            c.since = num(r[8]);
            c.loot = unpairs(r[9]);
            c.captives = unlist(r[10]);
            s.camps[c.place] = c;
        } else if (k == "V") {
            need(r, 9);
            CaravanRun v;
            v.id = r[1];
            v.def = r[2];
            v.leg = n32(r[3]);
            v.guards = n32(r[4]);
            v.goods = unpairs(r[5]);
            v.escorted = r[6] == "1";
            v.escort_fighters = n32(r[7]);
            v.departed = num(r[8]);
            s.caravans.push_back(v);
        } else if (k == "R") {
            need(r, 13);
            RaidRecord x;
            x.id = r[1];
            x.day = num(r[2]);
            x.group = r[3];
            x.target = r[4];
            x.place = r[5];
            x.chance_bp = n32(r[6]);
            x.success = r[7] == "1";
            x.goods_taken = n32(r[8]);
            x.goods = unpairs(r[9]);
            x.slain = unlist(r[10]);
            x.taken = unlist(r[11]);
            x.summary = r[12];
            s.raids.push_back(x);
        } else if (k == "E") {
            need(r, 13);
            EncounterRecord e;
            e.id = r[1];
            e.day = num(r[2]);
            e.hour = n32(r[3]);
            e.link = r[4];
            e.encounter = r[5];
            e.kind = r[6];
            e.group = r[7];
            e.stance = r[8];
            e.outcome = r[9];
            e.foes = n32(r[10]);
            e.foes_lost = n32(r[11]);
            e.allies_lost = n32(r[12]);
            s.encounters.push_back(e);
        } else if (k == "F") {
            need(r, 4);
            s.npc_fate[r[1]] = r[2];
            const std::int64_t since = num(r[3]);
            if (since >= 0) s.captive_since[r[1]] = since;
        } else if (k == "T") {
            need(r, 6);
            SiteState t;
            t.loot_left = unpairs(r[2]);
            t.searches = n32(r[3]);
            t.last_search = num(r[4]);
            t.emptied_day = num(r[5]);
            s.sites[r[1]] = t;
        } else {
            throw std::runtime_error("snapshot: unknown wild row kind " + k);
        }
    }
}

}  // namespace sim
