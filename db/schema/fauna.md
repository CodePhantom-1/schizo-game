# fauna

**Rows:** 20 · **Source:** the animals of world-art-plan §8, held to its §10 guard rails; group sizes, hours and counts are `INVENTED` numbers (ledger: [docs/proposals/invented-ledger-fauna.md](../../docs/proposals/invented-ledger-fauna.md)).

## Fields

```
id,name,group,habitats,hours,group_min,group_max,per_place,herd_share,model,tag,source_ref
```

## Rules

- every row carries `tag` and `source_ref`; the species are real (`A`), their numbers invented.
- `group` is one of `herd`, `work`, `city`, `bird_flock`, `bird_wader`, `bird_raptor`, `wild_night`, `water`, `small`; it picks the behaviour in `USimFaunaSubsystem` (herds follow the kernel's head count, flocks are boids, night predators come out after dark).
- `habitats` is a `;`-list of flora habitats (`shore`, `water`, `yard`, `street_edge`, `precinct`, `tombs`, ...) and place typologies prefixed `@` (`@cattle_pen`, `@stable`, `@envoys_house`, ...).
- `hours` is the active window `HH-HH` (it may wrap midnight: `20-05`); outside it the animal sleeps or is penned.
- `group_min`/`group_max` bound a group's size; `per_place` is how many groups stand at each place of an `@` typology.
- `herd_share` is the share of the kernel's `herd_head` this species takes on the pastures; the herd rows sum to 1.
- `model` is an id in `art/fauna_models.csv` (empty for a sound-only row).
- §10: no chickens, no camels; equids (the kunga) only at the eastern peoples' and the elite's places — `tools/tests/test_art_fauna.py` enforces it.
- the table is art and behaviour data; the kernel's herd count is the only simulation it follows (listed as lore in `tools/mechanics_registry.py`).
