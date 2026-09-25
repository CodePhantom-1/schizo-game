# weather

**Rows:** 6 · **Source:** The day's weather (W4-C, INVENTED); [docs/proposals/invented-ledger-wild.md](../../docs/proposals/invented-ledger-wild.md).

## Fields

```
id,name,season_weights,drought_weight,travel_pct,desert_travel_pct,thirst_per_hour,encounter_bp,raid_opportunity,raids,description,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `OPEN` rows are never loaded.
- `season_weights`: `season:weight;…` draw weights; `drought_weight` is added per drought stage (may be negative; the weight floors at 0).
- `travel_pct`/`desert_travel_pct`: leg time multipliers (desert tracks and desert-edge places use the second). `thirst_per_hour`: extra thirst while travelling. `encounter_bp`: added to the hourly encounter chance. `raid_opportunity`: added to raid opportunity; `raids` false = no band raids that day.
