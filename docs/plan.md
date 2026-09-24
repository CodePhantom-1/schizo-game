# THE BUILD PLAN — how this game gets made, end to end

**Status:** v2 (2026-09-24) — multi-agent execution model added (D-010). The agent builds everything; subagents on **GLM-5.3-Flash** do the parallel drafting from the first build wave; the designer decides at gates and plays every milestone.
**Related:** [game-design.md](game-design.md) (§11 decisions) · [mechanics.md](mechanics.md) (the imported systems) · [world-bible.md](world-bible.md) (canon) · [../DECISIONS.md](../DECISIONS.md) · parent patterns: [../../docs/architecture.md](../../docs/architecture.md) · [../../docs/roadmap.md](../../docs/roadmap.md)

---

## 0. The deal

| Who | Does |
|---|---|
| **The coordinator** (this agent) | architecture, module contracts, integration, all decision escalations, every gate — the only role that may touch two modules at once |
| **The fleet** (subagents, pinned GLM-5.3-Flash from the first wave) | bounded build work against fixed contracts: kernel modules, UE modules, bulk content rows, review passes, test bots — see §2 |
| **The designer** | the vision decisions (locked: [../DECISIONS.md](../DECISIONS.md)), taste and tone at content gates, playing every milestone |

**Nothing is built that isn't a note, an imported system, or a logged decision.** When anyone — coordinator or fleet — hits an `OPEN` wall, the build stops and asks. That is the anti-mess rule above all others.

## 1. The ten working rules (what keeps it clean)

1. **Data is the game.** Every deity, item, law, rite, schedule, festival, event and NPC is a row in the canon database, imported by the engine. New content = new rows, not new code.
2. **Simulation before scenery.** The world kernel is built and proven **headless** before a single asset exists.
3. **One vertical slice before any breadth.** One street, fully alive, before anything else gets big.
4. **One step = one commit** containing code + data + docs + tests together. No commit without green validators and green tests.
5. **Modules never reach into each other.** Systems talk only through data contracts (§5). Every module has a one-page `module.md` stating exactly that contract.
6. **Traceability is enforced by tooling, not discipline.** `canon_lint.py` fails the build on any row without a tag and a source reference into the notes.
7. **The engine's tools before my own:** World Partition, Mass, StateTree, Smart Objects, PCG, DataTables. Written code is only what UE5 doesn't ship.
8. **Buy or reuse art; hand-make only what is unique.** Everything placeholder is flagged `PLACEHOLDER` until replaced.
9. **Working build at every milestone**, tagged, even when ugly. Every milestone ends with the designer playing it.
10. **Docs are versioned with the game.** A change that alters design reality updates the blueprint docs in the same commit.

## 2. The agent fleet (multi-agent execution model)

**All fleet subagents run GLM-5.3-Flash, pinned explicitly in every fan-out from the first wave onward.** The coordinator is the session model. No model mixing unless the designer orders it.

### 2.1 The three fan-out patterns

1. **Contract-then-flock (code).** The coordinator writes a module's data contract + test skeleton (serial — contracts are architecture). Then a **build agent** implements the module against the contract, a **reviewer agent** checks contract adherence and style before merge, and the coordinator integrates and runs the golden tests. Used for every kernel module (Phase 5) and every UE module (Phase 6).
2. **Table storms (content).** One agent per table/domain drafts rows in bulk — names, items, foods, laws, customs, events, schedules, dialogues, quests. Acceptance is mechanical: `canon_lint.py` + `coverage_check.py` must pass, every row tagged and sourced to the notes. A reviewer agent samples each storm for tone and canon fidelity; the coordinator merges.
3. **World sweeps (verification).** Cheap parallel QA: agents run scenario suites and hunt contradictions across the db ("every deity with a cult site has a city", "no ending without a price"), reporting diffs. Runs after every integration and before every gate.

### 2.2 What stays serial, no matter what

Decision gates · contract authorship · integration milestones · golden-test convergence · the vertical-slice assembly · performance tuning · **every designer gate**. Parallelism compresses drafting — the bulk of the kernel and all content phases — not the critical path's fixed points.

### 2.3 Fleet rules

- **One agent = one module or one table.** No agent edits another's contract, module or rows.
- **Contracts first.** No build agent starts before its contract doc and test skeleton exist.
- **Validators are the merge gate.** An agent's output that fails lint, coverage or tests does not merge; the reviewer's pass is a filter, the validators are the law.
- **OPEN walls stop work.** A fleet agent that needs an undecided answer escalates to the coordinator, who escalates to the designer. A fleet agent never resolves an `OPEN`, never invents, never "fills in".
- **Fleets are sized to the task:** small (2–4) for code modules, large (8–15) for table storms, one per sweep.

## 3. Phase 0 — the decision gate · **COMPLETE** (2026-09-24)

Locked as D-001…D-009 in [../DECISIONS.md](../DECISIONS.md): **UE5** · **map Option A** (one city + region, ~20 km²) · **heirs + past life as story** · **ambiguous-leaning-literal cosmology** · **City of the Moon** as slice/v1 city · repo `schizo-game/` · single prisoner start (notes-dictated) · rank labels drafted by the coordinator, approved by the designer before Phase 4.

**Dev-machine assumption (confirm at Phase 4):** the parent's target — the designer's own PC as minimum spec (60 fps / 1080p / medium in the densest market scene).

## 4. Phase 1 — Foundation · **COMPLETE** (2026-09-24, commits `49dee2e`, `372f721`)

Repo structure (`docs/`, `db/{schema,canon,sources}`, `tools/`), the notes archived verbatim at `db/sources/notes.md`, **29 canon tables (~95 rows) authored and green**, `canon_lint` + `coverage_check` (27/27 notes elements) + `codex_gen` working. The database is the product's backbone, as planned.

## 5. Phase 2 — the world kernel (headless, engine-free) — **COMPLETE** (2026-09-24)

**Wave 0** (commit `adc8d9c`): scaffold + common layer + Time spine + ten frozen contracts. **Wave 1** (fleet run `dwfrun-07078046`, GLM-5.3-Flash): all eight modules built, reviewed, green — 32 open walls escalated and triaged (D-011), zero invented canon. **Wave 2** (coordinator): `WorldState::init/advance_days` wired in the fixed tick order; the DoD proven in `test_world.cpp` — **ten in-game years run headless and byte-deterministic; prices move with the drought; a rumour crosses the city in one hop; rites refuse the unknowing; a crime runs witness → hearing → verdict**. 12/12 suites green; lint + coverage green.

```
kernel/
├── include/sim/
│   ├── Time/          Calendar, Clock, festival calendar (schedules.csv)   ← wave 0 (spine)
│   ├── World/         WorldState — the daily tick; act/clock orchestration ← wave 2 (integration)
│   ├── Economy/       producers, stocks, silver-by-weight, prices (grain base)   ← wave 1
│   ├── Population/    Npc, Household, Schedule, Memory, RumourGraph, L0→L2 promotion ← wave 1
│   ├── Faction/       Standing, oath objects, treaty rules, one-great-king rule  ← wave 1
│   ├── Magic/         rite resolution: the five powers (favour/knowledge/materials/purity/place/time) ← wave 1
│   ├── Justice/       crime → witness → hearing → sealed verdict; detention ← wave 1
│   ├── Events/        trigger engine on the daily tick (events.csv)  ← wave 1
│   ├── Property/      asset registry, deed tablets, steward simulation, loans ← wave 1
│   └── Quests/        quest graphs; systemic quest generation ← wave 1
├── src/               one directory per module, mirrors include/
├── tests/             unit/ · scenarios/ · golden/ (determinism: same seed → byte-identical 10-year history)
└── module.md × N      one page per module: its data contract, what it may not touch
```

**Fleet plan for Phase 2:**
- **Wave 0 (coordinator, serial):** build system (CMake), common types (ids, tags, serialization), the **Time module** (the spine every module stands on), and all ten `module.md` contracts with test skeletons.
- **Wave 1 (8 build agents in parallel, GLM-5.3-Flash):** Economy, Population, Faction, Magic, Justice, Events, Property, Quests — one agent each, against its contract; reviewer pass per module.
- **Wave 2 (coordinator, serial):** integration — the daily tick wiring all modules; golden tests green; determinism proven.
- **Wave 3 (parallel):** scenario suites ("a year in a village under drought", "theft → arrest → verdict") + the first world sweep.

**DoD:** a headless run of **ten in-game years** — prices move with the drought, raid chance responds to hunger and defence, a rite fails when performed impure, a rumour crosses the city at walking speed — deterministic, in CI.

## 6. Phase 3 — engine bring-up (UE5) — **DEFERRED pending the engine** (D-012c)

No Unreal install exists on this machine; the install needs the designer's Epic/GitHub access (action item). The content storms (§8's pattern) are pulled forward while the engine is out — they are engine-independent and everything downstream consumes them.

**When the engine lands, this phase resumes:** `unreal/Plugins/SimRuntime/` binds the kernel to UE (daily tick on the game clock, DataTables import from `db/canon/`) — serial, coordinator — then parallel build agents: `Player/` (first-person; the interaction verb set: touch, sit, eat, drink, carry, open, knock, pray, work) · `NPC/` (StateTree behaviour, Smart Object tasks, Mass crowds — L0/L1/L2) · `UI/` (diegetic: working scales, the drawn map, the codex) · `World/` (World Partition, PCG) · `Audio/`. **DoD:** walk a grey-box street at noon and midnight; doors open; a scheduled NPC does a full day's tasks; silver weighed on real scales; one readable tablet; day/night; save/load through the kernel snapshot.

## 7. Phase 4 — the vertical slice: "one street" → gate

One district of the **City of the Moon** (D-005): market, temple front, houses, the gate. Content authored by **table storms**; assembly is the coordinator's, serial.

- **In the slice:** the prisoner intro; hunger/thirst/sleep; one crafting chain (grain → bread and beer); shops that close at night; the daily clock; guards + one full crime cycle (theft → arrest → hearing → verdict tablet); one rite start to finish with the five powers; one festival day; one quest from a person with a problem; the City-of-the-Moon architecture kit v1 and clothing set v1 (the two art risks).
- **Gate:** the designer plays it. Approval to widen. **No breadth before this gate.**

## 8. Phases 5–8 — breadth by act, then endings

| Phase | Builds | Fleet shape | Gate |
|---|---|---|---|
| **5: Act I** | the D-002 map region; the Empire's administration, cults and minor factions as content; survival + economy + property depth; standing and oaths; Act I beats (game-design §6) | content storms parallel; systems serial | designer plays; content/tone review |
| **6: Acts II–III** | the rebellion and the Brotherhood's approach (the silent-sequence machinery); the drought curve; war set-pieces; companions, the band; magic breadth (planetary opt-ins only if you opt in) | mixed | per act |
| **7: Act IV** | kingship and the founding system (endings §2); the four endings and their prices; the cliffhanger frame; the deed-recap | mixed | designer plays all four paths |
| **8: The hidden ending** | built only after the designer gives the specific sequence (game-design §11.12) | small | designer plays it blind |

Each phase: content pass (rows authored, linted) → systems pass → designer playthrough → fix list → next.

## 9. Phase 9 — polish and release

Player voice (toggle) via licensed TTS; NPC subtitle-plus-bark; accessibility; localization-ready strings from day one (no hardcoded text, ever); performance budgets (§3 target machine); the codex completed; store build. Episodic: **Act I as Early Access, Acts II–IV as major updates.**

## 10. Honest scope and time — with the fleet

The fleet compresses **drafting**, which was the bulk of Phases 2 and 5–8: the kernel's eight modules build in parallel; every content table storms. What does not compress: contracts, integration, the gates, and playtesting. Revised honest ranges:

| Milestone | Solo-agent plan | With the fleet |
|---|---|---|
| Kernel (Phase 2) | months | **weeks** (waves 0+2 are the floor) |
| Engine bring-up + slice (Phases 3–4) | ~1 year | ~6–9 months |
| Early Access (Act I) | 2–3 years | **~1–1.5 years** |
| v1.0 (all four endings) | 4–6 years | **~2–3.5 years** |

Floors are real: integration convergence, engine iteration, and the designer's gates set the minimums. Fan-outs also spend tokens quickly — fleets are sized to the task and table storms are batched (§2.3).

## 11. Guard rails

- No engine project before the Phase 0 answers — **satisfied** (D-001…D-009).
- No invented mechanics — the game-design §8.3 opt-in list stays lore until the designer opts in.
- No `OPEN` decision resolved silently — coordinator or fleet, the build stops and asks.
- No untagged content row, no placeholder asset presented as final, no hardcoded string.
- No breadth past the vertical-slice gate.
- No fleet agent touches contracts, another agent's module, or a decision-log entry.
