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
#include <utility>
#include <vector>

namespace sim {
namespace {

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
        // household and patron_deity have no canon source at Wave 1 (people.csv
        // carries no such column); they stay empty rather than invented.
        state.npcs.push_back(std::move(npc));
    }
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

}  // namespace sim
