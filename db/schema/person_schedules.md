# person_schedules

**Rows:** 4 · **Source:** per-person variations on the role schedules (INVENTED, K-2) — [docs/proposals/invented-ledger-festivals.md](../../docs/proposals/invented-ledger-festivals.md).

## Fields

```
id,person_id,hour,task,place,season,festival,tag,source_ref
```

## Rules

- every row carries `tag` and `source_ref`; `OPEN` rows never surface.
- `person_id` is a `people.csv` id. A person row outranks that person's role row at the same hour; `season`/`festival`/`place` behave as in `schedules.csv`.
