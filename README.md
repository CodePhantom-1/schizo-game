# SCHIZO GAME *(working title — the real title is an open decision)*

A first-person 3D open-world survival / settlement / action RPG, **100% based on the designer's concept notes**: the crumbling Empire of the Law Giver, the drought that is breaking the world, the four powers contesting the land, the eight cities, the pantheon, the magick of these peoples, and a prisoner taken from his homeland who leaves as a king.

Where everything comes from:

- **Content — 100% the notes** ([docs/world-bible.md](docs/world-bible.md), archived verbatim at [db/sources/notes.md](db/sources/notes.md)): the world, the story, the factions, the cities, the pantheon, the magick, the endings, the tone. Nothing is invented and **nothing is imported from any other project — including no storyline**.
- **Mechanics — imported systems** ([docs/mechanics.md](docs/mechanics.md)): the game's systems are carried unchanged from the earlier Age of Bronze blueprint ([../docs/](../docs/)) by the designer's instruction. **Systems only** — no story, setting, tone or content crosses over with them.

> **Status: Phase 3 begun — the game builds inside Unreal.** The UE 5.8.3 editor was compiled from source, and the SchizoGame project (SimRuntime plugin + world kernel) builds through UnrealBuildTool. Kernel: 16/16 suites green; all four acts have quest/dialogue data. Next: subsystem verification in the editor, then the grey-box street of the City of the Moon. Kernel done (12/12 suites, ten-year determinism proven); UE5 bring-up deferred pending engine install (D-012c — designer action item); the seven-table content storm is running on GLM-5.3-Flash. No engine project exists yet.

## The blueprint set

| Doc | What it is |
|---|---|
| [docs/game-design.md](docs/game-design.md) | The game: pitch, the player and his past life, the four activity loops, factions, the story on the imported pacing machinery, magic in this world, content policy, and the full list of open decisions |
| [docs/endings.md](docs/endings.md) | The four endings (one hidden), kingship and the player's own city-state, the cliffhanger frame |
| [docs/world-bible.md](docs/world-bible.md) | The world exactly as the notes give it: cosmology and the ages, the origin myth of Urash, the Empire, the four powers, the eight cities, the pantheon lists, the directions and elements, the forms of magick, the wider world, the real Sumerian reference lists |
| [docs/mechanics.md](docs/mechanics.md) | The audit: every imported system with its source section, what gets reskinned with notes-content, what is open, and the gaps the notes ask for that have no imported mechanic |
| [docs/plan.md](docs/plan.md) | The build plan: how the agent creates the game 100% solo, step by step — the decision gate, the canon database, the headless world kernel, the engine phases, milestones with gates, and the ten working rules that keep it clean |
| [docs/completion-plan.md](docs/completion-plan.md) | The completion plan (2026-09-25): every system, the world, the UI, audio and ship quality, down to the smallest detail, as stages A–U with a playable "done when" per step; the main quest is parked until the designer says go |
| [docs/city-of-the-moon.md](docs/city-of-the-moon.md) | The City of the Moon redesigned (D-025): a compact crescent of ~110 living buildings around the sacred lagoon of the two waters, in nine quarters |

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

- **2026-09-25 — W4-A: character progression.** mechanics.md rows 20 to 24 are now in the kernel (`sim/Character.hpp`, `sim/Progression.hpp`; [module_Character.md](kernel/contracts/module_Character.md)):
  - six attributes, which rise with exercise;
  - 27 skills in six groups. They grow by use (crafting, rites, trade, hungry meals, reading, unseen deeds, work), from teachers among the street's residents (fee and cap in `skill_teachings.csv`), from texts (literacy needed), and by practice up to 25;
  - levels from skill points, capped at 40, with one talent per level (41 talents, each with a real effect);
  - 8 callings and 18 specialisations with perks, at levels 5, 15 and 25, giving +50% growth;
  - rank per polity, raised by standing plus a patron's act and dropped to 0 by outlawry;
  - work for wages (rations as wages).

  All of it uses integer and basis-point maths (D-022). Nothing gates on story or quests (D-021): `test_scenario_progression` shows a fresh player reaching every calling and talent through C API play alone. Saves carry a trailing optional section, so old saves still load. All new content is `INVENTED` and listed in [docs/proposals/invented-ledger-character.md](docs/proposals/invented-ledger-character.md).

- **2026-09-25 — K-2: festivals and per-person schedules.** The D-018 festival days are live on the kernel calendar through the new `festivals.csv` table. Each festival bends the street's day: non-exempt townspeople go to the gathering at the temple (or at home for the kispum feast), and the market opens or closes as the row says. The festival also counts as the festival day that Magic's time power checks, and Events can trigger on `festival:<id>`. Each person now gets their own day plan: role rows, plus their own `person_schedules.csv` rows, bent by season and festival, with `home`/`work` resolved to `places.csv` ids. New C API: `sim_world_is_festival`/`_festival`/`_festival_on`/`_market_open` and `sim_world_npc_id`/`_npc_task_at`/`_npc_place_at`/`_npc_schedule_at`. No new save state. All placeholder content is `INVENTED` and listed in [docs/proposals/invented-ledger-festivals.md](docs/proposals/invented-ledger-festivals.md).
- **2026-09-25 — K-1: the magic loop closes.** Rites are now learned (from the priest of the moon, or by reading a held text — new INVENTED table `rite_teachings.csv`), offered (materials debited from the player's inventory on every performed rite) and answered (the four canon effect families applied on success: favour, favour with the god a hymn addresses, a 30-day ward, a 75%-true omen — all `INVENTED: EFFECT`, ledgered in [docs/proposals/invented-ledger-rites.md](docs/proposals/invented-ledger-rites.md)). Magic still writes only MagicState; the fixed formula is untouched. Save/load and the C API carry it all.

- **2026-09-25 — the acts have data.** Act I, II and III quest + dialogue content authored by the GLM-5.3-Flash fleets and integrated gate-green (32 quest defs, 38 dialogue rows total; every row tagged INVENTED and citing its canon beat). Each act audited by fresh-eyes agents — findings applied (the Brotherhood's courier never approaches cold; the second noticing unified; named festivals made descriptive). Kernel additions: the C API (`sim/CApi.h`, the UE5 binding boundary), the purse (D-017 — property income credits per owner), engine-ready DataTables (`data/ue/`, staged to `Content/Sim/canon`), and `USimWorldSubsystem` (the world's engine home, authored pre-editor). 16/16 kernel suites green throughout; the editor compile at ~65%.

- **2026-09-25 — designer round 1 approved (D-015).** The rank ladder is canon: 0 the Outsider · 1 Sojourner · 2 Householder · 3 Éren (King's Man) · 4 Gigir (Chariot Warrior) · 5 Sukkal (Intimate of the King) · 6 Lugal (King) — D-007 resolved. The calendar's four seasons are canon (rains/sowing/harvest/vintage, new `seasons.csv`); `WorldState::init` builds the calendar from canon so `season_id()` is live in the tick; first season-gated event authored. Months and festival days stay OPEN.

- **2026-09-25 — content storm integrated; database is alive.** 147 rows authored across items, foods, laws, customs, events, schedules and names (run `dwfrun-27338ee5`, seven GLM-5.3-Flash authors + reviewers). The kernel gate went red on stale Wave-1 test assumptions (empty-table assertions) — fixed canon-robustly, 12/12 suites green with the new canon loaded; the storm workflow now gates on the kernel suite too. Law severity grounded in the real codes (D-013: Hammurabi/Eshnunna/Ur-Nammu penalties; the retune lever is the designer's). Items now sit on real shelves: markets stock 24 goods at canon price bands.

- **2026-09-24 — content storm launched; Phase 3 sequenced.** No UE5 on this machine (D-012c: designer action item — install needs Epic/GitHub access), so the seven-table content storm (items, foods, laws, customs, events, schedules, names) is pulled forward and is running on GLM-5.3-Flash (`tools/workflows/storm_canon_content.ts`). Content policy gains the `INVENTED` tag (D-012); the D-011 conventions became real columns (events.repeat, rites.purity_required, quests.deadline_days).

- **2026-09-24 — Phase 2 begun.** **Wave 0 complete** (commit `adc8d9c`): the headless C++20 kernel scaffolded — common layer (Types, deterministic Rng, Db canon loader, Test harness), the Time spine implemented + tested, the integration seam (`Context.hpp`, fixed tick order), `World.hpp` contract, ten frozen module contracts. **Wave 1 running**: the fleet workflow (`tools/workflows/wave1_kernel_modules.ts`) fans out 8 GLM-5.3-Flash build agents (Economy, Population, Faction, Magic, Justice, Events, Property, Quests) → deterministic cmake+ctest gate → per-module contract reviews → fix pass → final gate.
- **2026-09-24 — plan v2: the agent fleet.** Multi-agent execution model adopted (D-010): coordinator + fleet of subagents pinned to **GLM-5.3-Flash** from the first build wave (Phase 2). Three fan-out patterns (contract-then-flock, table storms, world sweeps), fleet rules, and revised honest timelines (kernel in weeks; Early Access ~1–1.5 years; v1.0 ~2–3.5 years). Phases 0–1 marked COMPLETE in the plan. Plan only — nothing built since Phase 1.
- **2026-09-24 — Phase 1 complete: foundation.** Repo initialized (`schizo-game/`); blueprint docs moved to `docs/`; the notes archived verbatim as the primary source; **the canon database authored** — 29 tables, every row tagged and sourced; `canon_lint` and `coverage_check` green; codex generator working; Phase 0 decisions locked in [DECISIONS.md](DECISIONS.md).
- **2026-09-24 — the build plan.** New `docs/plan.md`: the step-by-step execution structure for a solo-agent build — Phase 0 decision gate, Phase 1 canon database + validators, Phase 2 headless world kernel, Phase 3 engine bring-up, Phase 4 vertical slice, then breadth by act; ten working rules, repo structure, honest timeline.
- **2026-09-24 — Venus completed; notes-coverage audit.** The Venus planetary list written at the designer's explicit commission (D-000): Ishtar's portfolio — love, the sacred rites of Inanna, the binding of hearts, the morning star of conquest, the evening star of peace. Remaining notes-gaps stay `OPEN`. New: game-design §7.1 (canon bound to imported systems), §12 (the notes, covered); Ishtar = the lady of the skies and the sage of the swamps = the Prophet confirmed from the notes' own text; the **War Age** named and tied to the endings.
- **2026-09-24 — blueprint created; reframed per designer instruction.** The notes are the sole content source. The parent project contributes **mechanics only** — no storyline, setting, tone or other content.
