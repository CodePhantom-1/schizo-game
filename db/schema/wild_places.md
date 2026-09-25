# wild_places

**Rows:** 15 · **Source:** The places beyond the walls of the City of the Moon in the D-002 region (~20 km²): fields, riverbank, marsh, desert edge, roads, ruins, caves, tombs (W4-C, INVENTED under D-018/D-021); [docs/proposals/invented-ledger-wild.md](../../docs/proposals/invented-ledger-wild.md).

## Fields

```
id,name,city,kind,terrain,direction,x_m,y_m,danger,camp_site,edge_to,loot,risks,restock_days,description,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `OPEN` rows are never loaded.
- `city` references `cities.csv` (the city whose region this is). `kind`: field | riverbank | marsh | desert_edge | road | ruin | cave | tomb. The travel graph also uses places.csv's gate, wharf, fields and grazing lands, and off-map `cities.csv` ids; none of them is duplicated here.
- `x_m`/`y_m`: metres from the Moon Gate (east/north positive); the bounding box stays within ~20 km² (tests/test_wild.cpp). `danger` 0..10 weights encounters.
- `camp_site` true = a band may camp here. `edge_to`: the off-map city a journey from here leads to (blank = none).
- `loot`: `item:units;…` (items.csv ids) found by searching; `risks`: `token:basis_points;…` with tokens collapse | snake | jackals | restless_dead; `restock_days` 0 = never refills (a robbed tomb stays robbed). Searching a `tomb` is the laws.csv crime `tomb_robbery`.
