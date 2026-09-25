# Invented ledger — K-1 (rites that matter)

D-018 creative gap-filling, applied to close `kernel/contracts/scenario_rite.md`'s
gaps 1-5. Every choice below is INVENTED machinery or content that fits the
theme and canon and contradicts no `CANON`/`A` row. Veto any entry by
superseding it here or in `DECISIONS.md`.

## Data (`db/canon/rites.csv`)

- **`zisurru_warding.purity_required = 40`.** A warding rite over people and
  places calls for a purer exorcist than an ordinary offering (rpg-systems
  §1.2/§10.1 power 4). `sacrifice_fish_sea_gems` and `hymns_deity_names` were
  deliberately left with an empty `purity_required` — both are pinned by
  pre-existing tests (`test_scenario_rite.cpp`, `test_magic.cpp`) that assert
  purity is non-gating for them; touching those numbers would have broken a
  green gate for no gain, so gap 3 is demonstrated on the two rows free of
  that constraint instead.
- **`barutu_haruspicy.purity_required = 30`, `time_window = "a festival day"`.**
  Extispicy read for the temple calendar's high days (rpg-systems §10.1 power
  5; `calendar.csv:festival_days`, D-015/D-018). This is the first canon rite
  with a real festival-gated `time_window` (scenario_rite.md gap 4).
  **Follow-up finding, not fixed here:** `WorldState::init` (`kernel/src/World.cpp`)
  never loads `calendar.csv`'s `festival_days` row into `CalendarConfig` — its
  own comment still says "festival days stay OPEN", which is stale against
  D-015/D-018's real content. A real `SimWorld`/`WorldState` therefore never
  sees a festival day today, so this rite's hard time-gate is provably real
  but not yet reachable end-to-end outside a test that overrides `WorldState::cal`
  directly (see `test_scenario_rite_action.cpp`) or a C API caller who can't
  reach it at all. `World.cpp` is coordinator-owned; parsing the row's prose
  ("day 1 New Waters...; day 181...") into day numbers is a follow-up, not a
  K-1 change.
- **New row `gula_healing_rite`** (healing and purification family). No notes
  line names this rite, so the whole row is INVENTED — built from a real,
  grounded tradition (rpg-systems §10.2, the āšipu exorcist-physician) and a
  real deity already in canon (`deities.csv:gula`, cult site
  `city_of_the_moon`, D-005's slice city) rather than invented from nothing.
  Exists so the healing/purification effect family (rpg-systems §10.3) has a
  real canon rite to exercise, since none of the original 4 rows carry it.

## Mechanism (kernel)

- **`MagicState::known_rites` is a single set, not per-actor.** MagicState
  already models one implicit performer — `favour_by_deity`, `purity` and
  `place` carry no actor key (scenario_rite.md's own "not exercised, out of
  scope" note). Knowledge follows the same shape rather than inventing a
  per-actor map nothing else in the module supports yet.
- **Material consumption: presence-only, -1 per required key per performed
  attempt**, success or failure (Magic.hpp: "a failed rite still consumes the
  materials"; Magic.cpp reading 5: "quantities are not modeled"). Consuming
  more than 1 per attempt would invent a quantity the canon materials column
  doesn't carry.
- **Ward default duration: 7 days** (`kWardDurationDays`, `Actions.hpp`).
  rpg-systems §10.3 says protection "lasts hours or days" with no number.
- **Healing/purification: flat +30 relief / +30 purity** (`kHealingRelief`,
  `kPurificationBoost`). rpg-systems §10.3 names the effect, not a number.
- **Curse and binding: -15 purity to the performer on every successful
  attempt** (`kCursePurityCost`), plus a "curse"-kind Ward on the performer.
  rpg-systems §10.3: "sorcery is a crime... being caught means trial, exile
  or death" — no canon rite exercises this family yet, so the cost lands on
  the performer unconditionally (INVENTED) rather than guessing at a
  detection/trial mechanic that belongs to Justice, not Magic.
- **Divination reading bands: score >= 0.70 "favourable", >= 0.40
  "uncertain", else "ill-favoured".** rpg-systems §10.3: "omens with
  probabilities, not certainties" — the coarse banding is INVENTED; the
  stored `Omen.probability` is always the rite's own exact score, so nothing
  is lost for a caller that wants finer resolution.
- **`favour`/`offering` effect families apply no additional effect** beyond
  the +2/-5 swing `sim::perform_rite` already applies (Magic.cpp, unchanged).
  Substitution, ancestors and divine intervention are left untouched — no
  canon rite names these families yet (scenario_rite.md: "not exercised, out
  of scope, not just this scenario").
