# places

**Rows:** 23 · **Source:** canon rows authored from the designer's notes ([world-bible](../../docs/world-bible.md)), researched real-world rows, or reserved for later phases.

## Fields

```
id,city,district,kind,building_id,owner_person_id,name,description,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref` (`wb §x` and/or `notes L<n>` — [db/sources/notes.md](../sources/notes.md); `A` rows name their real-world source; `INVENTED` rows are authored glue shown to the player as such).
- `OPEN` rows mark missing canon; they cannot ship.
- `city` references `cities.csv`; `building_id` references `buildings.csv` when the place sits inside a specific building (blank for an open-air place — square, well, shrine niche, granary).
- `owner_person_id` references `people.csv` when the place has a resident/proprietor owner (blank for public places — the gate, the watch post, the market square, the well).
