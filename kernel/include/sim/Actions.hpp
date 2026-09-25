#pragma once
// Actions.hpp — W2-A: the world's verbs. Orchestrates the crime cycle
// (city-life §2-3, scenario_crime.md gaps 1-6) and quest consequences
// (scenario_quest.md's "reward/standing pipe" gap) across several modules'
// state (Population, Justice, Property, Faction, Quests) — a WorldState&
// verb, exactly the escalation scenario_crime.md #6 and scenario_quest.md's
// missing pipes ask for. Per the track brief, this file and World.cpp are
// the only places allowed to write more than one module's state.
//
// Determinism: no wall clock, no static mutable state; the only randomness
// available is ctx.rng, and nothing here draws from it — every outcome is a
// function of canon (laws.csv) and the call arguments.
#include "sim/World.hpp"

#include <string>
#include <vector>

namespace sim {

// city-life §3.3: "Detention... game hours to days" — no exact number is
// given. INVENTED N, retunable.
constexpr int kHearingDelayDays = 3;

// The flat compensation amount city-life §3.2 describes as "the stolen
// [item] plus added compensation" without giving a number until an
// items/value system prices stolen goods (D-011 deferral). INVENTED,
// retunable.
constexpr Silver kCompensationSilver = 50;

// commit_crime (scenario_crime.md gap 6, "a commit crime verb" + gaps 1-2,
// alarm/pursuit/detention + guard response): every witness remembers the
// act (Population::witness); if witnessed at all, the watch responds
// (city-life §3.1's INVENTED watch role) and the crime moves through
// alarmed -> pursued -> detained in the same call (Wave 1 collapses the
// chase to a stage transition, same scope as Justice.cpp's existing
// same-tick hearing). Files the crime on the docket (Justice::report_crime)
// and returns its id. Does NOT hold the hearing — call hold_crime_hearing()
// kHearingDelayDays later (gap 1: a real window between detention and trial).
Id commit_crime(WorldState& w, const Id& criminal, const Id& law_row,
                const Id& place_city, const std::vector<Id>& witnesses,
                const Id& victim = "");

// Hears every commit_crime crime whose hearing_day has come, with its stored
// victim and city (called by WorldState::advance_days each day).
void hold_due_hearings(WorldState& w);

// INVENTED mapping (city-life §3.1: enforcement is per jurisdiction) from a
// place to the faction whose law/court holds it, used for standing loss and
// outlawry. Cities with no explicit canon tie default to "the_empire" (built
// on tribute and conquest, wb §4.1 — the Empire is the fallback crown).
// Reads cities.csv `jurisdiction` (alias rows resolve to their target).
Id city_faction(const Db& db, const Id& place_city);

// hold_crime_hearing (scenario_crime.md gaps 1, 3, 4, 5): seals the hearing
// (Justice::hold_hearing, unchanged) and then applies the verdict:
//  - a sealed verdict tablet id on the Hearing ("verdict_tablet_<crime_id>";
//    the item itself stays engine content per Justice.hpp)
//  - "compensation": paid from the criminal's purse to the victim's purse
//    (Property); if the purse can't cover it, escalates to the law row's
//    next penalty_options entry, or "debt_service" (city-life §3.3.6) when
//    none remains
//  - standing loss with the jurisdiction's faction (city_faction), scaled
//    by verdict severity (INVENTED ladder)
//  - "exile"/"death": outlaws the criminal with that faction — rank 0
//    (mechanics.md row 12; Faction::outlaw, since the kernel carries no
//    numeric rank state — see Faction.hpp)
// victim may be "" (the crown/temple/community is the victim, e.g.
// sacrilege) — compensation then has nowhere to land and is skipped, same
// as an unpayable debt: it escalates.
Hearing hold_crime_hearing(WorldState& w, const Id& crime_id, const Id& victim,
                           const Id& place_city);

// complete_quest (scenario_quest.md's missing reward/standing pipes): moves
// the quest to completed (Quests::complete, unchanged) and, when the def
// carries reward data (quests.csv reward_silver/reward_faction/
// reward_standing, W2-A), credits the player's purse and/or adds standing.
// The quest giver's silver always goes to "player" (the single protagonist,
// game-design §1). No-op reward when def is unknown (no canon row) or
// carries none.
void complete_quest(WorldState& w, const Id& def_id, DayNumber day);

// fail_quest: fails an active quest outright (e.g. the player abandons it),
// independent of tick_quests' deadline sweep. No reward either way.
void fail_quest(WorldState& w, const Id& def_id, DayNumber day);

// K-1: a timed ward's default duration when a rite's canon row gives no
// number (rpg-systems §10.3: "lasts hours or days" — no exact figure).
// INVENTED, retunable.
constexpr int kWardDurationDays = 7;

// K-1: a "healing and purification" rite's flat relief/restoration amount
// (rpg-systems §10.3 names the effect, not a number). INVENTED, retunable.
constexpr int kHealingRelief = 30;       // hunger/thirst/fatigue, floored at 0
constexpr int kPurificationBoost = 30;   // purity, capped at 100

// K-1: the price of a "curse and binding" rite (rpg-systems §10.3: "sorcery
// is a crime" — being caught costs standing, but every attempt, caught or
// not, is INVENTED here to cost the performer's own purity; no canon row
// exercises this family yet). INVENTED, retunable.
constexpr int kCursePurityCost = 15;

// perform_rite_action (scenario_rite.md gaps 1-5): the engine-facing verb
// for "one rite, start to finish". Sequences what sim/Magic.hpp deliberately
// leaves to the caller (Magic.hpp:22-23):
//  - knowledge: reads MagicState::known_rites (knows_rite) instead of the
//    caller re-deriving it every attempt (gap 1);
//  - materials: reads what `performer` holds (w.inventories), via the same
//    normalized keys Magic.cpp scores against (rite_material_keys) — "water"
//    is free well-water and never checked/consumed, the Crafting.cpp rule
//    (module_Needs.md / Crafting.hpp); a PERFORMED attempt (success or
//    failure) consumes 1 unit of each other required material — materials
//    are presence-only (Magic.hpp reading 5), so quantities beyond 1 are not
//    modeled; a REFUSED attempt (performed == false: unknown rite, or not
//    known) leaves the inventory byte-for-byte untouched (gap 2);
//  - purity/place/time: `place` sets MagicState::place for this attempt;
//    purity gating is now real once a rite's canon row carries
//    purity_required (gap 3 — see db/canon/rites.csv);
//  - effect: on success, applies RiteResult.effect_family by family (gap 5)
//    — protection/blessing/curse become a Ward (Magic::add_ward), divination
//    becomes an Omen (Magic::add_omen, probability = the rite's own score),
//    healing/purification relieves Needs and/or purity. Substitution,
//    ancestors and divine intervention have no canon rite yet (scenario_rite
//    .md's "not exercised, out of scope") and are left as a no-op here.
RiteResult perform_rite_action(WorldState& w, const Id& performer, const Id& rite_id,
                               const Id& place);

}  // namespace sim
