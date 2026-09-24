# foods

**Rows:** 0 · **Source:** canon rows authored from the designer's notes ([world-bible](../../docs/world-bible.md)) or reserved for later phases.

## Fields

```
id,name,kind,ingredients,method,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `OPEN`) and `source_ref` (`wb §x` and/or `notes L<n>` — [db/sources/notes.md](../sources/notes.md)).
- `OPEN` rows mark missing canon; they cannot ship.
- `A` rows cite real-world sources in `source_ref`.
