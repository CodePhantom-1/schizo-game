# items

**Rows:** 66 · **Source:** canon rows authored from the designer's notes ([world-bible](../../docs/world-bible.md)), researched real-world rows, or reserved for later phases.

## Fields

```
id,name,category,culture,material,producer,price_band,tag,source_ref,ui_category,weight_g,stack_max,spoil_days,flags
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref` (`wb §x` and/or `notes L<n>` — [db/sources/notes.md](../sources/notes.md); `A` rows name their real-world source; `INVENTED` rows are authored glue shown to the player as such).
- `OPEN` rows mark missing canon; they cannot ship.
- **The physical columns (FND-02, P0a; `canon_lint.py` enforces them):** `ui_category` is one of food, drink, ingredient, material, tool, weapon, ammunition, armour, shield, clothing, ritual, document, trade_good, station_part, animal, medicine, treasure; `weight_g` is grams per unit (integer ≥ 0; arms and armour copy `arms.csv`); `stack_max` (integer ≥ 1); `spoil_days` (integer ≥ 0, 0 = never spoils); `flags` is `|`-joined from bound, document, floats, fixture, animal, container[:<grams>]. `category` stays: the needs table reads it. The values are INVENTED glue, listed in [docs/proposals/invented-ledger-items.md](../../docs/proposals/invented-ledger-items.md).
