# MODULE CONTRACT — Events

**Status:** Wave 1 agent. Trigger engine over WorldFacts (drought/war) with rules from events.csv (empty now).

**Owns:** EventsState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Events.cpp implements every declaration in include/sim/Events.hpp; tests/test_events.cpp passes; determinism holds (same seed -> same state bytes).

## Festival triggers (track K-2)

- `festival` (no argument): the day is any festival day — a named yearly festival (`festivals.csv`,
  via `Calendar::festivals`) or an anonymous absolute `festival_days` entry.
- `festival:<id>`: the day is that named festival (`ctx.cal.festival_id(day) == id`). An unknown id
  never matches.
- Now that `WorldState::init` loads `festivals.csv`, the canon's festival-gated rows
  (`festival_pickpockets`) are live on the four festival days rather than dormant (D-013's
  "authored-but-dormant until the calendar decision" — the calendar content was filled `INVENTED`
  under D-018).

Test: `tests/test_events.cpp` `test_named_festival_trigger`.
