// CApiQuests.h — A7: quests, the journal and dialogue through the C ABI.
// Included by sim/CApi.h. Same conventions as CApiVerbs.h: no exception
// crosses the boundary; -1 on a null argument; string outputs write at most
// cap-1 bytes plus NUL and return the untruncated length (a null/0 buffer
// just returns the length). Lists are index-based: *_count, then *_at with
// an index in 0..count-1 (-1 outside it).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;

// Quests -----------------------------------------------------------------------
// `list`: "available" (never accepted, completed or failed) | "active" |
// "completed" | "failed". -1 on an unknown list.
int sim_world_quest_count(const SimWorld* world, const char* list);
int sim_world_quest_at(const SimWorld* world, const char* list, int index, char* out, int cap);
// quests.csv's name / giver for a quest id; -1 on an unknown quest.
int sim_world_quest_title(const SimWorld* world, const char* quest, char* out, int cap);
int sim_world_quest_giver(const SimWorld* world, const char* quest, char* out, int cap);
// The active quest's stage label; -1 when the quest is not active.
int sim_world_quest_stage(const SimWorld* world, const char* quest, char* out, int cap);
// The active quest's deadline day (0 = none); -1 when not active.
int64_t sim_world_quest_deadline(const SimWorld* world, const char* quest);
// 0 accepted; -2 unknown quest; -3 already active, completed or failed.
int sim_world_quest_accept(SimWorld* world, const char* quest);
// Sets an active quest's stage (journals it with `text`): 1 changed, 0 same
// stage, -3 not active.
int sim_world_quest_advance(SimWorld* world, const char* quest, const char* stage, const char* text);
// Completes an active quest and pays its rewards (Actions.hpp complete_quest):
// 0 done, -3 not active.
int sim_world_quest_complete(SimWorld* world, const char* quest);
// The player gives the quest up: it fails and is journaled "abandoned".
// 0 done, -3 not active.
int sim_world_quest_abandon(SimWorld* world, const char* quest);

// The journal (append-only, survives completion/failure) ----------------------
int sim_world_journal_count(const SimWorld* world, const char* quest);
// Entry `index`: its day, stage and text; returns the text's length, -1 on a
// bad index. `day`, `stage` and `text` may each be null.
int sim_world_journal_at(const SimWorld* world, const char* quest, int index, int64_t* day,
                         char* stage, int stage_cap, char* text, int text_cap);

#ifdef __cplusplus
}  // extern "C"
#endif
