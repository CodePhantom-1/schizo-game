# MODULE CONTRACT — Faction

**Status:** Wave 1 agent. Standing 0-100, tiers, oaths, the one-Great-King rule, the oath-breaker curse flag.

**Owns:** FactionState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Faction.cpp implements every declaration in include/sim/Faction.hpp; tests/test_faction.cpp passes; determinism holds (same seed -> same state bytes).
