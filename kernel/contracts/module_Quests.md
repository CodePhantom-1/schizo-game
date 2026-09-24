# MODULE CONTRACT — Quests

**Status:** Wave 1 agent. The quest state machine: accept/complete/fail-on-deadline; defs from quests.csv (empty now).

**Owns:** QuestState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Quests.cpp implements every declaration in include/sim/Quests.hpp; tests/test_quests.cpp passes; determinism holds (same seed -> same state bytes).
