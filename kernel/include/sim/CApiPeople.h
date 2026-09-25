// CApiPeople.h — A7: NPC names, memories and the events feed through the C ABI.
// Included by sim/CApi.h. Conventions as CApiQuests.h (index lists, the
// buffer convention, -1 on a null argument, an unknown npc or a bad index).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;

// The npc's display name (people.csv `name`, e.g. "Ur-Utu"), never an id.
int sim_world_npc_name(const SimWorld* world, const char* npc, char* out, int cap);
// What the npc remembers (witnessed or heard as rumour), ordered by day.
int sim_world_npc_memory_count(const SimWorld* world, const char* npc);
// Memory `index`: its day, subject and fact; returns the fact's length.
int sim_world_npc_memory_at(const SimWorld* world, const char* npc, int index, int64_t* day,
                            char* subject, int subject_cap, char* fact, int fact_cap);
// Fired event `index` (0..sim_world_event_count-1, oldest first): its day,
// events.csv rule id and summary; returns the summary's length.
int sim_world_event_at(const SimWorld* world, int index, int64_t* day,
                       char* rule, int rule_cap, char* summary, int summary_cap);

#ifdef __cplusplus
}  // extern "C"
#endif
