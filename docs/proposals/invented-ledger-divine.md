# Invented ledger — W5 (divine wrath: the gods' books)

Per D-018 (creative gap-filling authorized; every invented item ledgered so
the designer can veto it). The designer rows name the CAUSES and the SHAPE —
a broken oath offends (mechanics.md row 26), an impure or failed rite
offends (rows 13/20), a conviction for an offence against the gods adds
"substantial" wrath (row 12; laws.csv), wrath escalates to omens, favour
penalties and a curse condition lifted by atonement, and it decays with time
— every NUMBER, deity mapping and mechanism below is invented to make that
run, and every number is a retunable constant. Code:
`kernel/include/sim/Divine.hpp`, `kernel/src/Divine.cpp`,
`kernel/src/CApiDivine.cpp`; hooks in `Faction.cpp` (break_oath),
`Rites.cpp` (perform_rite_in_world), `Justice.cpp` (hold_hearing).

## What is imported, not invented

- **The oath-breaker curse exists** — Faction.hpp's frozen `oath_breaker_curse`
  flag (rpg-systems §4.2; mechanics.md row 26: "the curse's bite is the
  magic system's problem"). W5 is that bite.
- **The causes** — mechanics.md rows 26 (broken oath), 13/20 (purity is
  load-bearing; an impure or failed rite offends), 12 + laws.csv (the divine
  offences: sacrilege, tomb_robbery, oath_breaking, each `A`-tagged to the
  law codes).
- **The consequence surfaces** — omens read worse (Rites' omen system,
  K-1), favour penalties (Magic's favour through the Rites favour system),
  a character condition that persists until a successful atonement rite,
  slow decay over time, a sharp drop on atonement. All named by the designer
  rows; W5 only numbers them.
- **"Wrath accumulates per deity"** using deities.csv ids, and rites address
  a deity via rites.csv's `deity` column / the resolved target — the designer
  row's own rule.

## Invented (W5)

### The book

- **Wrath is per (offender, deity), 0..100** — one book per pair, like
  favour and standing. Offenders are `player` or an npc id; npcs accrue
  (the gods see their oaths and crimes too), but only the player's wrath
  bites through the favour and omen systems, because MagicState models ONE
  performer's favour (Magic.hpp) — an npc's "favour penalty" has no state
  to land in. Npc wrath and npc curses are carried and queryable
  (sim_world_divine_*) for the engine and later systems.
- **The accrual verbs are module-hook-shaped**: Faction.cpp's `break_oath`
  and Justice.cpp's `hold_hearing` take an additive defaulted
  `DivineState*` (null keeps the Wave-1 behaviour exactly); Rites.cpp calls
  `note_rite_offence` inside `perform_rite_in_world`. This is the one
  signature accommodation the hooks needed — both are frozen-contract
  files, and a defaulted pointer is the smallest change that lets the gods
  notice every break and conviction without a wrapper verb.

### Deity mapping (which god is offended)

- **A broken oath offends Utu** (`utu`). Sworn oaths are witnessed by the
  sun who sees everything — the attested Mesopotamian oath-witness (Šamaš),
  grounded here in deities.csv's own row (the sun; the Retributors of Utu
  are his paladin guild). laws.csv oath_breaking: "before the god".
- **Tomb robbery offends the underworld queen** (`underworld_queen`) — the
  dead cult's own queen (deities.csv), matching laws.csv tomb_robbery: "the
  stones of the dead curse the violator".
- **Sacrilege offends the temple city's patron deity**: the first
  deities.csv row whose `cult_sites` names the crime's `place_city` (canon
  row order — the senior god listed first): nanna for the City of the Moon,
  enlil for the City of Kings. No identifiable cult, or no city on the
  crime: **An** (`an`), the father of the gods — the designer row's
  "generic divine ledger keyed by deity 'an'".
- **A rite offends the god it addresses**: the resolved target (an "any"
  rite's named god), else the row's own `deity` column. A rite that
  addresses no god (zisurru_warding, deity `any`, aimed at a place)
  offends nobody — there is no deity to offend.

### Amounts and thresholds (all retunable constants, sim/Divine.hpp)

| Choice | Constant | Value |
|---|---|---|
| oath broken | `kOathBreakWrath` | 15 |
| rite performed while impure | `kImpureRiteWrath` | 10 |
| rite performed and failed | `kFailedRiteWrath` | 5 |
| conviction for a divine offence | `kConvictionWrath` | 30 ("substantial" — more than any two other causes combined) |
| tier 1: ill omens begin | `kWrathIllOmenAt` | 20 |
| tier 2: disfavour begins | `kWrathDisfavourAt` | 50 |
| tier 3: the curse | `kWrathCurseAt` | 80 |
| favour penalty per tier ≥ 2 crossed | `kTierFavourPenalty` | 10 |
| perceived-favour penalty per tier in a divination | `kIllOmenFavourStep` | 10 |
| atonement wrath drop | `kAtonementWrathDrop` | 40 ("sharply" — always exits the curse tier from the 100 cap) |
| atonement favour gain | `kAtonementFavour` | 5 |
| decay period | `kWrathDecayDays` | 7 days per point ("slowly" — a conviction outlives a season of denial) |
| impurity floor for rows with no purity_required | `kImpureBelow` | 50 |

- **"Impure" for the rite offence** is purity below the rite's own
  `purity_required` (Magic.cpp reading 8's parse), **or below 50 when the
  row sets none** (every canon row but the atonement rite sets none, and
  purity's own canon range is 0..100 — 50 is the midpoint, favour's neutral
  birth value). An impure rite that also fails accrues both (15).
- **Escalation applies per tier crossed, upward only.** Crossing into
  tier 2 or 3 banks a −10 favour penalty with that deity; crossing into
  tier 3 lays the curse. Wrath falling back below a threshold (decay)
  applies nothing and never clears a curse.
- **Favour penalties land on the divine tick** (the next morning), not at
  accrual: the hooks in Faction.cpp and Justice.cpp have no writable
  favour state at their call sites, and the gods' displeasure hardening
  overnight reads right. Banked pending penalties are saved with the state.

### Consequences

- **Ill omens (tier ≥ 1):** the asker's divinations about that god compute
  the omen's truth from `favour − tier × 10` (Rites.cpp's divination hook),
  so a wrathful asker's signs read unfavourable more often. The omen
  record itself is unchanged (K-1's struct); only the truth it answers
  shifts. Omens stay probabilities, never certainties.
- **Disfavour (tier ≥ 2):** −10 favour with the offended deity per tier ≥ 2
  crossed, through Magic's own `add_favour` (the Rites favour system).
- **The curse (tier 3):** a condition on the offender, recorded in
  DivineState (`curse_by_offender`) and queryable over the C API
  (`sim_world_divine_cursed` returns the cursing deity). Its bite: the
  cursing god's wrath **does not decay** while cursed (time cannot lift
  what only atonement lifts) and the ill-omen skew runs at the full tier.
  It persists until a successful atonement rite addressed to that god.
  Faction's own `oath_breaker_curse` flag is untouched (still Magic's
  problem per the frozen contract); the divine curse is the bite itself.
- **Atonement:** `rites.csv su_ila_supplication` (INVENTED row, below). A
  successful performance addressed to the offended god drops his wrath by
  40, lifts his curse if wrath falls below 80, withdraws his banked favour
  penalty, and recovers 5 favour. With nothing to atone it is just a hymn.

### New canon rows (both INVENTED, tagged and sourced)

- **`db/canon/rites.csv su_ila_supplication`** — "Šu-ila (the raised-hand
  supplication)": the attested Mesopotamian penitential-Prayer genre
  (šu-ila, "raised hand") as the game's atonement rite. `deity=any`
  (addressed to the offended god by target, like the hymns),
  `materials=vocal performance` (intangible, like hymns_deity_names),
  `place=temples and shrines`, `purity_required=60` — the first canon row
  to gate on purity, on purpose: the supplicant comes to the god clean
  (mechanics.md row 20 "purity is load-bearing"; performing it impure
  accrues wrath). `effect_family=favour raising` so the existing K-1
  applier resolves the addressed god; the atonement effect itself keys on
  the rite id in Rites.cpp (`kAtonementRite`).
- **`db/canon/rite_teachings.csv teach_su_ila_supplication_priest`** —
  taught by the **priest of the sun** (people.csv
  ibni_shamash_priest_sun): the sun that witnesses every oath teaches the
  prayer that answers for breaking one. (Taught by the sun priest, not the
  moon priest, so K-1's `rites_taught_by(moon_priest) == 2` pin still
  holds.)

### Save format

- Four trailing save sections — `DIVINE_WRATH`, `DIVINE_CURSE`,
  `DIVINE_PENDING`, `DIVINE_DECAY_ACC` — optional on load (recognised by
  tag), so a save written before W5 still loads: the gods hold no grudge.
  No version bump.

### Deliberately left out

- **No per-npc favour, no city-level wrath pools** — MagicState is one
  performer's; a city-wide "gods are angry at the city" system is a
  designer decision, not a default.
- **No engine string tables touched** — wrath surfaces are ids and numbers
  over the C API; the codex/UI names for tiers are engine content.
- **The curse adds no new stat damage** — its bite is the frozen wrath +
  omens + favour surface above; inventing health/magic penalties would
  reach into Combat/Magic contracts.
