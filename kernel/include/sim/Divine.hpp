#pragma once
// Divine.hpp — W5: divine wrath. The gods keep books. Every offence against
// them — a broken sworn oath (mechanics.md row 26: "sworn treaty oaths with
// the oath-breaker curse"), a rite performed impure or failed (rows 13/20:
// purity is load-bearing), a conviction for a crime against the gods
// (laws.csv sacrilege / tomb_robbery / oath_breaking) — accrues wrath with the
// offended deity (deities.csv id). Wrath escalates through tiers:
//
//   tier 0  wrath <  20  unnoticed
//   tier 1  wrath >= 20  ill omens   — the asker's divinations about that god
//                                      read worse (Rites.cpp divination hook)
//   tier 2  wrath >= 50  disfavour    — favour penalty with the god on each
//                                      tier >= 2 crossed (Magic's favour,
//                                      through the Rites favour system)
//   tier 3  wrath >= 80  the curse    — a condition on the offender that
//                                      persists until an atonement rite
//                                      (rites.csv su_ila_supplication) is
//                                      performed successfully to that god
//
// Wrath decays slowly with time and drops sharply on a successful atonement.
// Every threshold, amount and effect here is INVENTED (the designer rows name
// the causes and the shape, not the numbers) and is ledgered in
// docs/proposals/invented-ledger-divine.md.
//
// Like sim/RiteEffects.hpp this header carries state + verbs but no World.hpp
// dependency (WorldState is forward-declared; Divine.cpp includes World.hpp),
// so sim/World.hpp can hold a DivineState member without a cycle.
//
// Determinism: no wall clock, no static mutable state, no randomness — wrath
// is pure bookkeeping over the call sequence.
#include "sim/Types.hpp"
#include "sim/Db.hpp"  // note_divine_conviction reads canon (city patron deities)

#include <map>
#include <string>

namespace sim {

struct WorldState;  // defined in sim/World.hpp (the seam — never included by module headers)

// --- INVENTED constants (retunable; ledgered) --------------------------------

// Wrath tiers (wrath is 0..100, like favour and standing).
constexpr int kWrathMax = 100;
constexpr int kWrathIllOmenAt = 20;   // tier 1: omens about the god read worse
constexpr int kWrathDisfavourAt = 50; // tier 2: favour penalties begin
constexpr int kWrathCurseAt = 80;     // tier 3: the curse condition

// Accrual: one offence's worth of wrath.
constexpr int kOathBreakWrath = 15;   // Faction::break_oath (mechanics.md row 26)
constexpr int kImpureRiteWrath = 10;  // a performed rite while impure (rows 13/20)
constexpr int kFailedRiteWrath = 5;   // a performed rite that fails (rows 20/13)
constexpr int kConvictionWrath = 30;  // a conviction for a divine offence ("substantial")

// Consequences.
constexpr int kTierFavourPenalty = 10;  // favour with the god lost per tier >= 2 crossed
constexpr int kIllOmenFavourStep = 10;  // perceived-favour penalty per tier in a divination
constexpr int kAtonementWrathDrop = 40; // a successful atonement drops wrath by this ("sharply")
constexpr int kAtonementFavour = 5;     // the appeased god's favour recovers by this

// Time.
constexpr int kWrathDecayDays = 7;     // one wrath point leaves every N days ("slowly")

// Purity: "impure" for the rite offence is below the rite's own
// purity_required (rites.csv; Magic.cpp reading 8), or below this floor when
// the row sets no requirement (every canon row but the atonement rite).
constexpr int kImpureBelow = 50;

// The deities the fallbacks accrue to (deities.csv ids; ledgered).
inline const Id kOathWitnessDeity = "utu";           // sworn oaths are witnessed by the sun
inline const Id kTombDeity = "underworld_queen";     // the dead's own queen curses grave robbers
inline const Id kDivineLedgerDeity = "an";           // the generic ledger: the father of the gods

// The atonement rite (rites.csv, INVENTED row, W5): a successful performance
// addressed to an offended god drops his wrath sharply and lifts his curse.
inline const Id kAtonementRite = "su_ila_supplication";

struct DivineState {
    // offender id ("player" or an npc) -> deity id -> wrath 0..100. The gods
    // keep one book per pair: wrath with Utu does not spill onto Nanna.
    std::map<Id, std::map<Id, int>> wrath_by_offender;
    // Offenders under the top-tier curse: who, and which god's wrath laid it.
    // Set when wrath reaches tier 3; cleared ONLY by a successful atonement
    // rite to that god — never by time (see tick_divine).
    std::map<Id, Id> curse_by_offender;
    // Favour penalties earned but not yet applied: deity -> signed delta,
    // applied to the performer's favour (Magic, through add_favour) on the
    // next divine tick. The accrual hooks run where MagicState is not
    // writable (Faction.cpp, Justice.cpp), so the displeasure lands at dawn.
    std::map<Id, int> pending_favour_penalty;
    int decay_acc = 0;  // days counted toward the next wrath-decay step
};

// --- queries -----------------------------------------------------------------

// The offender's wrath with the deity (0 when unknown; a read creates nothing).
int wrath(const DivineState& s, const Id& offender, const Id& deity);
// 0..3 for a wrath value: <20 unnoticed, >=20 ill omens, >=50 disfavour, >=80 curse.
int wrath_tier(int wrath_value);
bool is_cursed(const DivineState& s, const Id& offender);
// The deity whose wrath cursed the offender; "" when not cursed.
Id curse_deity(const DivineState& s, const Id& offender);
// How badly divine wrath skews a divination about `deity` for `offender`:
// the tier's perceived-favour penalty (Rites.cpp subtracts it from the omen's
// truth test, so the god looks less favourable than his favour says).
int divine_omen_penalty(const WorldState& w, const Id& offender, const Id& deity);

// --- the three designer hooks (each is called by one line in one module) ------

// mechanics.md row 26: an oath is broken, the gods take notice — the breaker
// gains wrath with the oath-witness god (kOathWitnessDeity). Called from
// Faction.cpp break_oath's optional divine hook.
void note_oath_break(DivineState& s, const Id& offender);

// mechanics.md rows 13/20: a rite performed while impure, or performed and
// failed, offends the deity the rite addresses. Called from
// Rites.cpp perform_rite_in_world after a PERFORMED rite (refusals offend
// nobody: nothing was attempted before the gods).
void note_rite_offence(WorldState& w, const Id& rite_id, const Id& addressed, bool succeeded);

// A conviction (verdict != dismissed) for an offence against the gods adds
// substantial wrath: laws.csv sacrilege (temple robbery — the temple's city's
// patron deity), tomb_robbery (the underworld queen), oath_breaking (the oath
// witness). Other crimes offend no god. Called from Justice.cpp hold_hearing's
// optional divine hook.
void note_divine_conviction(DivineState& s, const Db& db, const Id& criminal,
                            const Id& law_row, const Id& crime_kind, const Id& place_city);

// --- atonement and time -------------------------------------------------------

// A successful kAtonementRite performance addressed to `deity`: his wrath
// drops by kAtonementWrathDrop, his curse lifts if wrath falls below tier 3,
// his pending anger is withdrawn and his favour recovers. Returns the effect
// text ("atonement:<deity>:<before>-><after>"), "" when there was nothing to
// atone. Called from Rites.cpp perform_rite_in_world's success path.
std::string perform_atonement(WorldState& w, const Id& deity);

// The world-level oath-break verb (the engine's path; Faction's break_oath
// plus the gods noticing): breaks the oath and notes the wrath in one call.
void break_oath_in_world(WorldState& w, const Id& oath_id);

// The divine tick (advance_days, once a day): pending favour penalties land
// on the performer's favour; wrath decays kWrathDecayDays times slower than
// days — except the cursing god's wrath on a cursed offender, which time
// cannot touch (only atonement lifts a curse).
void tick_divine(WorldState& w);

}  // namespace sim
