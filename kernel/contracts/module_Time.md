# MODULE CONTRACT — Time

**Status:** IMPLEMENTED (Wave 0). The calendar spine.

**Owns:** Date/DayNumber math, season lookup, festival days
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Time.cpp implements every declaration in include/sim/Time.hpp; tests/test_time.cpp passes; determinism holds (same seed -> same state bytes).
