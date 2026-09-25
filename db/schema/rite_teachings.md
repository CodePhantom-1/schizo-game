# rite_teachings

**Rows:** 4 · **Source:** INVENTED (K-1, D-018) — who or what teaches each rite of [rites.csv](rites.md); ledger: [docs/proposals/invented-ledger-rites.md](../../docs/proposals/invented-ledger-rites.md).

## Fields

```
id,rite_id,via,source,note,tag,source_ref
```

- `rite_id` — a rites.csv id.
- `via` — `teacher` or `text`.
- `source` — for `teacher`: a schedules.csv role (matched case-insensitively against a Population npc's role); for `text`: an items.csv id the reader must hold (read, not consumed).
- `note` — flavor for the codex.

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref` (`wb §x` and/or `notes L<n>` — [db/sources/notes.md](../sources/notes.md); `A` rows name their real-world source; `INVENTED` rows are authored glue shown to the player as such).
- `OPEN` rows mark missing canon; they cannot ship (the kernel skips them).
