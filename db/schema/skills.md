# skills

**Rows:** 27 · **Source:** the skills, six groups, 0..100, grown by use / teachers / texts (mechanics.md row 21; W4-A, D-021). INVENTED rows ledgered in [docs/proposals/invented-ledger-character.md](../../docs/proposals/invented-ledger-character.md); the kernel reads it through [kernel/contracts/module_Character.md](../../kernel/contracts/module_Character.md).

## Fields

```
id,name,group,attribute,grows_by,tag,source_ref
```

- `group` — one of the six: arms, craft, commerce, letters, field, sacred.
- `attribute` — the governing attribute (attributes.csv): each point above or below 5 is +/-10% growth.
- `grows_by` — semicolon list of use tokens. Wired in the kernel: `station:<items.csv station>` (a craft at that station), `verb:trade`, `verb:sell`, `verb:eat` (a hungry meal), `verb:read` (studying any text), `verb:crime_unseen`, `rite:<rites.csv tradition>`. Documented hooks for tracks still being built: `verb:combat`, `verb:treat` (W4-B), `verb:hunt`, `verb:fish`, `verb:travel_by_boat`, `verb:travel_wild` (W4-C).

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `INVENTED` rows are authored glue (D-018), listed in the ledger so the designer can veto any of them.
- `OPEN` rows mark missing canon; they cannot ship (the kernel skips them).
