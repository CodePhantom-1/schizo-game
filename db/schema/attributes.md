# attributes

**Rows:** 6 · **Source:** the six attributes, range 1..10 (mechanics.md row 20; W4-A, D-021). INVENTED rows ledgered in [docs/proposals/invented-ledger-character.md](../../docs/proposals/invented-ledger-character.md); the kernel reads it through [kernel/contracts/module_Character.md](../../kernel/contracts/module_Character.md).

## Fields

```
id,name,description,tag,source_ref
```

- `description` — what the attribute covers, for the codex and the UI.

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `INVENTED` rows are authored glue (D-018), listed in the ledger so the designer can veto any of them.
- `OPEN` rows mark missing canon; they cannot ship (the kernel skips them).
