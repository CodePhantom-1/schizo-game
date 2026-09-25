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
