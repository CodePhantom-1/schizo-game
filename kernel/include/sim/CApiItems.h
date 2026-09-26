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

// INV-03/04: the actor's stacks, in their fixed (normalized) order. A record is
// "item;qty;quality;condition;owner;stolen;made_day;bound" (quality 0 poor ..
// 3 masterwork; condition 0..100; owner "" = the holder's; bools 0/1). The
// index is what the stack verbs below take. count: -1 on null, 0 for an
// unseen actor. at: -2 for a bad index; buffer convention.
int sim_world_stack_count(const SimWorld* world, const char* actor);
int sim_world_stack_at(const SimWorld* world, const char* actor, int index, char* out, int cap);

// FND-02: an item's physical columns as "ui_category;weight_g;stack_max;
// spoil_days;flags" (flags '|'-joined). -2 for an unknown or OPEN item.
int sim_world_item_def(const SimWorld* world, const char* item, char* out, int cap);

// FND-04: goods in the world and in containers. Stack indexes are the actor's
// (sim_world_stack_at) or the container's (sim_world_container_stack_at).
// Witnesses are npc ids ';'-joined (null or "" = nobody saw); taking goods
// whose rightful owner is someone else flags them stolen and, when witnessed,
// files a theft. Each verb returns units moved (>= 0) or a refusal code and
// sets the last reason.
//
// Drops up to qty units at a place and a resting position in cm. id_out
// (optional: null is fine) receives the new world item's id.
int sim_world_drop(SimWorld* world, const char* actor, int stack_index, int qty, const char* place,
                   int x_cm, int y_cm, int z_cm, char* id_out, int cap);
// Picks up to qty units, never past 125% of the actor's carrying capacity.
int sim_world_pick_up(SimWorld* world, const char* actor, const char* world_item, int qty,
                      const char* witnesses_semicolon);
// The world items at a place, in order of creation: count (-1 null), and
// "id;x_cm;y_cm;z_cm;" followed by the stack record (-2 bad index).
int sim_world_world_item_count(const SimWorld* world, const char* place);
int sim_world_world_item_at(const SimWorld* world, const char* place, int index, char* out, int cap);
// Places a container (id chosen by the caller; "" owner = anyone's; capacity
// in grams, 0 = unlimited). 0 or a refusal (-1 when the id is taken).
int sim_world_add_container(SimWorld* world, const char* id, const char* place, const char* kind,
                            const char* owner, int capacity_g);
// A container's stacks: count (-2 unknown container) and records (-2 bad index).
int sim_world_container_stack_count(const SimWorld* world, const char* container);
int sim_world_container_stack_at(const SimWorld* world, const char* container, int index, char* out,
                                 int cap);
// Moves up to qty units of a stack into / out of a container.
int sim_world_put_in(SimWorld* world, const char* actor, const char* container, int stack_index, int qty);
int sim_world_take_out(SimWorld* world, const char* actor, const char* container, int stack_index,
                       int qty, const char* witnesses_semicolon);

// FND-06: a timed action of `minutes` for the actor: minutes add up toward
// whole hours (carried across saves), and each whole hour advances hunger,
// thirst and fatigue like sim_world_advance_needs (sleeping != 0: asleep).
// minutes <= 0 or a null argument does nothing.
void sim_world_advance_minutes(SimWorld* world, const char* actor, int minutes, int sleeping);

// FND-09: the player notice feed. count is the sequence number the next
// notice will get (so the engine reads every seq from its last seen up to
// count-1); -1 on null. at writes "day;key;text" (key is a string-table key:
// notice.raid | notice.quest_failed | notice.verdict); -2 when that seq was
// never written or was evicted (only the newest 256 are kept).
int64_t sim_world_notice_count(const SimWorld* world);
int sim_world_notice_at(const SimWorld* world, int64_t seq, char* out, int cap);

#ifdef __cplusplus
}  // extern "C"
#endif
