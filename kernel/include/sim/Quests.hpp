#pragma once
// Quests.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// Quests (living-world §8) — at Wave 1 only the MACHINERY: the history arc,
// background, companion, sourced-side, systemic, faction-contract and emergent
// sources all arrive as content in later phases. The quest state machine must
// exist and must fail quests whose deadline passes ("quests fail and time out;
// the world doesn't wait").
//
// Invariants:
//  - quest definitions live in db/canon/quests.csv (empty now); the machine is
//    data-driven — no quest names in code
//  - deterministic; writes ONLY QuestState
#include "sim/Types.hpp"

#include <map>
#include <string>
#include <vector>


namespace sim {

struct WorldContext;  // defined in sim/Context.hpp (the seam — never included by module headers)

struct QuestDef {
    Id id;               // quests.csv id
    std::string kind;    // history_arc|background|companion|sourced|systemic|faction|emergent
    int deadline_days = 0;  // 0 = no deadline
};

struct Quest {
    Id def_id;
    DayNumber accepted = 0;
    DayNumber deadline = 0;  // 0 = none
    bool failed = false;
    std::string stage;       // free-form stage label ("accepted", "done", …)
};

struct QuestState {
    std::vector<QuestDef> defs;      // loaded from db (empty at Wave 1)
    std::vector<Quest> active;
    std::vector<Id> completed;
    std::vector<Id> failed_list;
};

void load_defs(const WorldContext& ctx, QuestState& state);

// Marks deadlines: a quest past its deadline fails (moved to failed_list).
void tick_quests(const WorldContext& ctx, QuestState& state, int days = 1);

Quest& accept(QuestState& state, const Id& def_id, DayNumber day);
void complete(QuestState& state, const Id& def_id, DayNumber day);

}  // namespace sim
