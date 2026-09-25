# SEAM CHANGELOG — W2-I

Track W2-I wires the T3 (Needs) and T4 (Crafting) modules into `WorldState`,
the save file, and the C API. Owner of the seam this wave: `World.hpp/.cpp`,
`Context.hpp`, `CApi.h/.cpp`, `Snapshot.hpp/.cpp` only. Additive throughout —
no existing declaration changed signature or removed behaviour.

## World.hpp / World.cpp

- `WorldState` gains `NeedsState needs;` and
  `std::map<Id, Inventory> inventories;` (actor id -> `Inventory`; "player"
  plus any npcs the engine chooses to track).
- `WorldState::init` resets both and seeds `inventories["player"]` to an
  empty `Inventory{}` — the prisoner start (D-009: arrives with nothing).
- `WorldState::context()` passes both through to `WorldContext` (see below).
  Neither participates in the fixed daily tick order (`advance_days`); the
  engine advances needs by the hour through the C API
  (`sim_world_advance_needs`), independent of the day tick.

## Context.hpp

- `WorldContext` gains two const-ref members, appended at the end of the
  struct (so existing positional-init call sites need only append, not
  reorder): `const NeedsState& needs;` and
  `const std::map<Id, Inventory>& inventories;`. Includes `sim/Needs.hpp` and
  `sim/Crafting.hpp` (for `Inventory`).
- Every hand-built `WorldContext{...}` aggregate-init call site across the
  test suite (`test_contracts.cpp`, `test_economy.cpp`, `test_events.cpp`,
  `test_faction.cpp`, `test_justice.cpp`, `test_magic.cpp`,
  `test_population.cpp`, `test_property.cpp`, `test_quests.cpp`) was updated
  to add a `NeedsState needs;` / `std::map<Id, Inventory> inventories;` pair
  and pass them at the end of the init list — required for those files to
  keep compiling, not a behaviour change.

## Snapshot.hpp / Snapshot.cpp

- Two new additive sections written after `QUESTS_FAILED`, so the `SIMSAVE 1`
  format did not need a version bump:
  - `NEEDS\t<n>` then `n` rows of `actor\thunger\tthirst\tfatigue`.
  - `INVENTORIES\t<n>` then `n` actor blocks, each `actor\t<item-count>`
    followed by that many `item\tqty` rows.
- `load_world` is now atomic: it parses into a local `WorldState` and only
  `std::move`s it into the caller's `WorldState&` once every section has
  parsed without throwing. A truncated/malformed save now throws with the
  caller's live world left byte-for-byte untouched (previously `w.init()`
  ran on the live reference immediately, so a failure partway through left
  it reset-but-not-restored). Covered by
  `test_load_world_is_atomic_on_failure` in `tests/test_snapshot.cpp`.

## CApi.h / CApi.cpp (additive; same conventions as the existing surface)

- **Save/load:** `sim_world_save`/`sim_world_load` (file path), plus the
  cheap buffer variants `sim_world_save_to_buffer`/`sim_world_load_from_buffer`
  (the save is already an in-memory string, so a buffer round-trip needs no
  extra copy logic).
- **Needs:** `sim_world_hunger`/`thirst`/`fatigue` (read-only getters, -1 on
  null, 0 for an actor never seen — matching `needs_of`'s zero-default
  without mutating the world through a const pointer);
  `sim_world_advance_needs`; `sim_world_eat`/`sim_world_drink` (consume one
  unit from the actor's own `inventories[actor]` unless the item is
  `"water"` for drink, which Needs already treats as free; error codes
  documented in `CApi.h`: -1 null, -2 not enough in inventory, -3 not
  edible/drinkable per `Needs.cpp`'s categories); `sim_world_need_effects`
  (semicolon-joined, buffer convention).
- **Inventory:** `sim_world_item_count`, `sim_world_give_item` (qty may be
  negative; clamps at 0; returns the resulting count).
- **Crafting:** `sim_world_craft` crafts from the actor's own inventory via
  `Crafting.hpp`'s `craft()`; the station list is `;`-separated. Error codes
  map `craft()`'s reason-string prefixes (owned by the Crafting module, not
  this seam) to negative codes: -2 unknown/OPEN/malformed recipe, -3 missing
  station, -4 insufficient inputs. Water needs no special handling here —
  Crafting.cpp already treats it as free well-water infrastructure.
- **Schedule:** `sim_world_task_at(world, role, hour, out, cap)` — today's
  task for `role` at `hour`, via `Schedule.hpp`'s `task_at()`.
- No `sim_world_advance_hours`: the day tick stays daily; the engine owns
  sub-day hours by calling `sim_world_advance_needs` directly.

## Tests

- `tests/test_capi.cpp`: 7 new test functions covering needs, inventory,
  eat/drink error codes, the grain -> barley_flour -> bread crafting chain
  + eating through the C API alone, `task_at`'s buffer/absent-role path, and
  a save -> load -> continue determinism check (file path, plus a buffer
  round-trip) entirely through `extern "C"`.
- `tests/test_snapshot.cpp`: `test_load_world_is_atomic_on_failure` (new).

# SEAM CHANGELOG — K-1 (rites: knowledge, offerings, effects)

Additive throughout — no existing declaration changed signature or removed
behaviour; the fixed Magic success formula is untouched.

## Magic.hpp / Magic.cpp

- `MagicState` gains `std::set<Id> known_rites;` (appended last) and two free
  functions `knows_rite()` / `learn_rite()`. `perform_rite` is unchanged and
  still writes only MagicState; the caller fills
  `RiteInputs::performer_knows_rite` from `knows_rite()`.

## World.hpp / World.cpp

- New header `sim/RiteEffects.hpp` (state only: `Ward`, `Omen`,
  `RiteEffectsState`), included by World.hpp.
- `WorldState` gains `RiteEffectsState rite_effects;` (reset by `init`). Not
  part of the daily tick; not in `WorldContext` (Context.hpp untouched).

## Rites.hpp / Rites.cpp (new)

- The caller-side verb layer (like Actions.hpp: a WorldState& file allowed to
  write MagicState — only through Magic's own free functions —,
  `inventories` and `rite_effects`): `learn_rite_from_teacher`,
  `learn_rite_from_text`, `rites_taught_by`, `rites_taught_in`,
  `perform_rite_in_world`, `ward_holds`. See module_Magic.md.

## Db.cpp

- The explicit table list gains `rite_teachings` (new INVENTED canon table,
  `db/canon/rite_teachings.csv`, schema `db/schema/rite_teachings.md`).

## Snapshot.cpp

- Three trailing sections after `INVENTORIES`: `MAGIC_KNOWN\t<n>` (rite ids),
  `RITE_WARDS\t<n>` (`place\trite\tlaid\tuntil`), `RITE_OMENS\t<n>`
  (`day\trite\tsubject\tsign\tconfidence_pct`). Optional on load: a save that
  ends after `INVENTORIES` (written before K-1) loads with nothing known, no
  ward and no omen. A save cut inside the new sections still throws. No
  version bump (`SIMSAVE 2`).

## CApi.h / CApi.cpp (additive)

- Rite knowledge: `sim_world_knows_rite`, `sim_world_known_rites`,
  `sim_world_learn_rite_from_teacher`, `sim_world_learn_rite_from_text`,
  `sim_world_rites_taught_by`, `sim_world_rites_taught_in`.
- Performer: `sim_world_set_rite_place`, `sim_world_rite_place`,
  `sim_world_purity`, `sim_world_set_purity` (clamps 0..100).
- The rite: `sim_world_perform_rite(world, rite, target, effect_out, cap)` —
  1 succeeded / 0 failed / negative refusal codes (documented in CApi.h).
- Effects: `sim_world_ward_until`, `sim_world_warded`,
  `sim_world_omen_count`, `sim_world_omen`.

## Tests

- test_magic `test_rite_knowledge_state`; test_scenario_rite `test_k1_*` (6);
  test_snapshot `test_k1_rite_state_round_trips` (+3 cases in the
  every-module guard); test_capi `test_rites_through_c` (listed first so it
  runs ahead of the path-dependent save test).

---

# SEAM CHANGELOG — K-2 (festivals + per-person schedules)

Additive throughout — no existing C API signature changed; no new save state.

## Db.cpp

- The loader's table list gains `festivals` and `person_schedules`.

## Time.hpp / World.cpp

- `FestivalDef` + `CalendarConfig::festivals`; `Calendar::festival_on`, `festival_id`,
  `festivals()`; `is_festival` also true on a yearly festival's day_of_year (module_Time.md).
- `WorldState::init` loads `db/canon/festivals.csv` into the calendar. Magic's festival time power
  and Events' `festival` trigger are therefore live against canon (no change to Magic).

## Schedule.hpp / Population

- `ScheduledTask` gains `place`, `festival`, `origin` (defaulted members).
- New: `FestivalBend`, `festival_bend`, `market_open`, `person_day_plan`, `person_task_at`
  (module_Schedule.md). `day_plan`/`task_at` now honour festival rows and the gathering window.
- `npc_task_at` (Population.hpp, signature unchanged) now resolves per person: role + the
  person's `person_schedules.csv` rows + season + festival, with `place` resolved to a places.csv id.

## Snapshot

- Nothing new to save: festivals and plans are pure over canon + day, and the calendar is
  rebuilt by `init` inside `load_world`. `tests/test_festivals.cpp` checks that a restored world
  answers every npc/hour identically across a festival.

## CApi.h / CApi.cpp (additive; same buffer conventions)

- **Festivals:** `sim_world_is_festival(world)` (1/0/-1), `sim_world_is_festival_day(world, day)`,
  `sim_world_festival(world, out, cap)` (today's festival id, "" when none),
  `sim_world_festival_on(world, day, out, cap)`, `sim_world_market_open(world)` (0 only when
  today's festival closes the market).
- **People:** `sim_world_npc_id(world, index, out, cap)` enumerates `0..sim_world_npc_count-1`;
  `sim_world_npc_task_at` / `sim_world_npc_place_at` / `sim_world_npc_schedule_at(world, npc,
  hour, out, cap)` answer "what / where / which row is person X at hour H today" (-1 for a null
  argument, an unknown npc, or an npc with no schedule — the three named leaders).

## Data (db/canon)

- New `festivals.csv` (4 rows), new `person_schedules.csv` (4 rows); `schedules.csv` gains
  `festival` + `place` columns (+4 festival rows); `people.csv` gains `home_place` + `work_place`
  (blank on the 3 CANON rows). All new content `INVENTED`, listed in
  docs/proposals/invented-ledger-festivals.md.
- `tools/export_datatables.py` exports `festivals` and `person_schedules` too.

## Tests

- `tests/test_festivals.cpp` (new), plus additions to `test_time.cpp`, `test_schedule.cpp`,
  `test_events.cpp`.

# Kernel review follow-up + D-022 (2026-09-25)

## Rng.hpp (additive)

- `Rng::below(n)`: an unbiased uniform integer in `[0, n)` (rejection sampling), pure integer
  maths. `int_in` keeps its modulo draw because Events' existing rolls depend on it.
- `sim::stable_hash(std::string_view)`: FNV-1a 64, the same on every standard library, for
  `fork()` salts.

## Magic.hpp

- D-022: the fixed formula is now integer basis points with one roll per rite
  (module_Magic.md). `RiteResult` gains `int score_bp`; `double score` is kept as
  `score_bp / 10000.0`. Outcomes for a given seed and day changed, so tests that pinned exact
  draws were re-derived.

## CApi.h / CApi.cpp

- Every K-1/K-2 entry point and `sim_world_destroy` now catches everything and returns its
  documented error value. `sim_world_omen_count`'s -1 and `sim_world_destroy`'s no-throw are
  now documented.

# SEAM CHANGELOG — W4-A (character progression)

Contract: module_Character.md. New files: `sim/Character.hpp` + `src/Character.cpp` (the pure
sheet), `sim/Progression.hpp` + `src/Progression.cpp` (the world verbs). Every edit to a shared
file is a labelled "W4-A" addition; no module contract changed.

## Db.cpp

- Table list gains `attributes`, `skill_teachings`, `work_roles`.

## World.hpp / World.cpp

- `WorldState` gains `progression` (catalog, rebuilt by `init`, never saved), `character` (the
  player's sheet), `npc_sheets` (light sheets). `init` builds all three after `seed_people`.

## Actions.cpp / Rites.cpp (caller-side hooks)

- `commit_crime`: an unwitnessed deed grows the criminal's stealth (player only).
- `hold_crime_hearing`: an exile/death verdict also strips the player's rank with that polity.
- `perform_rite_in_world`: a performed rite grows its tradition's Sacred skill; a successful
  offering/hymn's favour is scaled by `rite_favour_bp` (0 by default: K-1 numbers unchanged).

## Snapshot.cpp

- `Reader::peek_tag()` (new): detects an optional trailing section by its tag.
- Trailing optional `CHAR_CORE`, `CHAR_ATTRS`, `CHAR_SKILLS`, `CHAR_TALENTS`, `CHAR_RANKS`,
  `CHAR_NPCS`; absent in a pre-W4-A save, which loads as a fresh level-1 prisoner.

## CApi.h / CApi.cpp (additive)

- `SimWorld` gains `w4a_refusal`. `sim_world_craft` (success) grows the station's craft skill;
  `sim_world_eat` (success, when hungry) grows survival.
- New: the "Character progression (W4-A)" block (30 functions; see module_Character.md).

## Data / tools

- New tables `attributes.csv`, `skill_teachings.csv`, `work_roles.csv`; `skills.csv`,
  `talents.csv`, `callings.csv` filled (headers extended); `items.csv` +4, `recipes.csv` +3.
  All INVENTED, ledgered in docs/proposals/invented-ledger-character.md.
- `tools/export_datatables.py` exports attributes, callings, talents, skill_teachings, work_roles.

## Tests

- `tests/test_character.cpp` (new), `tests/test_scenario_progression.cpp` (new: the D-021
  reachability proof through the C API alone).

# SEAM CHANGELOG — W4-C (the wild lands)

Additive throughout; every edit to a shared file sits in a labelled `W4-C` block. Full contract: [module_Wild.md](module_Wild.md).

- **World.hpp / World.cpp:** `WorldState` gains `WildState wild;` (include `sim/Wild.hpp`). `init` calls `init_wild(db, wild)` after the quest defs load, and `advance_days` calls `tick_wild(*this)` after `hold_due_hearings`. `tick_wild` is world-orchestrated like the hearings: it writes Economy (`consume`/`deliver`), Population (`witness`), Events (`fired` + `fire_count_by_rule`), Quests, Justice (`commit_crime`), Property (purse) and Faction (`add_standing`) only through their public APIs. Faction.cpp is untouched. `WorldContext` is unchanged.
- **Db.cpp:** seven new canon tables load: `wild_places`, `wild_links`, `wild_groups`, `wild_encounters`, `caravans`, `transport_modes`, `weather`.
- **Snapshot.cpp:** `Reader::peek_tag()` is added, and one trailing optional section `WILD\t<n>` is written after the K-1 sections and read when present (in any order among trailing sections). Pre-W4-C saves load unchanged.
- **CApi.h:** `#include "sim/CApiWild.h"` (43 new functions, add-only; implemented in `src/CApiWild.cpp`, every entry point guarded by try/catch like CApi.cpp).
- **tools/export_datatables.py:** the seven tables export to `data/ue/`.
- **tests/test_scenario_crime.cpp:** `test_unwitnessed_theft_stays_open` now asserts that no memory concerns the criminal, instead of that no memory exists at all: the city now carries rumours of bandit camps from day 1.
