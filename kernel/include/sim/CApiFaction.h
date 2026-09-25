// CApiFaction.h — W5-B: the C ABI over faction politics (sim/Faction.hpp,
// the treaty table and the raid formula's politics layer). Included by
// sim/CApi.h; same conventions as CApiWild.h: no exception crosses the
// boundary; string outputs follow the buffer convention (write at most cap-1
// bytes plus NUL, return the untruncated length, -1 on a null argument / bad
// index). Records are ';'-separated fields; list fields use '|'.
//
// The player's standing itself is already exposed in CApi.h
// (sim_world_standing / sim_world_add_standing); this header carries the rest
// of the Faction surface plus the W5-B raid-politics reads.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;

// Standing tiers (rpg-systems §4.2): the tier name of the player's standing
// with `faction` ("Stranger".."Oath-bound"). Buffer convention; -1 on null.
int sim_world_faction_tier(const SimWorld* world, const char* faction, char* out, int cap);

// Oaths ever sworn (oldest first): "id;swearer;to_faction;day;broken".
int sim_world_oath_count(const SimWorld* world);
int sim_world_oath(const SimWorld* world, int index, char* out, int cap);

// 1 when the oath-breaker curse is on the record, 0 when not; -1 on null.
int sim_world_oath_breaker_curse(const SimWorld* world);

// 1 when the player stands outlawed with `faction` (Faction.hpp is_outlawed:
// standing forced 0, rank 0), 0 when not; -1 on null.
int sim_world_is_outlawed(const SimWorld* world, const char* faction);

// Treaties (treaties.csv): "id;name;parties;terms" (parties '|'-joined; terms
// carries only the machine tokens, '|'-joined — e.g. "no_raids" — because the
// prose around them holds semicolons).
int sim_world_treaty_count(const SimWorld* world);
int sim_world_treaty(const SimWorld* world, int index, char* out, int cap);
// 1 when a live sworn peace ("no_raids" terms, neither party outlawed) binds
// `a` and `b`, 0 when not; -1 on a null argument.
int sim_world_treaty_live_between(const SimWorld* world, const char* a, const char* b);

// Band politics (the raid formula's W5-B layer), for the UI: what a band's
// faction owes the city today. "faction;net_pts;blocked;note" — faction ""
// means unaligned outlaws; net_pts is the political modifier on its raids on
// the city's holdings (positive = politics urges the raid); blocked = 1 when
// a live treaty reins it to nothing; note names what fired, ','-joined
// (war | grudge | treaty | outlaw). -1 on a null argument / unknown group.
int sim_world_band_politics(const SimWorld* world, const char* group, char* out, int cap);

#ifdef __cplusplus
}  // extern "C"
#endif
