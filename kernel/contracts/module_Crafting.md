# MODULE CONTRACT — Crafting

**Status:** T4 — the first crafting chain (grain -> bread and beer). rpg-systems §6: cooking stations are the quern, the bread oven and the brewing vat; recipes come only from the bible §3 food lists (db/canon/foods.csv barley_bread / barley_beer) and are tagged.

**Owns:** nothing persistent — it is a pure interpreter of db/canon/recipes.csv against a caller-owned `Inventory`. No `CraftingState`, no Rng, no wall clock.
**May read:** db/canon/recipes.csv, db/canon/items.csv (through `Db`, passed directly — this module has no `WorldContext` dependency).
**Must never:** hold its own state across calls; apply partial effects on a failed craft; resolve an OPEN row (`load_recipes` skips them); touch engine code.

**API:**
- `load_recipes(db)` — every non-OPEN recipes.csv row, sorted by id.
- `can_craft(db, inv, recipe_id, stations_at_hand, times)` — pure predicate.
- `craft(db, inv, recipe_id, stations_at_hand, times, reason)` — checks then atomically applies; `false` + `*reason` and an untouched `inv` on any failure (unknown/OPEN recipe, missing station, insufficient inputs, `times <= 0`).

**Definition of done:** src/Crafting.cpp implements every declaration in include/sim/Crafting.hpp; tests/test_crafting.cpp passes (bread chain, beer chain, refusal-with-no-partial-effects for both missing input and missing station, every recipe's items resolve in items.csv, determinism); every db/canon/recipes.csv row is tagged and sourced (canon_lint.py); `python3 tools/export_datatables.py` still runs unchanged (recipes is not added to its TABLES — a coordinator decision).
