# Invented ledger — W2-A (the world's verbs: crime cycle + quest consequences)

Per D-018 ("the designer authorized creative gap-filling... every invented
item ledgered"). One line each: what, why it fits theme and canon. None of
these contradict a CANON/A row; all are retunable and vetoable per D-018.

- **`temple_of_sun_and_moon` faction row (db/canon/factions.csv, INVENTED).**
  cities.csv:city_of_the_moon already lists "temple of sun and moon" as a
  feature and wb §3 describes its offering pattern; no faction row existed
  for it, so `the_priestess_debt` and its act_ii-iv deepenings had nowhere
  to credit standing. Added as `kind=minor`, sourced to wb §3 / cities.csv /
  notes L52, matching the shape of the existing minor-cult faction rows
  (`sky_cult`, `dead_cult`).

- **quests.csv `reward_silver`/`reward_faction`/`reward_standing` columns
  (INVENTED, all 41 rows).** scenario_quest.md documented this as a pure
  data gap (no reward column existed at all). Silver scaled by act depth
  (opening/act_i/act_ii/act_iii/act_iv = 1/2/3/4/6) and quest kind
  (systemic/emergent/faction/history_arc = 1.0/1.1/1.3/1.5), nudged up by
  `deadline_days` (longer commitment, larger payout), rounded to 5 grains.
  `reward_faction`/`reward_standing` set only where the giver/story text
  names a faction unambiguously (temple stewards -> temple_of_sun_and_moon;
  "the rebellious/southern alliance" -> southern_city_states_alliance; "the
  sage of the swamps"/the Prophet -> neo_sumerian_rebellion; eastern
  newcomers/mountain people -> the_barbarians; the warchief's envoy ->
  eastern_barbarian_alliance; the veiled courier -> brotherhood_of_the_serpent;
  the sukkal of the imperial court -> the_empire); left blank everywhere
  else rather than guessing a tie that isn't in the text.

- **The city -> jurisdiction-faction map (`Actions::city_faction`,
  kernel/src/Actions.cpp, INVENTED).** city-life §3.1 makes enforcement
  jurisdictional but no canon table maps a city to "whose law it keeps".
  Built from each faction row's own `canon` text naming a city
  (`city_of_the_moon` -> `southern_city_states_alliance`, home per
  factions.csv; `city_of_the_sun` -> `retributors_of_utu`, headquartered
  there; `city_of_the_warrior_spirit` -> `eastern_barbarian_alliance`, ruled
  by its warchief; `city_of_the_dead`/`gravestone` -> `dead_cult`;
  `city_of_the_sky` -> `sky_cult`); cities with no explicit tie
  (`city_of_jewels`, `city_of_kings`, `city_of_the_abyss`) default to
  `the_empire` (wb §4.1: built on tribute and conquest — the fallback
  crown).

- **Hearing delay `kHearingDelayDays = 3`
  (kernel/include/sim/Actions.hpp, INVENTED).** city-life §3.3.3 says
  detention runs "game hours to days" without a number. Picked a short,
  playable window; retunable constant, not canon content.

- **Compensation amount `kCompensationSilver = 50`
  (kernel/include/sim/Actions.hpp, INVENTED).** city-life §3.2 describes
  theft compensation as "the stolen [item] plus added compensation" without
  a number, and no items/value system prices stolen goods yet (D-011
  deferral). A flat silver-grain amount stands in until that system exists;
  ledgered exactly as D-011's own price constants were (compare the Economy
  price-model constants D-011 already ratified).

- **Standing-loss severity ladder by verdict
  (`Actions.cpp::standing_penalty`, INVENTED).** No canon number exists for
  "how much a verdict costs your standing with the jurisdiction". Scaled by
  severity: dismissed 0, compensation -5, debt_service -8, confiscation -10,
  exile -25, death -40. Retunable.

- **Outlawry as `FactionState::outlawed_by_faction` + `Faction::outlaw()`
  (kernel/include/sim/Faction.hpp, INVENTED machinery).** mechanics.md row
  12 names "outlawry -> rank 0", but the kernel carries no numeric per-
  faction rank state anywhere (ranks.csv is content, not simulated state).
  Rank 0 (D-015: "the Outsider" — stateless outside any city's protection)
  is modeled as zero standing plus an explicit outlawed flag with that
  jurisdiction, the closest kernel-state equivalent without inventing a
  full rank-tracking system this wave didn't ask for.

- **`Crime::stage` / `Crime::hearing_day` and `Hearing::tablet_id`/
  `compensation_paid` (kernel/include/sim/Justice.hpp, additive fields).**
  Not new canon, just state to carry city-life §3.3's documented pipeline
  (alarm -> pursuit -> detention -> hearing, and the sealed tablet) through
  the kernel; `hearing_day` defaults to 0 so every pre-existing crime filed
  via plain `report_crime()` keeps the original same-tick auto-hearing
  behaviour (`tick_justice` in kernel/src/Justice.cpp), and only crimes
  orchestrated through `commit_crime()` get the real delay.
