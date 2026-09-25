# wild_groups

**Rows:** 5 · **Source:** Bandit, raider and partisan bands and their camps (W4-C, INVENTED on canon factions); [docs/proposals/invented-ledger-wild.md](../../docs/proposals/invented-ledger-wild.md).

## Fields

```
id,name,kind,faction,leaders,camp_sites,territory,forms_when,form_days,repop_days,base_strength,max_strength,start_supplies,start_morale,prowess_pct,prefers,grudges,description,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `OPEN` rows are never loaded.
- `kind`: bandits | raiders | partisans. `faction`: `factions.csv` id, blank for outlaws of no faction.
- `leaders`: `;`-list; each (re)formation takes the next name. `camp_sites`: `;`-list of `wild_places.csv` camp sites, the first being home; the band moves along it when threatened. `territory`: the nodes it roams, raids and ambushes on.
- `forms_when`: `start` or `drought_gte:N` / `war_gte:N` (AND-ed; the events.csv trigger vocabulary). The camp forms after `form_days` consecutive days of those conditions; `repop_days` after a clearing it may re-form if they still hold.
- `base_strength`/`max_strength` in fighters; `start_supplies`/`start_morale` 0..100; `prowess_pct` 100 = ordinary armed men. `prefers`: raid-target weights `fields|herds|caravans|market:N`. `grudges`: `factions.csv` or `wild_groups.csv` ids it hates (feuds, target choice).
