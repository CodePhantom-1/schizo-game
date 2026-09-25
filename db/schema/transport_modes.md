# transport_modes

**Rows:** 6 · **Source:** How the player travels (mechanics.md row 31; W4-C, INVENTED numbers); [docs/proposals/invented-ledger-wild.md](../../docs/proposals/invented-ledger-wild.md).

## Fields

```
id,name,speed_m_per_h,requires_item,link_kinds,description,tag,source_ref
```

## Rules

- every row carries `tag` (`CANON` / `A` / `INVENTED` / `OPEN`) and `source_ref`; `OPEN` rows are never loaded.
- `speed_m_per_h`: integer metres per hour (D-022). `requires_item`: an inventory item needed to use the mode (blank = always).
- `link_kinds`: `;`-list of `wild_links.csv` kinds the mode can use.
