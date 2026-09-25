# schedules

**Rows:** 36 · **Source:** canon rows authored from the designer's notes ([world-bible](../../docs/world-bible.md)), researched real-world rows, or reserved for later phases.

## Fields

```
id,role,hour,task,season,festival,place,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref` (`wb §x` and/or `notes L<n>` — [db/sources/notes.md](../sources/notes.md); `A` rows name their real-world source; `INVENTED` rows are authored glue shown to the player as such).
- `OPEN` rows mark missing canon; they cannot ship.
- `festival` (K-2): blank = an ordinary row; a `festivals.csv` id = only on that festival; `any` = on every festival day. Festival rows outrank ordinary rows at their hour (kernel/contracts/module_Schedule.md).
- `place` (K-2): blank (unplaced), `home`, `work` (resolved per person through `people.csv` `home_place`/`work_place`), or a `places.csv` id.
