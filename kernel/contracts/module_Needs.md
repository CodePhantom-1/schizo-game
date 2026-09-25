# MODULE CONTRACT — Needs

**Track:** T3 — the body's needs (hunger, thirst, fatigue).
**Imported mechanic:** docs/mechanics.md rows 20 and 28 -> parent `../docs/rpg-systems.md`
§1.2 (Conditions) and §6 (Food and eating).

**Scope.** §1.2 lists hunger, thirst, fatigue, heat, cold, zonal wounds, illness,
purity and intoxication as the full body model. This module is **only**
hunger/thirst/fatigue, per the coordinator's required surface — heat/cold,
wounds, illness, purity and intoxication are separate future tracks and are
not touched here.

**Owns:** `NeedsState` only.
**May read:** `Db` (items.csv, read-only, for eat/drink category lookups).
**Must never:** write another module's state; use wall-clock time or
`std::rand`; iterate an unordered container into an output; resolve an OPEN
row; touch engine code.

**The engine owns the clock.** `advance_needs` takes a whole number of
game-hours from the caller; the module has no internal timer.

## Surface

```cpp
Needs& needs_of(NeedsState& s, const Id& actor);
void advance_needs(NeedsState& s, const Id& actor, int hours, bool sleeping, double severity = 1.0);
bool eat(const Db& db, NeedsState& s, const Id& actor, const Id& item_id, std::string* reason = nullptr);
bool drink(const Db& db, NeedsState& s, const Id& actor, const Id& item_id, std::string* reason = nullptr);
std::vector<std::string> need_effects(const Needs& n);
```

## What is edible / drinkable

items.csv has no nutrition column, so eligibility is keyed off its
`category` column (the finest-grained thing canon actually gives us):

| category | eat? | drink? | example id |
|---|---|---|---|
| `staple` | yes | no | grain, wheat |
| `staple food` | yes | no | dates |
| `food and offering material` | yes | no | fish |
| `ration` | yes | no | drought_ration_measure |
| `staple drink` | no | yes | beer |
| everything else | no | no | gold, wool, tools, ritual goods… |

`water` is a special virtual id, always drinkable, **not a canon/items.csv
row** — rpg-systems §6 treats potable water as infrastructure, not a
tradeable good (no items.csv row exists for it and none should be invented;
the coordinator said water "may be treated as always drinkable — document
it," and this is that documentation). `drink(db, s, actor, "water")` never
touches `db`.

Unknown item id -> refused, `reason = "unknown_item"`.
Item exists but wrong category for the call -> refused, `reason = "not_food"`
or `"not_drink"`.

## INVENTED constants (no canon/parent numbers exist for any of these)

- Hourly climb while awake: hunger +2, thirst +3, fatigue +4. Sleep: fatigue
  −12/h, hunger/thirst keep climbing (you don't eat or drink in your sleep).
- Per-category restore on eat: food and offering material 35, staple 30,
  ration 25, staple food 20 (roughly: fresher/more caloric canon foods
  restore more — fish and grain outrank the date-syrup snack).
- Per-category restore on drink: staple drink (beer) 20 thirst, **and** 5
  hunger — rpg-systems §6: "Beer and wine give some water and calories."
  Water: 40 thirst, 0 hunger.
- Effect thresholds (`need_effects`): hunger ≥40 "hungry", ≥75 "starving";
  thirst ≥40 "parched"; fatigue ≥70 "exhausted". The parent doc names the
  conditions (§1.2, §6 "hunger becomes the whole game" in famine) but gives
  no numbers.
- `severity` multiplier on `advance_needs` (default 1.0): the accessibility
  needs-severity toggle named at mechanics row 37 / rpg-systems §12. The
  parent doc names the toggle, not its range or default, so this module only
  assumes 1.0 = the designed rate; it scales the waking hunger/thirst/fatigue
  climb, never the sleep restore rate.

Not modeled here (future tracks, per §1.2): heat/cold doubling water needs
in summer, diet-balance ("Well-fed"/"Weakness"), spoilage, illness, purity,
intoxication, cooking stations, feasting, offerings.

## Definition of done

`src/Needs.cpp` implements every declaration in `include/sim/Needs.hpp`;
`tests/test_needs.cpp` passes: hourly decay, sleep restores fatigue faster
than waking tires, eat/drink reduce the right meter, refusal of non-food/
non-drink and unknown ids, effect thresholds (including all four firing
together, sorted), clamping at 0 and 100, per-actor independence, and
determinism (two independently-advanced `NeedsState`s from the same inputs
land byte-identical).
