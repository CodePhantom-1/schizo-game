# caravans

**Rows:** 3 · **Source:** Caravans that travel real routes on a schedule (W4-C, INVENTED); [docs/proposals/invented-ledger-wild.md](../../docs/proposals/invented-ledger-wild.md).

## Fields

```
id,name,faction,route,goods,every_days,offset_days,guards,description,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `OPEN` rows are never loaded.
- `faction`: the caravan's political owner (`factions.csv`, blank = free merchants); bands with a grudge against it prefer it.
- `route`: `;`-list of nodes, each consecutive pair sharing a `wild_links.csv` link, ending at `moon_gate_place`, where the goods are delivered to the city market.
- `goods`: `item:units;…` (items.csv ids). A run departs on days where `(day-1-offset_days) % every_days == 0` and moves two stops a day; `guards` fight for it.
