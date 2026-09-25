// CApiWild.h — W4-C: the C ABI over the wild lands (sim/Wild.hpp,
// sim/WildActions.hpp). Included by sim/CApi.h; same conventions: no
// exception crosses the boundary; string outputs follow the buffer
// convention (write at most cap-1 bytes plus NUL, return the untruncated
// length, -1 on a null argument / bad index). Records are ';'-separated
// fields in the documented order; list fields inside a record use '|'.
//
// No fast travel (mechanics.md row 35): sim_world_travel returns the
// game-minutes the journey took — the engine plays them out and advances its
// own clock (whole days through sim_world_advance_days).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;

// Places and links ----------------------------------------------------------
// Every travel node: wild places, the city places the roads start from, and
// the off-map cities journeys reach.
int sim_world_wild_place_count(const SimWorld* world);
// "id;kind;source;danger;x_m;y_m;camp_site;edge_to;name"  (source: wild|place|city)
int sim_world_wild_place(const SimWorld* world, int index, char* out, int cap);
int sim_world_wild_link_count(const SimWorld* world);
// "id;from;to;kind;distance_m;danger"
int sim_world_wild_link(const SimWorld* world, int index, char* out, int cap);

// Where the kernel thinks the player stands (a node id).
int sim_world_player_place(const SimWorld* world, char* out, int cap);
// Engine sync when the player walks somewhere on his own feet (not a travel
// shortcut). 0 ok, -1 null, -2 unknown node.
int sim_world_set_player_place(SimWorld* world, const char* place);
// Today's weather id (weather.csv).
int sim_world_weather(const SimWorld* world, char* out, int cap);
// Minutes the cheapest route to `to` by `mode` would take today; -1 null,
// -5 no route.
int sim_world_route_minutes(const SimWorld* world, const char* to, const char* mode);

// Travel ----------------------------------------------------------------------
// Travels the player to `to` by `mode` (transport_modes.csv id) starting at
// `start_hour`; `stance` (may be null = "fight"): fight|pay|flee|rob|give|pass;
// `companions`: armed men with him. Returns 0 arrived, 1 halted on the way
// (beaten or collapsed), or a refusal: -1 null, -2 unknown place, -3 unknown
// mode, -4 needs an item, -5 no route, -6 already there, -7 escorting.
// `out` (may be null): "minutes=N;stopped_at=ID;halted_by=X;encounters=N".
int sim_world_travel(SimWorld* world, const char* to, const char* mode, int start_hour,
                     const char* stance, int companions, char* out, int cap);

// Encounter log (most recent 64, oldest first):
// "id;day;hour;link;encounter;kind;group;stance;outcome;foes;foes_lost;allies_lost"
int sim_world_encounter_count(const SimWorld* world);
int sim_world_encounter(const SimWorld* world, int index, char* out, int cap);

// Camps and groups -------------------------------------------------------------
// "place;group;status;strength;supplies;morale;leader;lookout;loot_units;captives;known;scouted"
// (captives: npc ids joined by '|')
int sim_world_camp_count(const SimWorld* world);
int sim_world_camp(const SimWorld* world, int index, char* out, int cap);
// "id;active;camp;leader;strength;supplies;morale;grudge;paid_until;allied;member;led;times_cleared"
int sim_world_group_count(const SimWorld* world);
int sim_world_group(const SimWorld* world, int index, char* out, int cap);
// Today's raid chance for a group, in basis points (0..2000); -1 null.
int sim_world_raid_chance(const SimWorld* world, const char* group);

// Recent raids (most recent 64, oldest first):
// "id;day;group;target;place;chance_bp;success;goods_taken;slain;taken;summary"
int sim_world_raid_count(const SimWorld* world);
int sim_world_raid(const SimWorld* world, int index, char* out, int cap);

// Caravans on the road: "id;caravan;name;at;guards;goods_units;escorted"
int sim_world_caravan_count(const SimWorld* world);
int sim_world_caravan(const SimWorld* world, int index, char* out, int cap);

// "" | "slain" | "captive:<group>" | "sold_east"
int sim_world_npc_fate(const SimWorld* world, const char* npc, char* out, int cap);
// Wild rumours the npc has heard, one per line; hearing one about a camp
// marks the camp known to the player.
int sim_world_hear_rumours(SimWorld* world, const char* npc, char* out, int cap);

int sim_world_herd_head(const SimWorld* world);
int sim_world_player_wounds(const SimWorld* world);
int sim_world_infamy(const SimWorld* world);
// The band the player rides with ("" = none).
int sim_world_player_group(const SimWorld* world, char* out, int cap);

// Player verbs (D-021 open play) ----------------------------------------------------
// Each returns the verb's code (WildActions.hpp: >= 0 done, -1 null/bad
// argument, -2 unknown camp/group/run/site, -3 not there, -4 not possible now,
// -5 cannot pay, -6 refused); `out` (may be null) receives the text.
int sim_world_scout_camp(SimWorld* world, const char* place, int hour, char* out, int cap);
int sim_world_attack_camp(SimWorld* world, const char* place, int companions, int hour, char* out,
                          int cap);
// objective: free_captives | steal_loot | sabotage | learn
int sim_world_infiltrate_camp(SimWorld* world, const char* place, const char* objective, int hour,
                              char* out, int cap);
int sim_world_pay_off_group(SimWorld* world, const char* group, char* out, int cap);
int sim_world_ally_with_group(SimWorld* world, const char* group, char* out, int cap);
int sim_world_join_group(SimWorld* world, const char* group, char* out, int cap);
int sim_world_leave_group(SimWorld* world, char* out, int cap);
int sim_world_challenge_leader(SimWorld* world, const char* group, int hour, char* out, int cap);
int sim_world_found_band(SimWorld* world, const char* camp_place, int companions, char* out,
                         int cap);
// target: fields | herds | caravans | market
int sim_world_lead_raid(SimWorld* world, const char* target, int hour, char* out, int cap);
int sim_world_ransom_captive(SimWorld* world, const char* npc, char* out, int cap);
int sim_world_escort_caravan(SimWorld* world, const char* run, int companions, char* out, int cap);
int sim_world_abandon_escort(SimWorld* world, char* out, int cap);
int sim_world_rob_caravan(SimWorld* world, const char* run, int companions, int hour, char* out,
                          int cap);
int sim_world_search_site(SimWorld* world, const char* place, int hour, char* out, int cap);

#ifdef __cplusplus
}  // extern "C"
#endif
