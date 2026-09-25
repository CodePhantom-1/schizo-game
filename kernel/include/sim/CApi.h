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

#ifdef __cplusplus
}  // extern "C"
#endif
