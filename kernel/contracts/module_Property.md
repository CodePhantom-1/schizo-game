# MODULE CONTRACT — Property

**Status:** Wave 1 agent. Assets, deeds, silver loans at 20%/yr; steward report lines.

**Owns:** PropertyState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Property.cpp implements every declaration in include/sim/Property.hpp; tests/test_property.cpp passes; determinism holds (same seed -> same state bytes).
