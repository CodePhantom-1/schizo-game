// CApiDivine.h — W5: the C ABI over divine wrath (sim/Divine.hpp). Included
// by sim/CApi.h; same conventions as sim/CApiWild.h: no exception crosses the
// boundary; string outputs follow the buffer convention (write at most cap-1
// bytes plus NUL, return the untruncated length, -1 on a null argument / bad
// index). Records are ';'-separated fields in the documented order.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;

// The offender's wrath with a deity, 0..100; -1 on a null argument.
int sim_world_divine_wrath(const SimWorld* world, const char* offender, const char* deity);
// The wrath tier 0..3 (<20 unnoticed, >=20 ill omens, >=50 disfavour,
// >=80 curse); -1 on a null argument.
int sim_world_divine_tier(const SimWorld* world, const char* offender, const char* deity);
// 1 when the offender is under the top-tier curse (and `out`, if given,
// receives the cursing deity's id), 0 when not; -1 on a null argument.
int sim_world_divine_cursed(const SimWorld* world, const char* offender, char* out, int cap);

// Every wrath ledger entry in map order (deterministic): count, then
// "offender;deity;wrath" per index.
int sim_world_divine_entry_count(const SimWorld* world);
int sim_world_divine_entry(const SimWorld* world, int index, char* out, int cap);

// Breaks a sworn oath AND notes the divine consequence (mechanics.md row 26:
// the oath-breaker curse — the gods take notice). 0 broken, -1 null,
// -2 no such oath, -3 already broken.
int sim_world_break_oath(SimWorld* world, const char* oath_id);

#ifdef __cplusplus
}  // extern "C"
#endif
