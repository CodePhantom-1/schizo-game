# DECISIONS — append-only log

One entry per decision. The build stops and asks before anything here gets contradicted.

| # | Date | Decision | Source |
|---|---|---|---|
| D-000 | 2026-09-24 | The **Venus** planetary power-list was written at the designer's explicit commission — the only authorized invention beyond the notes (love/desire, the sacred rites of Inanna, fertility, the binding of hearts, unions, morning star of conquest, evening star of mercy). | designer instruction; db/canon/planetary_powers.csv |
| D-001 | 2026-09-24 | **Engine: Unreal Engine 5** (5.4+). Carries the adopted mechanics decision; confirmed by the designer at the Phase 0 gate. Godot 4 remains the named fallback if the vertical slice shows iteration speed matters more than visual ceiling. | Phase 0 gate |
| D-002 | 2026-09-24 | **Map: Option A — one city + region (~20 km²)** at v1. The player's arrival city is fully simulated; the other cities exist through people, caravans, rumours and off-map ventures. Grow city-by-city only if it works. | Phase 0 gate |
| D-003 | 2026-09-24 | **Continuity: the imported heir system** — property, oaths and debts pass to a chosen heir across time skips. The reincarnation stays **narrative** (the Law Giver's past life); no rebirth mechanic is built. | Phase 0 gate |
| D-004 | 2026-09-24 | **Cosmology: ambiguous-leaning-literal** — Mythic mode (rites work, omens land, divine intervention happens), but the game never authorially confirms the Brotherhood's "demonic" claim about Ishtar. Both readings are shown. | Phase 0 gate |
| D-005 | 2026-09-24 | **Slice/v1 city: the City of the Moon** — the player's arrival city, the vertical slice, and the center of the v1 map. | Phase 0 gate |
| D-006 | 2026-09-24 | **Repo:** the blueprint folder renamed `schizo-game/`, git initialized, blueprint docs moved into `docs/`, the notes transcript archived verbatim at `db/sources/notes.md` as the primary source. | Phase 0 gate |
| D-007 | 2026-09-24 | **Rank labels:** the imported 7-tier ladder (0 outsider → 6 king) keeps its structure with canon basis rows; the setting labels will be **drafted by the agent and approved by the designer** before Phase 3 (ranks.csv holds the OPEN slots). | plan.md §2 0.6 |
| D-008 | 2026-09-24 | **Schema docs are generated** from the CSV headers (db/schema/*.md); the CSVs are the schema truth. | plan.md §3 |
| D-009 | 2026-09-24 | **Backgrounds: the notes' single prisoner start.** Not separately asked at the gate — the notes give exactly one backstory ("a prisoner taken from your homeland"), so the single start is dictated by canon. The background *system* exists as an imported mechanic if the designer ever authors more starts. | plan.md §2 0.4; notes L3;L105 |
| D-010 | 2026-09-24 | **Multi-agent execution adopted (plan v2).** The build runs on a coordinator + fleet model from Phase 2 onward: all fleet subagents pinned to **GLM-5.3-Flash** from the first wave. Three fan-out patterns — contract-then-flock (code), table storms (content), world sweeps (verification). Fleet rules: one agent = one module/table; contracts first; validators are the merge gate; OPEN walls escalate, never get filled. Fleet sizing is task-bound (token cost noted). | designer instruction; docs/plan.md v2 §2 |

**Standing rules:** every row tagged + sourced (`canon_lint.py`); `OPEN` rows cannot ship; every notes element keeps a blueprint home (`coverage_check.py`); no decision in this log may be silently changed — append a superseding entry instead.
