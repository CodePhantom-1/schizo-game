# skill_teachings

**Rows:** 43 · **Source:** who or what teaches each skill, at what fee, up to what cap (mechanics.md row 21; W4-A). INVENTED rows ledgered in [docs/proposals/invented-ledger-character.md](../../docs/proposals/invented-ledger-character.md); the kernel reads it through [kernel/contracts/module_Character.md](../../kernel/contracts/module_Character.md).

## Fields

```
id,skill_id,via,source,max_level,fee_per_hour,note,tag,source_ref
```

- `skill_id` — a skills.csv id.
- `via` — `teacher` (a schedules.csv role; any resident with that role teaches) or `text` (an items.csv id the reader holds; read, not consumed; needs scribal arts 10).
- `max_level` — the cap: a teacher's own skill (its light sheet), a text's limit.
- `fee_per_hour` — silver grains the player pays the teacher per hour (0 for a text).

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `INVENTED` rows are authored glue (D-018), listed in the ledger so the designer can veto any of them.
- `OPEN` rows mark missing canon; they cannot ship (the kernel skips them).
