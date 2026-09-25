# festivals

**Rows:** 4 · **Source:** the machine-readable form of `calendar.csv:festival_days` (INVENTED, D-018); K-2 — [docs/proposals/invented-ledger-festivals.md](../../docs/proposals/invented-ledger-festivals.md).

## Fields

```
id,name,day_of_year,deity,place,attend_from,attend_until,exempt_roles,market,gathering,description,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `OPEN` rows never reach the calendar.
- `day_of_year` 1..360, at most one festival per day; the festival recurs every year.
- `place`: a `places.csv` id or `home`. `[attend_from, attend_until)`: hours (from 0..23, until up to 24) in which non-exempt roles' ordinary tasks give way to the `gathering` task. `exempt_roles`: `;`-separated schedules.csv roles that keep their day. `market`: `open` / `closed`.
