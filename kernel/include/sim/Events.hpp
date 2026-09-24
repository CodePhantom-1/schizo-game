#pragma once
// Events.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// The city's pulse (city-life §7): events come from triggers, not dice alone —
// a trigger engine over the WorldFacts and the other states, with rules from
// db/canon/events.csv. The table is EMPTY at Wave 1 (content is a later-phase
// storm): the engine must exist, load zero rules, and fire nothing.
//
// Rule shape (events.csv): triggers is a ';'-separated condition list parsed as
//   <kind>:<arg>            kinds: drought_gte, war_gte, season, festival, chance
//   e.g. "drought_gte:2;chance:10"  = drought stage >= 2 and a 1-in-10 daily chance
// Invariants:
//  - an event fires at most once per rule unless the rule's row says repeat
//  - deterministic: chance rolls come from ctx.rng.fork(day + rule salt)
//  - writes ONLY EventsState
#include "sim/Types.hpp"

#include <map>
#include <string>
#include <vector>


namespace sim {

struct WorldContext;  // defined in sim/Context.hpp (the seam — never included by module headers)

struct EventRule {
    Id id;              // events.csv id
    std::string category;
    std::vector<std::string> triggers;  // parsed condition strings
    bool repeatable = false;
};

struct TriggeredEvent {
    Id rule_id;
    DayNumber day = 0;
    std::string summary;
};

struct EventsState {
    std::vector<EventRule> rules;         // loaded once from db
    std::vector<TriggeredEvent> fired;    // ordered by day
    std::map<Id, int> fire_count_by_rule;
};

// Loads rules from db (empty at Wave 1). Idempotent.
void load_rules(const WorldContext& ctx, EventsState& state);

// Evaluates triggers for `days` days; fires matching rules into `fired`.
void tick_events(const WorldContext& ctx, EventsState& state, int days = 1);

}  // namespace sim
