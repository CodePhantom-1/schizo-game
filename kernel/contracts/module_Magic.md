# MODULE CONTRACT — Magic

**Status:** Wave 1 agent; K-1 extended. The five powers of a rite (favour/knowledge/materials/purity+place/time); the fixed success formula is in the header.

**Owns:** MagicState only
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Magic.cpp implements every declaration in include/sim/Magic.hpp; tests/test_magic.cpp passes; determinism holds (same seed -> same state bytes).

## K-1 — rites: knowledge, offerings, effects

Magic.cpp still writes **only MagicState**, and the fixed success formula and
`perform_rite` are unchanged. K-1 adds, additively:

- **Knowledge state:** `MagicState::known_rites` (`std::set<Id>`), with
  `knows_rite()` / `learn_rite()`. `perform_rite` still reads the gate from
  `RiteInputs::performer_knows_rite`; the caller fills it from `knows_rite()`.
  `learn_rite` does not check canon — the teaching verbs do.

The caller side of the loop (the outcome Magic.hpp leaves to the caller) is
`sim/Rites.hpp` / `src/Rites.cpp`, a WorldState& verb layer like
`sim/Actions.hpp`:

| Step | Verb | Writes |
|---|---|---|
| learn from a teacher | `learn_rite_from_teacher(w, rite, npc)` — the npc's schedule role must be listed for the rite in `rite_teachings.csv` (`via=teacher`) | MagicState (via `learn_rite`) |
| learn from a text | `learn_rite_from_text(w, rite, item)` — the item must be listed (`via=text`) and held in `inventories["player"]`; read, not consumed | MagicState (via `learn_rite`) |
| perform | `perform_rite_in_world(w, rite, target)` — refusals first (Magic's `unknown_rite`/`rite_not_known`, then `no_deity_addressed`/`unknown_deity`/`no_place_to_ward`), then RiteInputs from knowledge + inventory, then Magic's `perform_rite` | MagicState (via `perform_rite`, `add_favour`) |
| offerings | one unit of each item-material debited on every **performed** rite, success or failure; intangible materials (no items.csv row) are the knowing performer's own | `WorldState::inventories["player"]` |
| effects | on success only, by `effect_family`: offering (+5 favour), favour raising (+3 to the addressed god), protection (ward on a place for 30 days), divination (an omen, 75% true) | MagicState (favour), `WorldState::rite_effects` |

Also on the caller side: a `deity=any` rite that addresses a named god gets
Magic.hpp's own +2 performed / −5 failed deltas applied to that god (Magic.cpp
reading 3 cannot resolve the god; the caller can).

Every effect value is `INVENTED: EFFECT` — ledger:
[docs/proposals/invented-ledger-rites.md](../../docs/proposals/invented-ledger-rites.md).

## D-022 — integer score, one roll per rite

The designer's D-022 replaces the floating-point score and the shared daily
draw (Magic.hpp holds the formula):

- **Score in basis points** (`RiteResult::score_bp`, 0..10000): favour
  `favour * 4000 / 100` (integer), materials 2000, purity 2000, place 1000,
  time 1000. No floating point touches the outcome, so a rite resolves the
  same on x86 and ARM64 builds. `RiteResult::score` stays as
  `score_bp / 10000.0` for display.
- **One roll per rite:** `ctx.rng.fork(day).fork(stable_hash(rite_id)).below(10000)`,
  success when `roll < score_bp`. `stable_hash` is FNV-1a 64 over the id's
  bytes (sim/Rng.hpp; never `std::hash`), and `Rng::below(n)` is an
  unbiased bounded draw (rejection sampling), both new in Rng.hpp. Two rites
  on the same day now roll independently. Retrying *the same* rite on the
  same day still gets the same roll (the salt is the rite, not the attempt).
- **Purity:** the purity term reads `rites.csv` `purity_required`; blank is
  the default 0 (kernel review follow-up, 2026-09-25).

Tests: test_magic (`test_d022_*`, `test_purity_required_column_gates_the_purity_term`),
test_scenario_rite (`test_d022_rolls_survive_save_load`). Pinned fixtures
were re-derived against the new rolls (seed 42 → 14 for the failure cases);
the assertions are unchanged or stricter.

**Time:** festival-demanding rites keep using `Calendar::is_festival` (Magic.cpp
reading 7); K-1 adds no calendar surface.

**Save:** `known_rites`, wards and omens are saved in the trailing
`MAGIC_KNOWN` / `RITE_WARDS` / `RITE_OMENS` sections (module_Snapshot.md).

**C API:** `sim_world_knows_rite`, `sim_world_known_rites`,
`sim_world_learn_rite_from_teacher`, `sim_world_learn_rite_from_text`,
`sim_world_rites_taught_by`, `sim_world_rites_taught_in`,
`sim_world_set_rite_place`, `sim_world_rite_place`, `sim_world_purity`,
`sim_world_set_purity`, `sim_world_perform_rite`, `sim_world_ward_until`,
`sim_world_warded`, `sim_world_omen_count`, `sim_world_omen` (CApi.h).

**Tests:** test_magic (`test_rite_knowledge_state`), test_scenario_rite
(`test_k1_*`: the loop for all four canon rites, failure consumption,
refusals leave no trace, determinism + save), test_snapshot
(`test_k1_rite_state_round_trips`, incl. a pre-K-1 save), test_capi
(`test_rites_through_c`).
