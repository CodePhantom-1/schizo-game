# wild_encounters

**Rows:** 9 · **Source:** What travellers meet on the roads (W4-C, INVENTED); [docs/proposals/invented-ledger-wild.md](../../docs/proposals/invented-ledger-wild.md).

## Fields

```
id,name,kind,hostile,weight,danger_min,terrains,hours,seasons,drought_pct,war_pct,foes_min,foes_max,prowess_pct,description,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `OPEN` rows are never loaded.
- `kind`: bandits | raiders | refugees | merchants | animals. `hostile` true/false. `weight` is the base draw weight; `danger_min` the least link danger it appears on.
- `terrains`: `;`-list of link kinds or place kinds (blank = anywhere). `hours`: any | day | night | dusk_night. `seasons`: `;`-list of seasons.csv ids (blank = all).
- `drought_pct`/`war_pct`: weight +pct per stage (may be negative). `foes_min`..`foes_max` and `prowess_pct` size the fight.
