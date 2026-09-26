// CApiItems.h — P0a: the C ABI over the player's hands (mechanics registry
// FND-01…06, FND-09): the refusal-reason channel, item stacks and item
// definitions, carrying weight, goods in the world and in containers, timed
// actions by the minute, and the player notice feed. Included by sim/CApi.h.
//
// Conventions (as CApiVerbs.h): no C++ exception crosses the boundary; string
// outputs write at most cap-1 bytes plus NUL and return the untruncated
// length; -1 on a null argument. Refusal codes and their string-table keys
// are fixed in kernel/contracts/verb_results.md: -1 refuse.bad_argument,
// -2 refuse.unknown, -3 refuse.bound, -4 refuse.too_heavy, -5 refuse.full,
// -6 refuse.locked. Every verb here sets the last reason: its key on a
// refusal, "" on success.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;

// FND-05: the string-table key of why the last verb of this header refused
// ("" after a success or before any verb). Buffer convention.
int sim_world_last_reason(const SimWorld* world, char* out, int cap);

// FND-03: carrying. Grams the actor can carry unburdened (30 kg + 3 kg per
// Strength), grams carried (stacks plus the purse's silver), and the tier:
// 0 normal (<= 100%), 1 burdened (<= 125%), 2 pinned (> 125%). -1 on null.
int64_t sim_world_capacity_g(const SimWorld* world, const char* actor);
int64_t sim_world_carried_g(const SimWorld* world, const char* actor);
int sim_world_encumbrance(const SimWorld* world, const char* actor);

#ifdef __cplusplus
}  // extern "C"
#endif
