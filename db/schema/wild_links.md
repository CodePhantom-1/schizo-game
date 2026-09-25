# wild_links

**Rows:** 26 · **Source:** Travel links between places (W4-C, INVENTED); [docs/proposals/invented-ledger-wild.md](../../docs/proposals/invented-ledger-wild.md).

## Fields

```
id,from,to,kind,distance_m,danger,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `OPEN` rows are never loaded.
- `from`/`to`: `wild_places.csv`, `places.csv` or (for `journey`) `cities.csv` ids; links are two-way.
- `kind`: road | track | river | marsh_path | desert_track | journey. `transport_modes.csv` says which modes may use each kind.
- `distance_m`: integer metres (D-022); journeys to off-map cities are tens of km. `danger` 0..10 weights the encounter chance.
