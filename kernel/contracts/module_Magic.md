# MODULE CONTRACT — Magic

**Status:** Wave 1 agent. The five powers of a rite (favour/knowledge/materials/purity+place/time); the fixed success formula is in the header.

**Owns:** MagicState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Magic.cpp implements every declaration in include/sim/Magic.hpp; tests/test_magic.cpp passes; determinism holds (same seed -> same state bytes).
