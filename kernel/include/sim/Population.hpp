#pragma once
// Population.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// Named NPCs: identity, home, schedule, memory, and the rumour graph
// (living-world §2: memory entries spread along social links at walking speed —
// one hop per tick).
//
// Invariants:
//  - deterministic; no wall-clock, no global rng outside ctx.rng
//  - writes ONLY PopulationState; reads others via WorldContext (const)
//  - seeds named NPCs from db/canon/people.csv (law_giver, the_prophet,
//    the_warchief at Wave 1); light residents are a later wave
#include "sim/Types.hpp"

#include <map>
#include <vector>


namespace sim {

struct WorldContext;  // defined in sim/Context.hpp (the seam — never included by module headers)

struct MemoryEntry {
    DayNumber day = 0;
    Id subject;          // who/what the fact is about (npc, faction, item…)
    std::string fact;    // what was witnessed or heard
};

struct Npc {
    Id id;
    Id name;
    Id home_city;
    Id household;
    Id faction_id;
    Id patron_deity;
    int loyalty = 50;                    // 0..100
    std::vector<MemoryEntry> memory;     // ordered by day
};

struct PopulationState {
    std::vector<Npc> npcs;
    // knows[npc_id] = npc_ids this npc talks to (the rumour graph edges)
    std::map<Id, std::vector<Id>> knows;
};

// Advances one tick: NPCs age their day; rumours spread one hop along `knows`
// (a fact heard today is retold tomorrow, with the teller as source).
void tick_population(const WorldContext& ctx, PopulationState& state, int days = 1);

// Seeds the named people from db/canon/people.csv (idempotent).
void seed_people(const WorldContext& ctx, PopulationState& state);

Npc* find_npc(PopulationState& state, const Id& npc_id);
const Npc* find_npc(const PopulationState& state, const Id& npc_id);

// A witnessed act enters memory with its day; retold as rumour on later ticks.
void witness(PopulationState& state, const Id& npc_id, const Id& subject,
             const std::string& fact, DayNumber day);

}  // namespace sim
