// CApi.h — the C ABI over the world kernel (coordinator-owned contract).
//
// WHY A C API: SimRuntime (UE5) links this kernel into a build system with its
// own flags. A C boundary means no STL/ABI coupling across toolchains — the
// engine passes strings and integers, the kernel does the rest.
//
// Ownership: SimWorld is owned by the caller (create → use → destroy).
// Strings: outputs are written into caller buffers (cap-limited); inputs are
// borrowed, never stored.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;  // opaque — one deterministic world

// Lifecycle ---------------------------------------------------------------
SimWorld* sim_world_create(const char* canon_dir, uint64_t seed);
void sim_world_destroy(SimWorld* world);

// Clock -------------------------------------------------------------------
void sim_world_advance_days(SimWorld* world, int days);
int64_t sim_world_day(const SimWorld* world);
// Season id for today ("rains"/"sowing"/"harvest"/"vintage"). Writes at most
// cap-1 bytes plus NUL; returns the untruncated length, or -1 on error.
int sim_world_season(const SimWorld* world, char* out, int cap);

// World facts (act content, coordinator-owned scalars) ---------------------
void sim_world_set_drought(SimWorld* world, int stage);
int sim_world_drought(const SimWorld* world);
void sim_world_set_war(SimWorld* world, int stage);
int sim_world_war(const SimWorld* world);

// Economy ------------------------------------------------------------------
int64_t sim_world_price(const SimWorld* world, const char* city, const char* item);

// Faction standing (0..100) ------------------------------------------------
int sim_world_standing(const SimWorld* world, const char* faction);
void sim_world_add_standing(SimWorld* world, const char* faction, int delta);

// The gods -----------------------------------------------------------------
int sim_world_favour(const SimWorld* world, const char* deity);
void sim_world_add_favour(SimWorld* world, const char* deity, int delta);

// Counters the engine UI reads ---------------------------------------------
int sim_world_verdict_count(const SimWorld* world);
int sim_world_event_count(const SimWorld* world);
int sim_world_npc_count(const SimWorld* world);

// Save / load ---------------------------------------------------------------
// Writes the world's save (Snapshot.hpp SIMSAVE format) to `path`.
// Returns 0 on success; -1 on a null argument; -2 if the file could not be
// opened for writing.
int sim_world_save(const SimWorld* world, const char* path);

// Loads a world previously saved with sim_world_save. `canon_dir` is the
// canon directory to rebuild the db/calendar from (same conventions as
// sim_world_create). Returns a new SimWorld the caller owns, or nullptr on
// a null argument, a file that could not be opened, or a malformed save.
SimWorld* sim_world_load(const char* canon_dir, const char* path);

// Buffer variants (cheap: the save is already an in-memory string).
// Writes at most cap-1 bytes plus NUL; returns the untruncated length, or
// -1 on a null argument.
int sim_world_save_to_buffer(const SimWorld* world, char* out, int cap);
// `data` is the NUL-terminated save text (as written by save_to_buffer or
// read from a sim_world_save file). Returns a new SimWorld, or nullptr on a
// null argument or a malformed save.
SimWorld* sim_world_load_from_buffer(const char* canon_dir, const char* data);

// Needs (hunger/thirst/fatigue, 0..100) --------------------------------------
// Each getter returns -1 for a null world/actor (an actor never seen yet
// reads as 0/0/0, matching Needs.hpp's needs_of()).
int sim_world_hunger(const SimWorld* world, const char* actor);
int sim_world_thirst(const SimWorld* world, const char* actor);
int sim_world_fatigue(const SimWorld* world, const char* actor);

// Advances one actor's needs by `hours` game-hours (the engine owns the
// sub-day clock; the day tick itself stays daily). `sleeping` is 0 or 1.
void sim_world_advance_needs(SimWorld* world, const char* actor, int hours, int sleeping);

// Eats/drinks one unit of `item` from the actor's own inventory (given/held
// via sim_world_give_item). "water" is always drinkable for
// sim_world_drink without touching the inventory (Needs.hpp's virtual
// always-available id); every other item, for both eat and drink, is
// consumed from inventory on success.
// Return codes (both functions):
//   0   ok — hunger/thirst applied and (unless "water") 1 unit consumed
//  -1   null world/actor/item
//  -2   the actor's inventory holds none of `item` (and item != "water")
//  -3   item is unknown, or not edible/drinkable (Needs.cpp categories)
int sim_world_eat(SimWorld* world, const char* actor, const char* item);
int sim_world_drink(SimWorld* world, const char* actor, const char* item);

// Named condition effects at the actor's current thresholds
// (need_effects(), semicolon-joined, e.g. "hungry;parched"). Writes at most
// cap-1 bytes plus NUL; returns the untruncated length, or -1 on a null
// argument.
int sim_world_need_effects(const SimWorld* world, const char* actor, char* out, int cap);

// Inventory ------------------------------------------------------------------
// Count of `item` the actor holds, or -1 on a null argument.
int sim_world_item_count(const SimWorld* world, const char* actor, const char* item);

// Adds `qty` (may be negative) to the actor's count of `item`; the count
// never drops below 0. Returns the resulting (clamped) count, or -1 on a
// null argument.
int sim_world_give_item(SimWorld* world, const char* actor, const char* item, int qty);

// Crafting --------------------------------------------------------------------
// Crafts `recipe` `times` times from the actor's own inventory, consuming
// its own inventory's items in place (Crafting.hpp: all-or-nothing).
// `stations_semicolon_list` is e.g. "quern;oven" (a null or empty string
// means no stations at hand).
// Return codes:
//   0   ok
//  -1   null world/actor/recipe, or times <= 0
//  -2   unknown or OPEN recipe id
//  -3   required station not in the list
//  -4   insufficient inputs in the actor's inventory
int sim_world_craft(SimWorld* world, const char* actor, const char* recipe,
                     const char* stations_semicolon_list, int times);

// Schedule --------------------------------------------------------------------
// The task in force for `role` at `hour` (0..23) on the current day.
// Writes at most cap-1 bytes plus NUL; returns the untruncated length, or
// -1 on a null argument or no schedule rows at all for that role.
int sim_world_task_at(const SimWorld* world, const char* role, int hour, char* out, int cap);

#ifdef __cplusplus
}  // extern "C"
#endif
