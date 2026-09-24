# MODULE CONTRACT — World

**Status:** Wave 2 (coordinator). Aggregates all states; owns the deterministic daily tick and the save file.

**Owns:** tick order (Context.hpp), WorldState::init/advance_days
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/World.cpp implements every declaration in include/sim/World.hpp; tests/test_world.cpp passes; determinism holds (same seed -> same state bytes).
