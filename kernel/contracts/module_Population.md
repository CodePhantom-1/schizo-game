# MODULE CONTRACT — Population

**Status:** Wave 1 agent. Named NPCs, memory, and the rumour graph (one hop per tick).

**Owns:** PopulationState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Population.cpp implements every declaration in include/sim/Population.hpp; tests/test_population.cpp passes; determinism holds (same seed -> same state bytes).
