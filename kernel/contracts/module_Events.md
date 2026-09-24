# MODULE CONTRACT — Events

**Status:** Wave 1 agent. Trigger engine over WorldFacts (drought/war) with rules from events.csv (empty now).

**Owns:** EventsState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Events.cpp implements every declaration in include/sim/Events.hpp; tests/test_events.cpp passes; determinism holds (same seed -> same state bytes).
