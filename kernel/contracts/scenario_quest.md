# SCENARIO CONTRACT — T7: one quest from a person with a problem

**Test:** `kernel/tests/test_scenario_quest.cpp` (registered automatically by
`kernel/CMakeLists.txt`'s `test_*.cpp` glob). 5 test functions, all green.

## The chosen quest

**`the_priestess_debt`** (`db/canon/quests.csv`, act_i, kind `emergent`,
`deadline_days=4`, giver "a debtor priestess"): a priestess of the temple of
sun and moon owes barley she cannot be seen chasing herself, and hires the
newly arrived outsider to collect from a grain merchant who "laughs at
temple marks."

**Why this row over the other Act I candidates:**

| Candidate | Why not it |
|---|---|
| `water_in_a_breaking_world`, `wages_of_bread_and_beer`, `the_gate_and_its_toll` (systemic) | chores generated from world conditions (thirst, hunger, a toll), not a *person's* problem told in a voice — the module contract itself distinguishes `systemic` from `emergent`/`faction`/`history_arc` |
| `a_roof_for_the_outsider` (emergent) | closer, but the "problem" is a wall to mend for a place to sleep — labor, not a person's stake in an outcome |
| `errand_for_the_caravan_master`, `an_offering_for_sun_and_moon` (systemic/faction) | tasks handed down by a role, not a problem belonging to the giver |
| **`the_priestess_debt`** | a named person (voiced in `dialogues.csv:texture_debtor_priestess_ledger` — "the offerings have thinned... the whole city is borrowed") with a concrete, personal problem (a debt she cannot chase herself) and a concrete ask; it also deepens forward into act_ii's `twenty_grains_on_the_hundred` (the same priestess, now creditor herself), so the slice sits on a real content thread instead of an orphan row |

All in the City of the Moon (D-005: the arrival/slice city) — the row carries
no explicit `city` column (see Data gaps below), but its giver, dialogue and
every other Act I row are written for the arrival city; nothing in the data
places it elsewhere.

## What the scenario test proves

On a real `WorldState` (`sim/World.hpp`), seeded from the actual
`db/canon` (not a fixture):

1. **Offer** — the def loads from live canon (`kind=emergent`,
   `deadline_days=4`), the row's `act`/`giver`/`source_ref` match the
   intended person-with-a-problem framing, and the giver's own dialogue
   line exists and is voiced correctly.
2. **Accept** — `accept()` sets `deadline = accepted + 4`, `stage =
   "accepted"`.
3. **Progress** — the free-form `Quest::stage` field is mutated
   (`"confronted_the_grain_merchant"`) across a tick boundary. This
   demonstrates and documents a real hazard: **`accept()`'s returned
   `Quest&` does not survive a subsequent `advance_days()`** —
   `tick_quests` move-assigns a new `active` vector every ticked day
   (`src/Quests.cpp`), so any reference taken before a tick is dangling
   after it. The test was written to hit this (a `q = w.quests.active.front()`
   write-through-dangling-reference produced a `Failed` gate on first run,
   diagnosed via `active.empty()` failing where it shouldn't have — see git
   history of this file). Not a module bug: `Quests.hpp` promises nothing
   about reference lifetime across a tick, and the sibling scenario
   (`test_scenario_debts.cpp`) never keeps such a reference alive across
   `advance_days()` either. Documented here since it's an easy trap for the
   next scenario author, not a doc/contract to fix.
4. **Complete** — before the deadline, `complete()` moves the id to
   `completed`; ticking on afterward changes nothing.
5. **Fail/timeout** — the same quest, ignored: the deadline day itself
   (day 5) is still live (per `Quests.cpp`'s documented semantics —
   `tick_quests` evaluates the *current*, pre-increment day each tick, so
   the tick that evaluates day 5 runs during the `advance_days` call that
   *ends* on day 6); the very next tick (day 6, the first day strictly past
   the deadline) fails it into `failed_list`, exactly once, verified across
   20 further days.
6. **Rewards/standing** — proven **absent**, deliberately, not invented:
   `quests.csv` carries no reward or standing column, and `Quests.hpp`'s
   own invariant is "writes ONLY QuestState." The test asserts
   `property.purse_by_owner` and `faction.standing_by_faction` are both
   still empty after completion. This is not a bug to fix; it is the
   contract working as specified — see **Missing kernel surfaces** below.
7. **Determinism** — the whole offer→accept→progress→complete arc,
   fingerprinted (active/completed/failed_list bytes), is identical across
   two runs from the same seed.

## Data-integrity sweep (second test in the same file)

`test_data_integrity_sweep_quests_and_dialogues()` walks every row of the
**live** `db/canon/quests.csv` (41 rows) and `db/canon/dialogues.csv` (48
rows) and checks:

- unique ids in both tables; every quest row has a known `act`
  (`opening`/`act_i`/`act_ii`/`act_iii`/`act_iv`) and a non-empty `giver`;
  every dialogue row has a non-empty `speaker`/`text`; `deadline_days`, where
  present, is a plain non-negative integer string;
- every `table:id` / `table.csv:id` cross-reference embedded in
  `source_ref` (against `items`, `events`, `customs`, `people`, `deities`,
  `factions`, `rites`, `ranks`, `endings`, `dialogues`, `quests`, `cities`)
  resolves to a real row in that table;
- every quest's `"deepens X[; Y[; Z]]"` prerequisite chain: every named
  quest id exists, no quest deepens one from a *later* act than its own, and
  the whole deepens graph is acyclic (DFS with a recursion-stack cycle
  check).

**Result: zero broken rows.** No missing cross-references, no out-of-order
or missing `deepens` targets, no cycles. The Act I/II/III/IV quest and
dialogue canon is clean as written. (Verified independently with a Python
prototype of the same checks against the raw CSVs before porting the logic
into the C++ test — same zero-findings result.)

The 7 pre-existing `OPEN` rows repo-wide (`calendar.csv` months/names/
festival days, `deities.csv`/`pantheons.csv` the eight of medicine,
`planetary_powers.csv` sun/saturn) are untouched by this sweep — none of
them are quests or dialogues, and none block this quest's own defs (the
Quests loader already excludes `OPEN` rows; see `src/Quests.cpp`
`row_is_open`).

## W2-A update (2026-09-25): the reward/standing pipes are closed

`kernel/include/sim/Actions.hpp` + `kernel/src/Actions.cpp`
(`kernel/tests/test_actions.cpp`) add `complete_quest()`/`fail_quest()`,
closing the "reward -> purse/inventory pipe" and "standing -> Faction pipe"
items below: `quests.csv` gained `reward_silver`/`reward_faction`/
`reward_standing` columns (INVENTED, filled for all 41 rows —
`the_priestess_debt` itself pays 35 silver + 5 standing with the new
`temple_of_sun_and_moon` faction row, closing the "temple isn't modeled as a
faction" data gap named below), and `complete_quest()` credits the purse and
adds standing from those columns; `fail_quest()` pays nothing. The dialogue
runner, journal/quest-log surface and `quests.csv` `city` column gaps remain
open (engine-side / data-only, out of this track's scope).

## Missing kernel surfaces (documented, not built — per instructions)

The slice quest works end-to-end as *state machine*; W2-A above closed the
reward/standing pipes. The rest remains, and nothing in `kernel/` yet
delivers or resolves it as *play*:

- **A dialogue runner.** `dialogues.csv` rows are static text with a
  `speaker`/`context`; nothing in the kernel presents them, branches them,
  or ties a dialogue line to offering/advancing/completing a quest. Per
  living-world.md §8, "how quests reach you: conversations, letters,
  rumour, the palace crier, temple oracles and omens" — all engine-side,
  undesigned here.
- **A journal/quest-log surface.** `QuestState::active[i].stage` is a bare
  `std::string` the module never reads or validates (confirmed by
  `Quests.cpp`: the machine "never branches on kind", and nothing branches
  on `stage` either). Nothing surfaces active quests, their stage text, or
  their remaining days to a player or an engine UI.
  `living-world.md §8`: "No floating quest markers by default... you mark
  your own map" — again engine-side.
  - **A reward → purse/inventory pipe.** `quests.csv` has no reward/silver/
  item column at all; `complete()` touches only `QuestState`. `Property.hpp`
  already has a purse (`credit_purse`, D-017) that a content author or an
  engine-side quest-resolution step could call on completion, but nothing
  wires `complete()` to it. Proven directly by this scenario (item 6 above).
- **A standing → Faction pipe.** Same gap for `add_standing`
  (`Faction.hpp`): completing a temple-adjacent quest like this one reads,
  narratively, like it should move standing with... something — but no
  faction id in `db/canon/factions.csv` represents "the temple of sun and
  moon" specifically (the closest canon factions are `the_empire`,
  `neo_sumerian_rebellion`, `the_barbarians`, `brotherhood_of_the_serpent`,
  `retributors_of_utu`, `dead_cult`, `sky_cult`, `eastern_barbarian_alliance`,
  `suti`, `southern_city_states_alliance` — none is "the temple"). This is a
  **data gap**, not just a missing pipe: even if `Quests`/`Faction` were
  wired together, this specific quest has no faction id to credit. Flagging
  for content authors, not resolving it (no OPEN row exists to resolve; the
  temple simply isn't modeled as a faction).
- **No `city` column on `quests.csv`.** The City-of-the-Moon placement of
  this whole Act I quest set is established only by prose in `source_ref`/
  giver text, not a foreign key to `cities.csv`. Fine for a single-city v1
  slice (D-002), but the data-integrity sweep above could not, and did not
  claim to, verify city placement structurally — only that referenced
  `cities:` tokens (none appear in quests.csv) resolve.

None of these are bugs in `Quests.*`; the module does exactly what its
contract says. They are the gap between "the state machine is correct" and
"a player can experience this quest," and are listed here per the task's
instruction to surface them without building them.

## Bugs found/fixed

None in `kernel/src/Quests.cpp` or `kernel/include/sim/Quests.hpp` — the
module's accept/complete/tick_quests behavior matches its contract and its
existing test suite (`tests/test_quests.cpp`) exactly; this scenario's one
early red run was a bug in the *test*, not the module (a dangling
`Quest&` held across a tick — see "Progress" above), fixed before this
report.
