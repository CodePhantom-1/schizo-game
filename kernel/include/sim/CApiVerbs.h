// CApiVerbs.h — W6-A: the C ABI the player's engine-side verbs read beyond
// CApi.h's own surface (inventory enumeration and best-carried food/drink).
// Included by sim/CApi.h; same conventions as CApiFaction.h: no exception
// crosses the boundary; string outputs follow the buffer convention (write at
// most cap-1 bytes plus NUL, return the untruncated length, -1 on a null
// argument).
//
// Why this exists: eat/drink-by-key and the carried-items HUD need to ask
// "what does the actor hold?" — CApi.h answers only per-item (item_count),
// which forced a fixed canon shortlist (the parked ue-2 spec's ponytail).
// These calls keep the kernel the single source of truth for both the list
// and the food/drink categories (Needs.cpp's restore table).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;

// Everything the actor holds, as "item:count" records ';'-joined and sorted by
// item id (the inventory is a std::map, so the order is stable). "" for an
// actor the world has never seen. Buffer convention; -1 on a null argument.
int sim_world_inventory(const SimWorld* world, const char* actor, char* out, int cap);

// The held item eating one unit of which restores the most hunger; "" when the
// actor holds no food at all (Needs.cpp's categories decide). Ties break to
// the first item id in sorted order, so the answer is deterministic. "water"
// is never the answer — it is drinkable but not holdable (virtual id).
// Buffer convention; -1 on a null argument.
int sim_world_best_food(const SimWorld* world, const char* actor, char* out, int cap);

// As sim_world_best_food for thirst: the held item drinking one unit of which
// restores the most; "" when the actor holds nothing drinkable. "water" is
// not held and so is never the answer — the engine's well verb passes it to
// sim_world_drink directly. Buffer convention; -1 on a null argument.
int sim_world_best_drink(const SimWorld* world, const char* actor, char* out, int cap);

#ifdef __cplusplus
}  // extern "C"
#endif
