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

## Follow-up (bug review 2026-09-25)

- **`midday_rest` split per role.** The row's role was the compound string "workshops and households", which no resident holds (roles match `schedules.csv` exactly), so the midday rest never applied to anyone. It is now two rows with the same task text: `midday_rest` (role `craftsman`) and `midday_rest_households` (role `household`). Both stay `INVENTED` (D-012 glue); see docs/audits/bug-review-unreal-tools-2026-09-25.md.

## W6-B street builder (2026-09-25, wave 6)
Presentation-layer inventions for building the quarter in UE (all INVENTED, vetoable; data stays canon):
- Layout: 8 m plot centres alternating ±5 m about the street axis, turning north after 14 plots; the gate special-cased onto the axis. Same CSV row order → same street, every time.
- Per-kind footprints (m): gate 2 m opening; market 6×6 paved; stalls 1×1 tile+awning; bakery 3×3; brewery 4×3; temple 4×4 + flanking pilasters; well/shrine 1×1 paved; granary 2×2 windowless; smithy 3×3 walled open yard; default 2×2; house 4×4 for mudbrick_house_courtyard else 3×3, roof-access variant when StrCrc32(place_id)%3==2.
- The door slot is an APPROACH point (1 m outside the wall, ground z, yaw facing the street); doors hang on the wall face (see SimVerbSpawner), the gate's leaf quarter-turned and doubled to fill the opening.
- Runtime colour palette by FBX slot name (M_MudPlaster/M_Mudbrick/M_Timber/M_Reed from kit_common.py MAT_DEFS) on the engine's parameter material — no authored materials until the content pass.
- Kit import: cut pieces (SM_WallDoor/SM_WallWindow/SM_RoofAccess/SM_Awning) carry NO auto collision (a convex hull of a cut piece is the solid slab — doorways would be unenterable); solid pieces keep theirs.
