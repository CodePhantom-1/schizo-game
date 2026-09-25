#pragma once
// Faction.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// Standing (0..100 per faction), sworn oaths with the oath-breaker curse
// (rpg-systems §4: Stranger → Oath-bound tiers; one Great King at a time is
// enforced here as "one oath to a major faction at a time").
//
// Invariants:
//  - standing clamped to [0, 100] on every write
//  - breaking an oath marks it broken, drops standing hard, and flags the
//    Oath-breaker curse in the state (the curse's bite is the magic system's
//    problem — Magic reads it via WorldContext)
//  - deterministic; writes ONLY FactionState
#include "sim/Types.hpp"

#include <map>
#include <vector>


namespace sim {

struct WorldContext;  // defined in sim/Context.hpp (the seam — never included by module headers)

struct Oath {
    Id id;             // "oath_<n>" in sequence
    Id swearer;        // npc or the player ("player")
    Id to_faction;     // factions.csv id
    DayNumber day = 0;
    bool broken = false;
};

struct FactionState {
    std::map<Id, int> standing_by_faction;  // factions.csv id -> 0..100
    std::vector<Oath> oaths;
    bool oath_breaker_curse = false;        // set when any oath is broken
    // W2-A (kernel/src/Actions.cpp): mechanics.md row 12 "outlawry -> rank
    // 0". The kernel carries no numeric rank per faction, so an exile/death
    // verdict is recorded here instead — rank 0 (D-015's "the Outsider") is
    // exactly "stateless outside any city's protection", i.e. zero standing
    // and marked outlawed with that jurisdiction.
    std::map<Id, bool> outlawed_by_faction;
};

void tick_faction(const WorldContext& ctx, FactionState& state, int days = 1);

int standing(const FactionState& state, const Id& faction_id);  // 0 if unknown
void add_standing(FactionState& state, const Id& faction_id, int delta);  // clamped

// Seating tiers (rpg-systems §4.2): 0-19 Stranger, 20-49 Known, 50-74 Trusted,
// 75-89 Sworn, 90-100 Oath-bound.
std::string tier_of(int standing);

// Swears an oath; refuses (returns nullptr) if the swearer already holds an
// unbroken oath to a major faction (the one-Great-King rule).
Oath* swear(FactionState& state, const Id& swearer, const Id& to_faction, DayNumber day);
// Breaks the oath: marks it, drops standing with that faction by 40, sets the curse.
void break_oath(FactionState& state, const Id& oath_id);

// W2-A: forces standing with the faction to 0 and marks it outlawed (rank 0,
// mechanics.md row 12). Used by Actions.cpp on an exile/death verdict.
void outlaw(FactionState& state, const Id& faction_id);
bool is_outlawed(const FactionState& state, const Id& faction_id);

}  // namespace sim
