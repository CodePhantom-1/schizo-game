// Population.cpp — named NPCs, memory, and the rumour graph (living-world §2:
// memory spreads along social links at walking speed — one hop per tick).
//
// Rumour model, from the frozen header contract (sim/Population.hpp):
//   - witness() records a fact into an NPC's memory with its day.
//   - At the tick of day D, every memory entry learned on day D - 1 ("heard
//     yesterday") is retold once along the teller's `knows` edges to every
//     listener who does not already hold that exact (subject, fact). A fact
//     heard today is therefore retold tomorrow, moves exactly one hop per
//     tick, and dies out once its whole connected component knows it.
//   - "with the teller as source": the listener's entry exists because the
//     teller retold it — the knows edge is the provenance. MemoryEntry (a
//     frozen contract struct) carries no source field, and hiding the teller
//     inside the fact string would defeat (subject, fact) dedup and make the
//     graph retell forever, so the fact text travels unchanged.
//
// "NPCs age their day": Npc (frozen contract) has no age or birth fields, so
// aging has nothing mechanical to advance at Wave 1; the tick's content is
// the rumour hop.
//
// Determinism: no wall clock, no static state, no threads — and no randomness
// at all (the spread is a pure function of state + day), so ctx.rng is left
// untouched for the modules that need it.
#include "sim/Population.hpp"

#include "sim/Context.hpp"  // the seam: module implementation reads ctx members here

#include <algorithm>
#include <cctype>
#include <map>
#include <utility>
#include <vector>

namespace sim {
namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string ascii_lower_trim(const std::string& s) {
    std::string t = trim(s);
    std::transform(t.begin(), t.end(), t.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return t;
}

// A resident's `role` column names a schedules.csv role only when it matches
// one case-insensitively; a named leader's narrative title never does, so
// this is a safe, additive probe (D-011 light residents; INVENTED matching
// rule — exact, case-insensitive — content authors write roles that already
// mirror schedules.csv, per the Schedule module contract's content-gap note).
std::string matching_schedule_role(const WorldContext& ctx, const std::string& raw_role) {
    const std::string wanted = ascii_lower_trim(raw_role);
    if (wanted.empty()) return {};
    for (const std::string& role : schedule_roles(ctx.db))
        if (ascii_lower_trim(role) == wanted) return role;
    return {};
}

// knows[a] gains b and knows[b] gains a, unless already present or a == b.
void add_knows_edge(PopulationState& state, const Id& a, const Id& b) {
    if (a == b) return;
    auto& edges_a = state.knows[a];
    if (std::find(edges_a.begin(), edges_a.end(), b) == edges_a.end()) edges_a.push_back(b);
    auto& edges_b = state.knows[b];
    if (std::find(edges_b.begin(), edges_b.end(), a) == edges_b.end()) edges_b.push_back(a);
}

// Social links for the light residents (D-011 deferred item), so rumours
// travel the street. INVENTED deterministic rule, in three parts, applied
// only to npcs with a non-empty `role` (the schedule-matching residents —
// named leaders are excluded, keeping Wave 1's empty knows-graph unchanged):
//   1. household: any two residents who share a non-empty `household` id
//      know each other. people.csv carries no household column at Wave 1
//      (Population.hpp: "household… stay empty rather than invented"), so
//      this rule is presently inert — it activates the day content adds one,
//      with no code change needed here.
//   2. same-role: residents of the same role in the same city know each
//      other (the bakers all know the bakers).
//   3. neighbours: residents of the same city, sorted by id for a stable
//      street order, are linked to their immediate predecessor — a deterministic
//      stand-in for "lives on the same street" until a real street/plot layout
//      exists.
// Idempotent: add_knows_edge only appends a missing edge, so re-seeding
// never duplicates or removes a link.
void wire_resident_links(PopulationState& state) {
    std::vector<const Npc*> residents;
    for (const Npc& n : state.npcs)
        if (!n.role.empty()) residents.push_back(&n);

    std::map<Id, std::vector<Id>> by_household;
    std::map<std::pair<Id, std::string>, std::vector<Id>> by_city_role;
    std::map<Id, std::vector<Id>> by_city;  // for the neighbour chain

    for (const Npc* n : residents) {
        if (!n->household.empty()) by_household[n->household].push_back(n->id);
        by_city_role[{n->home_city, ascii_lower_trim(n->role)}].push_back(n->id);
        by_city[n->home_city].push_back(n->id);
    }

    for (auto& [key, ids] : by_household)
        for (std::size_t i = 0; i < ids.size(); ++i)
            for (std::size_t j = i + 1; j < ids.size(); ++j) add_knows_edge(state, ids[i], ids[j]);

    for (auto& [key, ids] : by_city_role)
        for (std::size_t i = 0; i < ids.size(); ++i)
            for (std::size_t j = i + 1; j < ids.size(); ++j) add_knows_edge(state, ids[i], ids[j]);

    for (auto& [city, ids] : by_city) {
        std::vector<Id> sorted_ids = ids;
        std::sort(sorted_ids.begin(), sorted_ids.end());
        for (std::size_t i = 1; i < sorted_ids.size(); ++i)
            add_knows_edge(state, sorted_ids[i - 1], sorted_ids[i]);
    }
}

// Keeps memory ordered by day (Population.hpp: `memory;  // ordered by day`).
// Stable: an entry lands after every entry with day <= its own, so same-day
// entries keep their encounter order.
void insert_by_day(std::vector<MemoryEntry>& memory, MemoryEntry entry) {
    const auto pos = std::upper_bound(
        memory.begin(), memory.end(), entry.day,
        [](DayNumber day, const MemoryEntry& e) { return day < e.day; });
    memory.insert(pos, std::move(entry));
}

bool already_knows(const Npc& npc, const Id& subject, const std::string& fact) {
    for (const MemoryEntry& e : npc.memory)
        if (e.subject == subject && e.fact == fact) return true;
    return false;
}

// One day of rumour spread, at day `today`: retells every fact learned on
// today - 1. The retell plan is copied out before anything is mutated, so a
// day's spread is a pure function of the morning state and cannot cascade
// within the day (a fact heard today is retold tomorrow, never today).
void spread_one_day(PopulationState& state, DayNumber today) {
    const DayNumber heard_on = today - 1;

    struct Retell {
        Id teller;
        Id subject;
        std::string fact;
    };
    std::vector<Retell> plan;
    for (const Npc& teller : state.npcs)
        for (const MemoryEntry& e : teller.memory)
            if (e.day == heard_on)
                plan.push_back(Retell{teller.id, e.subject, e.fact});

    for (const Retell& r : plan) {
        const auto edges = state.knows.find(r.teller);
        if (edges == state.knows.end()) continue;
        for (const Id& listener_id : edges->second) {
            if (listener_id == r.teller) continue;  // nobody retells to himself
            Npc* listener = find_npc(state, listener_id);
            if (listener == nullptr) continue;      // dangling edge: skip quietly
            if (already_knows(*listener, r.subject, r.fact)) continue;
            insert_by_day(listener->memory, MemoryEntry{today, r.subject, r.fact});
        }
    }
}

}  // namespace

void tick_population(const WorldContext& ctx, PopulationState& state, int days) {
    for (int i = 0; i < days; ++i)  // days <= 0 advances nothing
        spread_one_day(state, ctx.day + static_cast<DayNumber>(i));
}

void seed_people(const WorldContext& ctx, PopulationState& state) {
    for (const Row& row : ctx.db.rows("people")) {
        const std::string& id = row.get("id");
        if (id.empty()) continue;
        if (row.get("tag") == "OPEN") continue;        // canon content that does not exist yet
        if (find_npc(state, id) != nullptr) continue;  // idempotent
        Npc npc;
        npc.id = id;
        npc.name = row.get("name");
        npc.home_city = row.get("city");
        npc.faction_id = row.get("faction");
        npc.role = matching_schedule_role(ctx, row.get("role"));
        // household and patron_deity have no canon source at Wave 1 (people.csv
        // carries no such column); they stay empty rather than invented.
        state.npcs.push_back(std::move(npc));
    }
    wire_resident_links(state);  // idempotent: only ever adds a missing edge
}

Npc* find_npc(PopulationState& state, const Id& npc_id) {
    for (Npc& npc : state.npcs)
        if (npc.id == npc_id) return &npc;
    return nullptr;
}

const Npc* find_npc(const PopulationState& state, const Id& npc_id) {
    for (const Npc& npc : state.npcs)
        if (npc.id == npc_id) return &npc;
    return nullptr;
}

void witness(PopulationState& state, const Id& npc_id, const Id& subject,
             const std::string& fact, DayNumber day) {
    Npc* npc = find_npc(state, npc_id);
    if (npc == nullptr) return;  // unknown witness: nothing to record
    insert_by_day(npc->memory, MemoryEntry{day, subject, fact});
}

std::optional<ScheduledTask> npc_task_at(const Db& db, const Calendar& cal,
                                          const PopulationState& state, const Id& npc_id,
                                          DayNumber day, int hour) {
    const Npc* npc = find_npc(state, npc_id);
    if (npc == nullptr) return std::nullopt;
    // K-2: per-person resolution — role rows + the person's own
    // person_schedules.csv rows, bent by season and festival, places resolved.
    return person_task_at(db, cal, npc->id, npc->role, day, hour);
}

}  // namespace sim
