# callings

**Rows:** 26 · **Source:** callings and their specialisations (mechanics.md row 23; W4-A, D-021). INVENTED rows ledgered in [docs/proposals/invented-ledger-character.md](../../docs/proposals/invented-ledger-character.md); the kernel reads it through [kernel/contracts/module_Character.md](../../kernel/contracts/module_Character.md).

## Fields

```
id,name,kind,parent,skills,specialisations,perks,tag,source_ref
```

- `kind` — `calling` (chosen at level 5; a second one at 25) or `specialisation` (of `parent`, chosen at 15).
- `skills` — the favoured skills: +50% growth while the calling is held.
- `specialisations` — for a calling row: its specialisation ids (each row's `parent` points back).
- `perks` — a specialisation's perks, in the effect grammar of talents.csv `effect`.

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `INVENTED` rows are authored glue (D-018), listed in the ledger so the designer can veto any of them.
- `OPEN` rows mark missing canon; they cannot ship (the kernel skips them).
