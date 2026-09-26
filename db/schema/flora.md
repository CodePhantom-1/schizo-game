# flora

**Rows:** 19 · **Source:** the plants, stones and household clutter of world-art-plan §5–§7; the densities and scales are `INVENTED` numbers (ledger: [docs/proposals/invented-ledger-flora.md](../../docs/proposals/invented-ledger-flora.md)).

## Fields

```
id,name,group,habitats,seasons,withers,meshes,per_100m2,scale_min,scale_max,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; the plants are real (`A`), their numbers invented.
- `group` is one of `tree`, `shrub`, `grass`, `water`, `flower`, `crop`, `prop`; it picks the cull distance in the game (grass and flowers near, shrubs and props farther, trees never culled).
- `habitats` is a `;`-list of where the species stands: `shore`, `water`, `yard`, `garden`, `street_edge`, `open`, `wall_foot`, `roof`, `precinct`, `tombs`, `camp`. `tools/art/scatter.py` turns each habitat into candidate points around the crescent.
- `seasons` is a `;`-list of `seasons.csv` ids when the species shows (spring flowers, lilies), or `all`.
- `withers` is `1` when the drought browns it (its leafy parts turn to straw as the drought stage rises), `0` for stones, pots and sacks.
- `meshes` is a `;`-list of mesh ids from `art/scatter_meshes.csv`; each instance picks one at random.
- `per_100m2` is the density over the habitat's area; `scale_min`/`scale_max` bound the uniform random scale of each instance.
- the table is art data: the kernel loads it with the rest of the canon but no mechanic reads it (it is listed as lore in `tools/mechanics_registry.py`).
