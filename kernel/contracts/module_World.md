# MODULE CONTRACT — World

**Status:** Wave 2 (coordinator). Aggregates all states; owns the deterministic daily tick and the save file.

**Owns:** tick order (Context.hpp), WorldState::init/advance_days
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/World.cpp implements every declaration in include/sim/World.hpp; tests/test_world.cpp passes; determinism holds (same seed -> same state bytes).

## W2-I update

`WorldState` gains `NeedsState needs;` and `std::map<Id, Inventory>
inventories;` (actor id -> Inventory). Neither is part of the fixed daily
tick order (Context.hpp) — the engine advances needs by the hour through the
C API, not through `advance_days`. `init()` seeds `inventories["player"]` to
an empty `Inventory{}` (the prisoner start, D-009). Both are exposed on
`WorldContext` as const refs (`needs`, `inventories`, appended at the end of
the struct) so any module can read an actor's state as of the same morning if
it ever needs to; nothing currently does. See `kernel/contracts/seam_changelog.md`
for the full W2-I diff (Snapshot + CApi included).
