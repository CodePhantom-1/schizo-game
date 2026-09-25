# people

**Rows:** 39 · **Source:** canon rows authored from the designer's notes ([world-bible](../../docs/world-bible.md)), researched real-world rows, or reserved for later phases.

## Fields

```
id,name,role,city,faction,home_place,work_place,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref` (`wb §x` and/or `notes L<n>` — [db/sources/notes.md](../sources/notes.md); `A` rows name their real-world source; `INVENTED` rows are authored glue shown to the player as such).
- `OPEN` rows mark missing canon; they cannot ship.
- `home_place` / `work_place` (K-2): `places.csv` ids that a person's schedule `home`/`work` tokens resolve to. Blank when the person lives or works off the slice street, and on the named leaders.
