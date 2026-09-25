# recipes

**Rows:** 8 · **Source:** canon rows authored from the designer's notes ([world-bible](../../docs/world-bible.md)), researched real-world rows, or reserved for later phases.

## Fields

```
id,name,inputs,outputs,station,hours,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref` (`wb §x` and/or `notes L<n>` — [db/sources/notes.md](../sources/notes.md); `A` rows name their real-world source; `INVENTED` rows are authored glue shown to the player as such).
- `OPEN` rows mark missing canon; they cannot ship.
- `inputs` and `outputs` are `item:qty;item:qty` lists; every item id must exist in [items.csv](../canon/items.csv).
- `station` is the item id of the station the recipe is crafted at (a row in items.csv, `category = station`).
- `hours` is the in-world time the craft takes (0 = instant).
