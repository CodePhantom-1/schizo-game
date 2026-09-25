// CApi.h — the C ABI over the world kernel (coordinator-owned contract).
//
// WHY A C API: SimRuntime (UE5) links this kernel into a build system with its
// own flags. A C boundary means no STL/ABI coupling across toolchains — the
// engine passes strings and integers, the kernel does the rest.
//
// Ownership: SimWorld is owned by the caller (create → use → destroy).
// Strings: outputs are written into caller buffers (cap-limited); inputs are
// borrowed, never stored.
// Exceptions: none ever cross this boundary. On an internal failure (e.g.
// out of memory) a function returns its error value (-1, or nullptr for the
// constructors); void functions become a no-op for the failed step.
// Canon: create/load return nullptr when canon_dir holds no seasons.csv.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;  // opaque — one deterministic world

// Lifecycle ---------------------------------------------------------------
SimWorld* sim_world_create(const char* canon_dir, uint64_t seed);
// Frees the world. A null world is a no-op; never throws.
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

// Rites (K-1: knowledge -> offerings -> effects; sim/Rites.hpp) -------------
// The performer is the player: MagicState is one performer's magic, and
// offerings are debited from the "player" inventory (sim_world_give_item).

// 1 if the player knows `rite`, 0 if not, -1 on a null argument.
int sim_world_knows_rite(const SimWorld* world, const char* rite);
// Known rite ids, sorted, semicolon-joined. Buffer convention (writes at most
// cap-1 bytes plus NUL; returns the untruncated length, -1 on null).
int sim_world_known_rites(const SimWorld* world, char* out, int cap);

// Learns `rite` from the npc `teacher` (a Population npc whose schedule role
// rite_teachings.csv lists for it) / from the text item `item` held in the
// player's inventory (read, not consumed).
// Return codes (both functions):
//   0   learned
//   1   already known (nothing changes)
//  -1   null argument
//  -2   unknown or OPEN rite
//  -3   unknown teacher npc                        (teacher variant only)
//  -4   this teacher / text does not teach the rite
//  -5   the text is not in the player's inventory  (text variant only)
int sim_world_learn_rite_from_teacher(SimWorld* world, const char* rite, const char* teacher);
int sim_world_learn_rite_from_text(SimWorld* world, const char* rite, const char* item);

// Rite ids the npc teaches / the text item teaches, sorted, semicolon-joined
// (empty for an unknown npc/item). Buffer convention; -1 on null.
int sim_world_rites_taught_by(const SimWorld* world, const char* teacher, char* out, int cap);
int sim_world_rites_taught_in(const SimWorld* world, const char* item, char* out, int cap);

// The performer's place tag ("temple:city_of_the_moon", "household:player"...)
// the rite's place requirement is matched against, and his purity (0..100,
// set clamps). Getters: buffer convention / -1 on null.
void sim_world_set_rite_place(SimWorld* world, const char* place);
int sim_world_rite_place(const SimWorld* world, char* out, int cap);
int sim_world_purity(const SimWorld* world);
void sim_world_set_purity(SimWorld* world, int purity);

// Performs `rite` in the world (Rites.hpp perform_rite_in_world). `target`
// (may be null/empty): the deities.csv id a deity="any" rite addresses, or
// asks about (divination); the place tag a protection rite wards ("" = the
// rite place). `effect_out` (may be null) receives what the success did, e.g.
// "favour:the_two_waters:+5", "ward:household:player:until=35",
// "omen:inanna:favourable:75" -- empty on failure or refusal.
// Return codes:
//   1   performed and succeeded (effect applied; offerings consumed)
//   0   performed and failed (offerings still consumed; favour may fall)
//  -1   null world/rite
//  -2   unknown or OPEN rite
//  -3   the player does not know the rite
//  -4   nothing addressed (an "any" rite with no god, a ward with no place)
//  -5   target names no live deities.csv row
// A negative code changes no state, except a -1 from an internal failure
// (e.g. out of memory), which may leave the rite partly applied.
int sim_world_perform_rite(SimWorld* world, const char* rite, const char* target,
                           char* effect_out, int cap);

// Last day (inclusive) a ward holds on `place`; 0 if none was ever laid; -1 null.
int64_t sim_world_ward_until(const SimWorld* world, const char* place);
// 1 if a ward holds on `place` today, else 0; -1 on null.
int sim_world_warded(const SimWorld* world, const char* place);

// Omens read so far, oldest first: sim_world_omen_count returns how many, or
// -1 on a null world. sim_world_omen writes omen `index` as
// "day;rite;subject;sign;confidence_pct" (buffer convention); -1 on a null
// argument or an index out of range.
int sim_world_omen_count(const SimWorld* world);
int sim_world_omen(const SimWorld* world, int index, char* out, int cap);

// Festivals (K-2) ---------------------------------------------------------------
// 1 when the current day is a festival day, 0 when not, -1 on a null world.
int sim_world_is_festival(const SimWorld* world);
// As above for any day number (>= 1); -1 on a null world or day < 1.
int sim_world_is_festival_day(const SimWorld* world, int64_t day);
// Today's festival id (festivals.csv, e.g. "new_waters"), "" when none.
// Writes at most cap-1 bytes plus NUL; returns the untruncated length, or -1
// on a null argument.
int sim_world_festival(const SimWorld* world, char* out, int cap);
// The festival id on any day number (>= 1), "" when none; -1 on a null
// argument or day < 1.
int sim_world_festival_on(const SimWorld* world, int64_t day, char* out, int cap);
// 1 when the market trades today, 0 when today's festival closes it, -1 on a
// null world.
int sim_world_market_open(const SimWorld* world);

// People and their day (K-2) ------------------------------------------------------
// The id of the npc at `index` (0..sim_world_npc_count-1). Writes at most
// cap-1 bytes plus NUL; returns the untruncated length, or -1 on a null
// argument or an index out of range.
int sim_world_npc_id(const SimWorld* world, int index, char* out, int cap);
// What npc `npc` is doing at `hour` (0..23) on the current day: per-person
// resolution (role + person overrides + season + festival). Each writes at
// most cap-1 bytes plus NUL and returns the untruncated length, or -1 on a
// null argument, an unknown npc, or an npc with no schedule at all.
//   _task_at     the task text
//   _place_at    where: a places.csv id ("" = unplaced / off the street)
//   _schedule_at the schedule row id (or festival id for a festival gathering)
int sim_world_npc_task_at(const SimWorld* world, const char* npc, int hour, char* out, int cap);
int sim_world_npc_place_at(const SimWorld* world, const char* npc, int hour, char* out, int cap);
int sim_world_npc_schedule_at(const SimWorld* world, const char* npc, int hour, char* out,
                              int cap);

#ifdef __cplusplus
}  // extern "C"
#endif
