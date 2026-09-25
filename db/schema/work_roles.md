# work_roles

**Rows:** 19 · **Source:** shifts a resident will hire the player for: rations as wages (rpg-systems §6; W4-A). INVENTED rows ledgered in [docs/proposals/invented-ledger-character.md](../../docs/proposals/invented-ledger-character.md); the kernel reads it through [kernel/contracts/module_Character.md](../../kernel/contracts/module_Character.md).

## Fields

```
id,role,skills,wage_item,wage_qty,per_hours,min_skill,note,tag,source_ref
```

- `role` — a schedules.csv role; any resident with that role hires.
- `skills` — semicolon list; each grows by use while working.
- `wage_item`, `wage_qty`, `per_hours` — the wage: wage_qty of wage_item per per_hours hours (floored); `silver` = the purse.
- `min_skill` — the first listed skill the work needs (e.g. a scribe must already read).

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `INVENTED` rows are authored glue (D-018), listed in the ledger so the designer can veto any of them.
- `OPEN` rows mark missing canon; they cannot ship (the kernel skips them).
