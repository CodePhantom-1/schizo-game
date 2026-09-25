# combat_styles

**Rows:** 11 · **Source:** W4-B combat: how each faction or role fights (mechanics.md row 30, "armour as identity"; game-design.md, "fight & conquer"). All rows are INVENTED, listed in [docs/proposals/invented-ledger-combat.md](../../docs/proposals/invented-ledger-combat.md) #5. Read by kernel `sim/Combat.hpp` (`combat_style`, `style_for`).

## Fields

```
id,name,faction,role,weapon,shield,armour,skill,morale,flee_at,surrender_at,favoured_zone,description,tag,source_ref
```

## Rules

- Every row carries `tag` and `source_ref`. `OPEN` rows are skipped.
- `style_for(faction, role)` picks a row in this order:
  1. a row whose `role` matches the npc's schedule role (case-insensitive);
  2. otherwise, a row whose `faction` matches;
  3. otherwise, `commoner`.
- `weapon`, `shield` and each `;`-separated `armour` id are `arms.csv` ids. They are the kit an npc is given on his first fight.
- `skill` (0–100) applies to all combat skills. `morale` is 0–100.
- `flee_at` and `surrender_at` are health thresholds. `surrender_at` 0 means he never yields.
- `favoured_zone` is `head`, `torso`, `arms`, `legs`, or blank.
