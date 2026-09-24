# THE BUILD PLAN — how this game gets made, end to end

**Status:** v1 (2026-09-24) · The execution plan: the agent designs, codes, tests and documents **everything**; the designer decides at defined gates and plays every milestone. Written so the build cannot turn into a mess.
**Related:** [game-design.md](game-design.md) (§11 decisions) · [mechanics.md](mechanics.md) (the imported systems) · [world-bible.md](world-bible.md) (canon) · parent patterns: [../docs/architecture.md](../../docs/architecture.md) · [../docs/roadmap.md](../../docs/roadmap.md)

---

## 0. The deal

| The agent (me) does | The designer does |
|---|---|
| all code, tooling, data authoring, docs, tests, builds — step by step, every phase | the **vision decisions** (Phase 0, from game-design §11) |
| first drafts of all canon-adjacent content (dialogue, quests, codex, flavour), always tagged and linted against the notes | **taste and tone**: reviewing canon-adjacent content at content gates |
| the entire pipeline: validators, test bots, CI, saves, performance | **playing every milestone** — the build's real acceptance test |
| keeping the docs updated in the same change as the code | answering decision requests when a build hits an `OPEN` wall |

**Nothing is built that isn't a note, an imported system, or a logged decision.** When a build needs an answer the notes don't give, I stop and ask — that is the anti-mess rule above all others.

## 1. The ten working rules (what keeps it clean)

1. **Data is the game.** Every deity, item, law, rite, schedule, festival, event and NPC is a row in the canon database, imported by the engine. New content = new rows, not new code.
2. **Simulation before scenery.** The world kernel is built and proven **headless** (no engine) before a single asset exists. If the collapse doesn't run correctly in a terminal, no amount of art saves it.
3. **One vertical slice before any breadth.** One street, fully alive, before anything else gets big.
4. **One step = one commit** containing code + data + docs + tests together. No commit without green validators and green tests.
5. **Modules never reach into each other.** Systems talk only through data contracts (§6). Every module has a one-page `module.md` stating exactly that contract.
6. **Traceability is enforced by tooling, not discipline.** The canon linter fails the build on any row without a tag and a source reference into the notes ([world-bible.md](world-bible.md)).
7. **The engine's tools before my own** (parent roadmap §1.2 adapted): World Partition, Mass, StateTree, Smart Objects, PCG, DataTables. I write only what UE5 doesn't ship.
8. **Buy or reuse art; hand-make only what is unique** (architecture kits per city, clothing sets, key props). Everything AI-generated or placeholder is flagged `PLACEHOLDER` in the asset registry until replaced.
9. **Working build at every milestone**, tagged, even when ugly. Every milestone ends with the designer playing it.
10. **Docs are versioned with the game.** A change that alters design reality updates the blueprint docs in the same commit.

## 2. Phase 0 — the decision gate (the designer's table)

The §11 list, sorted into what **blocks** the build and what can wait. I will ask these as structured questions; until answered, only decision-independent work (Phase 1 start) proceeds.

| # | Decision | Why it gates | My recommendation |
|---|---|---|---|
| 0.1 | **Confirm the engine: UE5** (adopted by mechanics row 1) | everything below Phase 1 | keep UE5; Godot stays the fallback if the slice shows iteration speed matters more than visual ceiling |
| 0.2 | **Map scope & scale** (§11.2) | controls total effort by an order of magnitude | **Option A — parent scale:** one city + its region (20 km²), the rest off-map ventures; grow city-by-city if it works. Option B: 3–4 compressed cities. Option C: all eight at 1:1 — do not do this first |
| 0.3 | **Reincarnation vs heirs** (§11.4) | progression, saves, the whole arc structure | the parent's heir system with the past life as story, until you say otherwise |
| 0.4 | **Backgrounds: single start** (§11.6) | the intro, starting skills | the notes' single prisoner start |
| 0.5 | **Cosmology: literal vs ambiguous** (§11.11) | how the magic system presents every effect | ambiguous-leaning-literal: Mythic mode with the Brotherhood's claim never authorially confirmed |
| 0.6 | **The rank labels** (§11.10) | the RPG layer's vocabulary | I draft a Sumerian-flavoured label ladder for your approval |
| 0.7 | Which city is the **slice city** (follows 0.2) | Phase 4 | the player's arrival city — the City of the Moon, richest and most systems-bearing (port, trade, scholarship) |
| — | *Deferrable:* title, homeland dossier, law-table sources, festival calendar, deity domains, Plan 1/2 environments, wider-world scope, NPC VO, endings unlocks, the hidden sequence | answered per-phase when reached | — |

**Dev-machine assumption (to confirm):** the parent's target — your own PC as minimum spec (60 fps / 1080p / medium in the densest market scene).

## 3. Phase 1 — Foundation: the canon database (no engine, no code risk)

The parent's `research/db/` pattern, adapted: the source of truth is the **notes**, not history.

```
schizo-game/                       # the code repo (created this phase; see 3.1)
├── docs/                          # this blueprint set, moved in and versioned with the game
├── db/
│   ├── schema/                    # table definitions + field docs (one .md per table)
│   ├── canon/                     # authored rows — every row tagged
│   │   ├── deities.csv            # from world-bible §6 (pantheon lists → god rows)
│   │   ├── pantheons.csv          # the lists themselves (Triad, Seven, Anu's, Enlil's, Enki's)
│   │   ├── cities.csv             # the eight cities + Gravestone
│   │   ├── regions.csv            # the directions table (world-bible §7) incl. Plan 1/2 fields
│   │   ├── planetary_powers.csv   # Earth, Moon, Mercury, Venus rows (§8.3)
│   │   ├── magick_forms.csv       # §8.1 forms, each mapped to its system slot
│   │   ├── divination_forms.csv   # §8.2, incl. the five aeromancy sub-types
│   │   ├── factions.csv           # the four powers + cults + minor groups
│   │   ├── endings.csv            # the four endings, prices, unlock slots
│   │   ├── items.csv  buildings.csv  foods.csv  rites.csv
│   │   ├── laws.csv   customs.csv events.csv  schedules.csv
│   │   └── people.csv names.csv   # name stocks: Sumerian/Akkadian [A] + canon names
│   └── sources/                   # notes transcript + [A] citations for imported real material
├── kernel/                        # Phase 2: the engine-agnostic simulation (C++)
├── unreal/                        # Phase 3: the UE5 project
├── tools/
│   ├── canon_lint.py              # THE law: fails on untagged, unsourced or contradictory rows
│   ├── coverage_check.py          # automated game-design §12: every notes element ≥ 1 row
│   └── codex_gen.py               # generates the in-game codex from the same tables
└── DECISIONS.md                   # append-only decision log (Phase 0 answers land here)
```

3.1 **Repo setup step (trivial but yours):** the blueprint currently lives in `schizo game/` (with a space). Phase 1 renames/copies it to `schizo-game/` and inits git there — flagged here because it's your folder.
3.2 **Row format:** `id, tag(CANON/A/OPEN), source_ref(world-bible § / notes line), …fields…`. `[A]` rows additionally cite the real source. `OPEN` rows cannot ship.
3.3 **DoD:** linter green; coverage check green (every §12 element has rows); deities, cities, factions, magick, divination, planetary powers, endings fully authored from the notes.

## 4. Phase 2 — the world kernel (headless, engine-free)

The imported systems implemented as a plain, testable C++ library — no rendering, no UE dependency, runnable in a terminal. This is where "not a mess" is won or lost.

```
kernel/
├── include/sim/
│   ├── Time/          Calendar, Clock, festival calendar (schedules.csv)
│   ├── World/         WorldState — the daily tick; act/clock orchestration
│   ├── Economy/       producers, stocks, silver-by-weight, prices (grain base)
│   ├── Population/    Npc, Household, Schedule, Memory, RumourGraph, promotion L0→L2
│   ├── Faction/       Standing, oath objects, treaty rules, one-great-king rule
│   ├── Magic/         Rite resolution: the five powers (favour/knowledge/materials/purity/place/time)
│   ├── Justice/       crime → witness → hearing → sealed verdict; detention model
│   ├── Events/        trigger engine on the daily tick (events.csv)
│   ├── Property/      asset registry, deed tablets, steward simulation, loans
│   └── Quests/        quest graphs; systemic quest generation from NPC needs
├── src/               # one directory per module, mirrors include/
├── tests/
│   ├── unit/          # per-module
│   ├── scenarios/     # "a year in a village under drought"; "theft → arrest → verdict"
│   └── golden/        # determinism: same seed → same 10-year world history, byte-identical
└── module.md × N      # one page per module: its data contract, what it may not touch
```

**Rules:** no singletons; all state serializable (the world snapshot **is** the save file); modules communicate only through the contracts in their `module.md`; the daily tick is deterministic (parent architecture §4.2) so saves and replays are trivial.

**DoD:** a headless run of **ten in-game years** — prices move with the drought, raid chance responds to hunger and defence, a rite fails when performed impure, a rumour crosses the city at walking speed — all green, deterministic, in CI.

## 5. Phase 3 — engine bring-up (UE5)

Wrap the kernel; prove the fantasy in grey-box.

- `unreal/Plugins/SimRuntime/` — binds the kernel to UE (daily tick on the game clock, DataTables import from `db/canon/`).
- `unreal/Source/SchizoGame/`: `Player/` (first-person, the interaction verb set: touch, sit, eat, drink, carry, open, knock, pray, work), `NPC/` (StateTree behaviour, Smart Object tasks, Mass crowds — L0/L1/L2 from living-world §1), `UI/` (diegetic: working scales, the drawn map, the codex reading `db/` via `codex_gen`), `World/` (World Partition, PCG fields/villages), `Audio/`.
- **DoD:** walk a grey-box street at noon and midnight; doors open; a scheduled NPC does a full day's tasks; silver weighed on real scales; one readable tablet (unreadable until the script skill says otherwise); day/night; save/load through the kernel snapshot.

## 6. Phase 4 — the vertical slice: "one street" → gate

Content (per Phase 0 answers): one district of the slice city — market, temple front, houses, the gate.

- **In the slice:** the prisoner intro; hunger/thirst/sleep; one crafting chain (grain → bread and beer); shops that close at night; the daily clock; guards + one full crime cycle (theft → arrest → hearing → verdict tablet); one rite performed start to finish with the five powers; one festival day; one quest from a person with a problem; the city's canon architecture kit v1 and clothing set v1 (the two art risks).
- **Gate:** the designer plays it. Approval to widen. **No breadth before this gate.**

## 7. Phases 5–8 — breadth by act, then endings

| Phase | Builds | Gate |
|---|---|---|
| **5: Act I** | the Phase-0 map region; the Empire's administration, cults and minor factions as content; survival + economy + property depth; faction standing and oaths; Act I story beats (game-design §6) | designer plays; content/tone review |
| **6: Acts II–III** | the rebellion and the Brotherhood's approach (the silent-sequence machinery); the drought curve; war set-pieces; companions, the band; magic breadth (planetary opt-ins only if you opt in) | per act |
| **7: Act IV** | kingship and the founding system (endings §2); the four endings and their prices; the cliffhanger frame; the deed-recap | designer plays all four paths |
| **8: The hidden ending** | built only after you give the specific sequence (§11.12) | designer plays it blind |

Each phase: content pass (rows authored, linted) → systems pass → designer playthrough → fix list → next.

## 8. Phase 9 — polish and release

Player voice (toggle) via licensed TTS; NPC subtitle-plus-bark (per Phase 0); accessibility; localization-ready strings from day one (rule: no hardcoded text, ever); performance budgets (§2 target machine); the codex completed; store build. The parent's episodic pattern applies: **Act I as Early Access, Acts II–IV as major updates.**

## 9. Honest scope and time

Mirror of the parent's honest timeline, at parent-scale map (Option A): **M0–M2 ≈ 1 year; Early Access (Act I) ≈ 2–3 years; v1.0 (all four endings) ≈ 4–6 years.** Option B adds years; Option C (all eight cities at 1:1) is a different project — the plan refuses it in v1. The agent removes the human-labour bottlenecks; what remains slow is **engine iteration, asset volume, and the gates** — so the gates are few, and each one matters.

## 10. What I will not do (guard rails)

- No engine project, no code, before Phase 0's answers (the memory rule, kept).
- No invented mechanics — the §8.3 opt-in list stays lore until you opt in.
- No `OPEN` decision resolved silently — the build stops and asks.
- No untagged content row, no placeholder asset presented as final, no hardcoded string.
- No breadth past the vertical-slice gate.
