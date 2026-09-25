# SCENARIO CONTRACT — Crime cycle (T5)

**Test:** `kernel/tests/test_scenario_crime.cpp`
**Slice ask (docs/plan.md §7):** "guards + one full crime cycle (theft → arrest → hearing → verdict tablet)".

## What passes today

Driven against a real `WorldState` (canon loaded, `../db/canon`), no fixture data:

- A theft committed in the City of the Moon is witnessed by `the_prophet`
  (db/canon/people.csv — the only named canon NPC seeded there); the witness
  records it via `Population::witness` (memory entry, ordered by day).
- The fact spreads along a `knows` edge one hop per tick
  (`Population::tick_population`), confirmed NOT to have hopped on the tick
  that also holds the hearing, and confirmed to have hopped on the next tick.
  (No canon social links exist yet — D-011 — so the test wires the edge
  itself, the same pattern `test_world.cpp`'s rumour test and
  `test_population.cpp` use.)
- The crime enters the docket via `Justice::report_crime` and is heard the
  same tick it is witnessed (`Justice::tick_justice` — alarm → pursuit →
  detention → hearing collapse to one step at Wave 1, per the scope note in
  `src/Justice.cpp`).
- The verdict is read from the real `db/canon/laws.csv` row, not the
  "compensation" fallback: proven with `burglary`, whose real first
  `penalty_options` entry is `death` — a value the fallback could never
  produce, so a passing assertion is proof the law table decided it, not
  coincidence. `theft`'s real first option happens to equal the fallback
  value, so the test checks its `tag` is `A`/`CANON` (shipped, not `OPEN`)
  and computes the expected value from the row instead of hardcoding it.
- The verdict is visible via state (`WorldState::justice.verdicts`,
  count and content).
- The unwitnessed path: a theft with `witnessed_by == ""` stays on
  `open_crimes` through 30 ticks; no NPC ever gains a memory of it; a
  hearing must be requested explicitly (`hold_hearing`) once evidence
  surfaces — nothing in the tick loop does that on its own.
- Determinism: the same seed running the identical script (witness → wire
  edge → report two crimes → advance 3 days) twice produces byte-identical
  `JusticeState` and `PopulationState` serializations.
- Faction/rank interaction: checked explicitly and found absent at kernel
  level — `standing(w.faction, "the_empire")` is unchanged by a hearing that
  seals a verdict. This matches `Justice.hpp`'s contract ("Must never: write
  another module's state") and is not a bug; it is the missing surface
  below.

## W2-A update (2026-09-25): gaps 1-6 closed

`kernel/include/sim/Actions.hpp` + `kernel/src/Actions.cpp` (tested by
`kernel/tests/test_actions.cpp`) now provide the orchestration verbs this
document asked for: `commit_crime()` (witness -> alarm -> pursuit ->
detention, closing gaps 1, 2 and 6) and `hold_crime_hearing()` (sealed
verdict tablet id, compensation paid from/to the purse with escalation on
non-payment, standing loss and outlawry with the jurisdiction's faction —
closing gaps 3, 4 and 5). See `docs/proposals/invented-ledger-actions.md`
for every INVENTED constant/mapping this introduced (the hearing delay, the
flat compensation amount, the city->faction jurisdiction map, the standing
severity ladder). The original gap list below is kept as the historical
record of what T5 found missing; each gap now links to its closure.

## What the slice still lacks at kernel level (T5's original finding)

Each of these had **no kernel surface** at T5 (T5 was scenario + report
only, per that track's assignment); W2-A closed all six (see above).
Contract/doc reference for each: 

1. **Alarm → pursuit → detention as distinct stages.** `Justice.hpp`'s own
   comment names all four ("crime → witness → alarm → pursuit → detention →
   hearing → sealed verdict"), but `JusticeState`/`Crime`/`Hearing` carry no
   field for any stage before the hearing, and `src/Justice.cpp`'s scope
   note says outright: "the frozen API and JusticeState carry no detention
   surface... this file implements the witnessed crime's path from the
   docket to the sealed verdict." A crime is witnessed and heard in the same
   tick call; there is no window in which the player could flee, be
   pursued, or be detained. — D-011 ("detention/pursuit/outlawry surfaces
   and their rank interaction" listed as DEFERRED); `docs/mechanics.md` row
   12 lists "guards, pursuit, detention" as part of the unchanged imported
   pipeline, still `Open` content.

2. **Guard response / who reports the crime.** `report_crime` takes
   `witnessed_by` directly; nothing in the kernel models a guard NPC
   noticing, being told, or choosing to act. `witness()` only writes to the
   witnessing NPC's own memory — it does not auto-file a `Crime`. The
   engine (or a future kernel surface) has to bridge "an NPC remembers a
   theft" to "a crime is on the docket." No contract currently owns this
   bridge.

3. **The sealed verdict TABLET as an item.** `docs/plan.md §7` names it
   explicitly ("verdict tablet"); `Hearing` has no item/tablet field at all,
   and the frozen `Justice.hpp` header comment says plainly "the sealed-
   tablet ITEM is engine content" — i.e. this is scoped to the engine layer,
   not the kernel, by design. Items exist as a canon table
   (`db/canon/items.csv`) and `Property::Asset.deed_tablet_id` shows the
   established pattern (an item id string field) that a verdict tablet would
   need on `Hearing` if the kernel ever owns it instead.

4. **Compensation paid from the purse.** D-017 built `PropertyState::purse`
   (`purse_by_owner`, `credit_purse`, `take_from_purse`) precisely as the
   deferred "property-income surface," but `hold_hearing` never calls it: a
   `"compensation"` verdict changes no one's silver. `Justice.hpp`'s "Must
   never: write another module's state" forbids Justice from calling
   `credit_purse`/`take_from_purse` itself under the current contract — this
   needs either a documented cross-module call (a coordinator-owned
   exception, like `World.cpp`'s tick order) or an engine-layer step that
   reads a sealed `"compensation"` verdict and moves silver itself.

5. **Verdict → faction standing / rank.** Confirmed by this scenario test
   (`standing()` unchanged post-hearing). `docs/mechanics.md` row 12 names
   "outlawry → rank 0" as part of the unchanged imported crime pipeline;
   `Faction.hpp` owns `standing_by_faction` and `tier_of()` (the rank/tier
   ladder) but nothing connects a `"death"`/`"exile"` verdict to either. Per
   D-011 this is explicitly deferred ("detention/pursuit/outlawry surfaces
   **and their rank interaction**"). No contract currently specifies which
   verdicts should move standing, by how much, or whether exile/death should
   force `tier_of()` toward 0 (Stranger) directly rather than through
   ordinary standing decay.

6. **A "commit crime" / perception verb.** The scenario test drives
   `Population::witness` and `Justice::report_crime` directly because
   nothing in the kernel connects "the player takes an item that is not
   theirs" to either call. This is expected — Economy/Property own item
   ownership and neither module's contract mentions filing a `Crime` — but
   it means the full cycle currently has no single kernel entry point; the
   engine (or a new coordinator-owned glue module) has to call both APIs
   itself when a theft happens.

None of the above resolves an `OPEN` canon row or invents content; laws.csv
itself is fully shipped (`A`-tagged, real Mesopotamian codes, D-013) for
every crime kind this test exercises. The gap is entirely kernel machinery,
matching D-011's own triage.
