# Invented ledger — W2-B, the slice street of the City of the Moon

Per D-018: every invented choice below fits theme and canon, contradicts no CANON/A row or the notes, and is subject to designer veto (a veto supersedes here).

- **New table `places.csv`.** Granular locations (gate, market stalls, bakery, brewery/tavern, temple front, houses, well, watch post, shrine niche, granary) sit inside the City of the Moon (CANON, wb §5) but the notes give the city only in outline. Fills the gap the vertical slice (plan.md §7) needs: "market, temple front, houses, the gate."
- **7 building kinds** (`city_gate`, `watch_post`, `market_stall`, `bakery`, `brewery_tavern`, `mudbrick_house_modest`, `mudbrick_house_courtyard`). The two house types are tagged `A` (real Mesopotamian domestic-architecture record, Ur/Eridu excavations); the rest are `INVENTED` glue grounded in existing schedules.csv rows (gate opens/closes, watch patrol, market stalls, baking) and wb §5.
- **29 light residents** (`people.csv`), each `INVENTED`, drawing real attested Sumerian/Akkadian names from `names.csv` (tag `A`) and roles matching `schedules.csv` exactly. Faction `southern_city_states_alliance` given to ordinary residents (the alliance's home city, wb §5); left blank for civic office-holders (gatekeepers, watchmen) and temple clergy (priest/priestess), who serve the city or the god rather than the political alliance.
- **Enmenanna, priestess of the moon**, fills quests.csv `the_priestess_debt`'s unnamed debtor priestess. **Ea-nasir, market trader**, fills that same quest's unnamed "grain merchant who laughs at temple marks" — a deliberate, gentle wink at the real Ea-nasir's attested reputation (Old Babylonian complaint tablet about bad copper), repurposed here for grain and temple debt rather than copper, so it never claims that specific historical complaint as this game's canon.
- **2 new schedule roles** (`brewer`, `tavern keeper`) with base (non-seasonal) rows, since the street's brewery/tavern needed staff with no prior schedule entry.
- **6 seasonal schedule variants** bending the day by season (rains/sowing/harvest/vintage): harvest grain-rush for market traders and gatekeepers, vintage date-sweetened baking and date-wine brewing, a rains-season shorter water-carrier draw (the drought discipline holding even when the wells rise), and sowing-season seed-loan contracts for the scribe (grounded in notes L57's sowing wheat and barley, and game-design §5.3's loans).
- **District name "Moon Gate Quarter"** — an invented label for the slice-street district within the City of the Moon; no canon district name exists to contradict.

## Registration

- `places` added to `tools/export_datatables.py` TABLES.
- `places` is **not yet** added to `kernel/src/Db.cpp`'s table list — flagged in the handback report for the kernel owner.
