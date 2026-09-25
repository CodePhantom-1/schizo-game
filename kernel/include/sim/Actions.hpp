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
                const Id& place_city, const std::vector<Id>& witnesses);

// INVENTED mapping (city-life §3.1: enforcement is per jurisdiction) from a
// place to the faction whose law/court holds it, used for standing loss and
// outlawry. Cities with no explicit canon tie default to "the_empire" (built
// on tribute and conquest, wb §4.1 — the Empire is the fallback crown).
Id city_faction(const Id& place_city);

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

}  // namespace sim
