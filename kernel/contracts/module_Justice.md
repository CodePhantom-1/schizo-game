# MODULE CONTRACT — Justice

**Status:** Wave 1 agent. Crime -> witness -> hearing -> verdict; detention, never prisons; laws.csv fallback = compensation.

**Owns:** JusticeState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Justice.cpp implements every declaration in include/sim/Justice.hpp; tests/test_justice.cpp passes; determinism holds (same seed -> same state bytes).
