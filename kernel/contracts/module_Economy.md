# MODULE CONTRACT — Economy

**Status:** Wave 1 agent. Prices and stocks per city; grain is the base good; silver by weight.

**Owns:** EconomyState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Economy.cpp implements every declaration in include/sim/Economy.hpp; tests/test_economy.cpp passes; determinism holds (same seed -> same state bytes).
