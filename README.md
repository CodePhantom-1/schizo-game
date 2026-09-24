# SCHIZO GAME *(working title — the real title is an open decision)*

A first-person 3D open-world survival / settlement / action RPG, **100% based on the designer's concept notes**: the crumbling Empire of the Law Giver, the drought that is breaking the world, the four powers contesting the land, the eight cities, the pantheon, the magick of these peoples, and a prisoner taken from his homeland who leaves as a king.

Where everything comes from:

- **Content — 100% the notes** ([docs/world-bible.md](docs/world-bible.md), archived verbatim at [db/sources/notes.md](db/sources/notes.md)): the world, the story, the factions, the cities, the pantheon, the magick, the endings, the tone. Nothing is invented and **nothing is imported from any other project — including no storyline**.
- **Mechanics — imported systems** ([docs/mechanics.md](docs/mechanics.md)): the game's systems are carried unchanged from the earlier Age of Bronze blueprint ([../docs/](../docs/)) by the designer's instruction. **Systems only** — no story, setting, tone or content crosses over with them.

> **Status: Phase 1 complete (foundation).** Phase 0 decisions locked (see [DECISIONS.md](DECISIONS.md)); the canon database is authored and green; next: **Phase 2 — the headless world kernel** ([docs/plan.md](docs/plan.md)). No engine project exists yet.

## The blueprint set

| Doc | What it is |
|---|---|
| [docs/game-design.md](docs/game-design.md) | The game: pitch, the player and his past life, the four activity loops, factions, the story on the imported pacing machinery, magic in this world, content policy, and the full list of open decisions |
| [docs/endings.md](docs/endings.md) | The four endings (one hidden), kingship and the player's own city-state, the cliffhanger frame |
| [docs/world-bible.md](docs/world-bible.md) | The world exactly as the notes give it: cosmology and the ages, the origin myth of Urash, the Empire, the four powers, the eight cities, the pantheon lists, the directions and elements, the forms of magick, the wider world, the real Sumerian reference lists |
| [docs/mechanics.md](docs/mechanics.md) | The audit: every imported system with its source section, what gets reskinned with notes-content, what is open, and the gaps the notes ask for that have no imported mechanic |
| [docs/plan.md](docs/plan.md) | The build plan: how the agent creates the game 100% solo, step by step — the decision gate, the canon database, the headless world kernel, the engine phases, milestones with gates, and the ten working rules that keep it clean |

## The database (the product's backbone)

| Path | What it is |
|---|---|
| [db/sources/notes.md](db/sources/notes.md) | The designer's notes, verbatim and line-numbered — the primary source every canon row cites |
| [db/canon/](db/canon/) | The content tables: deities, pantheons, cities, regions, factions, endings, story, magick, divination, planetary powers, rites, buildings, customs, people, ranks… every row tagged `CANON` / `A` / `OPEN` with a `source_ref` |
| [db/schema/](db/schema/) | One field doc per table (generated from the headers — the CSVs are the schema truth) |
| [tools/canon_lint.py](tools/canon_lint.py) | The law: fails the build on untagged, unsourced or duplicate rows |
| [tools/coverage_check.py](tools/coverage_check.py) | The automated §12 audit: every notes element has a blueprint home |
| [tools/codex_gen.py](tools/codex_gen.py) | Generates the in-game codex from the same tables (build/codex.md) |

## Working rules

1. **The notes are the game.** If a thing is not in the notes and not in an imported system, it does not exist yet.
2. **Mechanics are imported, never invented.** [docs/mechanics.md](docs/mechanics.md) is the audit trail; opt-ins are explicit designer decisions.
3. **Gaps are decisions, not defaults** — `OPEN` rows and questions collect in [DECISIONS.md](DECISIONS.md) and [docs/game-design.md §11](docs/game-design.md).
4. **Tags:** `CANON` the designer's notes · `A` real-world source the notes themselves import · `OPEN` undecided · `INVENTED: EFFECT` — the imported magic policy: the rite is canon, that it *works* is the game's world.
5. Docs are versioned with the game; every change ships code + data + docs + tests together ([docs/plan.md](docs/plan.md), the ten rules).

## Changelog

- **2026-09-24 — plan v2: the agent fleet.** Multi-agent execution model adopted (D-010): coordinator + fleet of subagents pinned to **GLM-5.3-Flash** from the first build wave (Phase 2). Three fan-out patterns (contract-then-flock, table storms, world sweeps), fleet rules, and revised honest timelines (kernel in weeks; Early Access ~1–1.5 years; v1.0 ~2–3.5 years). Phases 0–1 marked COMPLETE in the plan. Plan only — nothing built since Phase 1.
- **2026-09-24 — Phase 1 complete: foundation.** Repo initialized (`schizo-game/`); blueprint docs moved to `docs/`; the notes archived verbatim as the primary source; **the canon database authored** — 29 tables, every row tagged and sourced; `canon_lint` and `coverage_check` green; codex generator working; Phase 0 decisions locked in [DECISIONS.md](DECISIONS.md).
- **2026-09-24 — the build plan.** New `docs/plan.md`: the step-by-step execution structure for a solo-agent build — Phase 0 decision gate, Phase 1 canon database + validators, Phase 2 headless world kernel, Phase 3 engine bring-up, Phase 4 vertical slice, then breadth by act; ten working rules, repo structure, honest timeline.
- **2026-09-24 — Venus completed; notes-coverage audit.** The Venus planetary list written at the designer's explicit commission (D-000): Ishtar's portfolio — love, the sacred rites of Inanna, the binding of hearts, the morning star of conquest, the evening star of peace. Remaining notes-gaps stay `OPEN`. New: game-design §7.1 (canon bound to imported systems), §12 (the notes, covered); Ishtar = the lady of the skies and the sage of the swamps = the Prophet confirmed from the notes' own text; the **War Age** named and tied to the endings.
- **2026-09-24 — blueprint created; reframed per designer instruction.** The notes are the sole content source. The parent project contributes **mechanics only** — no storyline, setting, tone or other content.
