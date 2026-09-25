# SCENARIO CONTRACT — one rite, start to finish (T6)

**Test:** `kernel/tests/test_scenario_rite.cpp` — driven only through `sim::WorldState`
(`sim/World.hpp`) and the public `sim/Magic.hpp` API. All 17 kernel test suites are
green with this file added; `tools/canon_lint.py` and `tools/coverage_check.py` both pass.

## The rite chosen: `sacrifice_fish_sea_gems`

The only 4 rows in `db/canon/rites.csv` are `sacrifice_fish_sea_gems` (deity
`the_two_waters`), `hymns_deity_names`, `zisurru_warding`, `barutu_haruspicy` (the
latter three all `deity=any`). `sacrifice_fish_sea_gems` is the pick:

- It is the **only** canon rite with a real `deities.csv` id in its `deity` column.
  `deity=="any"` rites apply **no favour change at all** (`Magic.cpp` reading 3 —
  `RiteInputs` carries no field naming which god the performer addresses), so an
  `any`-deity rite cannot exercise the **favour** power on a named god at all — it
  was disqualified on that ground alone.
- The City of the Moon (D-005, the slice city) is canon's "capital... one of the
  oldest and richest (direct sea trade routes)... hub of trade" (`db/canon/cities.csv`).
  A rite to "the deities of salt and fresh water" is a plausible fit for a maritime
  capital, even though its home cult site in canon is "the first great temple
  (origin myth)" rather than a Moon-specific temple — the City of the Moon's own
  patron, Nanna (`db/canon/deities.csv`), has **no rite row at all** in the current
  4-row table, so no genuinely Moon-patron rite exists yet to choose instead.
- Its place requirement, `"the first great temple (origin myth)"`, matches the
  performer place tag `"temple:city_of_the_moon"` under `Magic.cpp`'s word-kind
  matching (`"temple"` is the shared kind) — this exact tag is already used by
  `test_magic.cpp`'s own fixtures for this very rite, so the scenario reuses a
  precedent already proven against the slice city rather than inventing a new one.

## What passes

- Refusal, unknown rite id (`unknown_rite`) and unlearned known rite
  (`rite_not_known`) — both leave `MagicState` untouched.
- Knowledge as a hard gate (rpg-systems §10.1 power 2): the attempt is refused,
  not merely scored lower, without it.
- Materials, place and (the always-satisfied case of) time each independently
  move the score by their documented weight (0.20 / 0.10 / 0.10).
- Favour (rpg-systems §10.1 power 1): absent → neutral 50; a performed rite
  moves it; +2 on success, net −3 on failure (+2 performed, −5 angered), pinned
  to the exact verified `Rng{seed}.fork(day).unit()` draws `test_magic.cpp`
  already establishes for this rite (seed 1 day 5 → success; seed 42 day 5 →
  failure).
- Purity carried but non-gating exactly as D-011 describes: identical score
  (and therefore identical outcome) at `purity=100` and `purity=0`, since
  `rites.csv` sets no `purity_required` value for this row and the fixed
  formula's default requirement is 0.
- Determinism: two identically-seeded worlds run the same script and land on
  byte-identical `MagicState` and identical `RiteResult`s.

## The five powers, as far as the kernel supports them

| Power (rpg-systems §10.1) | Exercised? | Note |
|---|---|---|
| Favour | Yes | Only reachable at all because this rite names a real deity; the other 3 canon rites cannot exercise it (see above). |
| Knowledge | Yes, as a gate | The kernel has **no persistent "known rites" state** — see gap 1 below. |
| Materials | Yes | Presence-only, no quantities; see gap 2 below for consumption. |
| Purity and place | Place: yes. Purity: proven non-gating, not proven gating — no canon row carries a `purity_required` value to gate against (gap 3). |
| Time | Trivially only | No canon rite sets a real `time_window`; see gap 4. |

## Missing kernel surfaces for the slice rite (report only — not built here)

1. **No persistent rite-knowledge state.** `RiteInputs::performer_knows_rite`
   is, per `Magic.hpp:45`, "learned from text or teacher (**caller tracks**)" —
   `MagicState` has no `known_rites` set or similar. The scenario models
   "learning" the only way the kernel allows: the caller flips a local bool on
   its own `RiteInputs` between calls. A real engine needs a durable "rites
   known by this character" surface (likely on `PopulationState` or a new
   per-NPC/player field) so knowledge persists across days without the
   engine layer re-deriving it from nothing every tick.
2. **Materials are not consumed from any inventory.** `Magic.hpp:22-23`
   is explicit: "no effect application here — the caller reads RiteResult and
   applies the outcome"; `RiteInputs::materials_held` is a `const`-read
   snapshot the caller constructs, and `perform_rite` never debits it. D-011
   deferred "offerings consumed from inventory" with content; at the kernel
   level there is additionally no cross-module hook from `MagicState` to
   `PropertyState`/an inventory surface at all — a real rite needs the engine
   (or a future module) to debit the performer's goods on every attempt
   (`performed==true`), successful or not (Magic.hpp:16: "a failed rite still
   consumes the materials").
3. **Purity gating has no data to gate on.** `rites.csv`'s `purity_required`
   column (added for D-011/D-012) is empty on all 4 rows, so the fixed
   formula's `purity >= rite's purity_required (default 0)` term is always
   true regardless of the performer's actual purity value — the kernel *code*
   supports gating (the comparison exists in `Magic.hpp`/`Magic.cpp`), but no
   canon content exercises it, and this scenario can only prove the
   non-gating default, never the gated path, without inventing a canon value.
   This is a content gap, not a kernel one, but it means the slice rite's
   purity mechanic is undemonstrated end-to-end today.
4. **Time is undemonstrated beyond the trivial always-true case.** No canon
   rite sets a `time_window` mentioning "festival" (`Magic.cpp` reading 7:
   "the column is empty in all canon rows today"); the only place the
   festival-gated branch is exercised at all is `test_magic.cpp`'s own
   fixture canon (test scaffolding, explicitly "not canon and ships
   nowhere"). A slice rite that is meant to show the calendar mattering needs
   either a canon rite with a real `time_window`, or `calendar.csv`'s
   `festival_days` OPEN row resolved (currently OPEN — cannot ship per
   `canon_lint.py`) — both are content/designer decisions, not kernel work.
5. **No omen/effect application.** `RiteResult.effect_family` is a string tag
   pulled straight from `rites.csv` (`"offering (favour)"` for this rite);
   nothing in the kernel interprets it. Per `Magic.hpp:22`, "the EFFECT is the
   game (INVENTED: EFFECT); no effect application here — the caller reads
   RiteResult and applies the outcome." For the other three canon rites this
   gap is sharper: `zisurru_warding` ("protection"), `barutu_haruspicy`
   ("divination (omens with probabilities)") and `hymns_deity_names` ("favour
   raising") all name effect families with no kernel-side mechanic at all —
   no ward duration/zone tracking, no omen generation with the documented
   probabilistic hedging (rpg-systems §10.3 divination: "omens with
   probabilities, not certainties"), no favour-raising path distinct from the
   generic rite-performance +2 already covered by `Magic.hpp`. This is the
   single largest gap for "one rite start to finish": the kernel can tell you
   a rite *succeeded*, never what that success *did* to the world.

## Not exercised, out of scope for this rite

- The other four effect families (healing/purification, curse and binding,
  substitution, ancestors, divine intervention) have no canon rite at all
  yet, so they are untouched by any scenario, not just this one.
- Sacrifice's "the two-waters worship" tradition slot / practitioner roles
  (rpg-systems §10.2) have no kernel representation; the scenario performs
  the rite as an anonymous "performer," matching what `MagicState` actually
  models (no performer-identity field beyond the implicit "the state passed
  in").
