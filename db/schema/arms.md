# arms

**Rows:** 20 · **Source:** W4-B combat (mechanics.md rows 29–30). Objects tagged `A` are attested in the real-world record. Every stat number is INVENTED machinery, listed in [docs/proposals/invented-ledger-combat.md](../../docs/proposals/invented-ledger-combat.md). Read by kernel `sim/Combat.hpp` (`arms_def`).

## Fields

```
id,name,slot,skill,damage_type,damage,reach_cm,speed,weight_g,balance,durability,quality,zones,layer,prot_cut,prot_pierce,prot_blunt,block,stamina,heat,material_tier,ammo,culture,tag,source_ref
```

## Rules

- Every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`. `OPEN` rows are never equipped.
- Every `id` is also an `items.csv` row, the tradeable good. This table holds only the fighting numbers.
- Integers only (D-022): `reach_cm` (for a ranged weapon, its range), `weight_g`, `speed` and `balance` 1–10, `quality` 0–100, `durability` in points.
- `slot`: `melee` | `ranged` | `shield` | `armour`.
- `skill`: a skill token (`blades`, `axes`, `maces`, `spears`, `bows`, `slings`, `shields`). This is the W4-A seam.
- `damage_type`: `cut` | `pierce` | `blunt`.
- Armour: `zones` is a `;` list of `head`/`torso`/`arms`/`legs`. `layer` is `under` | `over`, and only one piece is worn per zone per layer. `prot_*` are protection points against each damage type.
- `material_tier`: the metal ladder runs `flint` → `copper` → `arsenical` → `tin_bronze`. `iron` is a curiosity and cannot be recast. The rest are `leather`, `linen` and `organic`. Only copper, arsenical and tin bronze can be recast.
- `ammo`: the `items.csv` id a ranged weapon spends per shot. A javelin spends itself.
