# talents

**Rows:** 41 · **Source:** talents, one per level (mechanics.md row 22; W4-A, D-021). INVENTED rows ledgered in [docs/proposals/invented-ledger-character.md](../../docs/proposals/invented-ledger-character.md); the kernel reads it through [kernel/contracts/module_Character.md](../../kernel/contracts/module_Character.md).

## Fields

```
id,name,calling,skill,effect,min_level,min_skill,tag,source_ref
```

- `calling` — "" (anyone) or a calling / specialisation id the player must hold.
- `skill`, `min_skill` — the prerequisite skill value (effective).
- `min_level` — the level the talent opens at.
- `effect` — semicolon list: `skill_bonus:<skill>:<n>`, `growth_bp:<skill>:<bp>`, `attribute:<attr>:<n>`, `trade_bp:<bp>`, `wage_bp:<bp>`, `fee_bp:<bp>`, `study_bp:<bp>`, `rite_favour_bp:<bp>` (basis points: 10000 = x1, D-022).

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `INVENTED` rows are authored glue (D-018), listed in the ledger so the designer can veto any of them.
- `OPEN` rows mark missing canon; they cannot ship (the kernel skips them).
