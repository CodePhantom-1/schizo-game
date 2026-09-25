# Invented ledger — K-1 (rites: knowledge, offerings, effects)

Per D-018 (creative gap-filling authorized; every invented item ledgered so the
designer can veto it) and README working rule 4 (`INVENTED: EFFECT` — the rite
is canon, that it *works* is the game's world). One entry each: what, where,
why it fits theme and canon. None contradicts a CANON/A row; every number is a
retunable constant. Code: `kernel/include/sim/Rites.hpp`,
`kernel/src/Rites.cpp`, `kernel/include/sim/RiteEffects.hpp`.

## What is imported, not invented

- **Knowledge is a gate, learned from teachers or texts.** rpg-systems §2.1
  (mechanics.md row 21: skills grow "by use/teachers/texts"); rpg-systems
  §10.1 power 2 (knowledge of the rite); game-design §7.1 ("texts to read —
  Scripts and Rites knowledge"); Magic.hpp's own `RiteInputs` comment
  ("learned from text or teacher"). The gate was already D-011-ratified
  (`rite_not_known`). K-1 only gives it persistent state
  (`MagicState::known_rites`) and two ways in.
- **A failed rite still consumes the materials** (Magic.hpp, fixed).
- **The effect families** are rpg-systems §10's eight (game-design §8.1);
  the four canon rites name four of them in `rites.csv effect_family`.
- **Omens are never certainties** (game-design §8.1 guard rails;
  rpg-systems §10.3).

## Invented (K-1)

### Knowledge

- **`db/canon/rite_teachings.csv` (new table, 4 INVENTED rows).** Who or what
  teaches each canon rite. `via=teacher` names a schedules.csv role (matched
  against a Population npc's role); `via=text` names an items.csv id the
  reader must hold.
  - *priest of the moon* teaches `sacrifice_fish_sea_gems` and
    `hymns_deity_names`. The temple of sun and moon is the slice city's temple
    (D-005; buildings.csv) and its priests already sing the offerings
    (schedules.csv `moon_temple_evening_rite`); a sea-trade capital's temple
    is the natural keeper of the offering to the two waters (wb §3; notes
    L57), and hymns belong to "temples and shrines" (rites.csv).
  - `zisurru_incantation_tablet` teaches `zisurru_warding`: exorcism is a
    written craft in the real record, and deities.csv already places the
    temple's exorcist-physicians in its scribal-school slot (gula row).
  - `clay_liver_model` teaches `barutu_haruspicy`: the real Old Babylonian
    teaching models of extispicy.
- **Two new text items (items.csv):** `zisurru_incantation_tablet`
  (INVENTED, price band 3) and `clay_liver_model` (tagged `A` — the object is
  real: e.g. British Museum BM 92668; the Mari liver models).
- **A text is read, not consumed**, and **reading is not gated by literacy**
  yet — rpg-systems §2.5's languages & scripts have no kernel state. When
  they do, `learn_rite_from_text` is where the gate goes.
- **Teaching is free and ungated** (no fee, no standing or rank check). The
  parent's teacher mechanics give no number to import; a fee would be a
  price-model constant for the designer.

### Offerings

- **One unit of each material per performance.** rites.csv carries no
  quantities (Magic.cpp reading 5: presence only), so one of each is debited.
- **Intangible materials.** A material that names no items.csv row
  (`hymns_deity_names`: "vocal performance; the deity's true name") is
  supplied by the performer who knows the rite, and never debited.
- **`a_burned_goat's_liver` item row (CANON, notes L202).** The barutu
  material needed an item to be carried and consumed; the id follows the
  D-011 rites-material normalization exactly as `different_types_of_flour`
  did.
- **The performer is the player** (`kRitePerformer = "player"`):
  MagicState is one performer's magic (game-design §1), so offerings come
  from `inventories["player"]`.

### Effects (`INVENTED: EFFECT`)

Applied by `perform_rite_in_world` only when the rite **succeeds**. A failed
rite does nothing to the world beyond Magic's own −5 and the spent offering.

| rites.csv effect_family | Family | Effect | Constant |
|---|---|---|---|
| offering (favour) | offering | extra favour with the rite's god (on top of Magic's +2) | `kOfferingFavour = 5` |
| favour raising | favour raising | favour with the god the hymn addresses | `kHymnFavour = 3` |
| protection | protection | a ward on a place tag (the target, or the performer's place) for N days; re-warding extends, never shortens | `kWardDays = 30` |
| divination (omens with probabilities) | divination | an omen on "does this god look on me with favour" (true answer: favour ≥ 50, the neutral birth value); the sign reads true N% of the time, drawn from `rng.fork(day ^ kOmenSalt)` | `kOmenTruthPct = 75` |

- **"any" rites address a named god.** Magic.cpp reading 3 cannot resolve
  `deity=any` (RiteInputs names no god), so Magic applies no favour. The
  applier takes the god as the rite's target and applies Magic.hpp's own
  +2 (performed) / −5 (failed) to him, plus the family effect. An "any"
  rite of a god-addressing family refuses with `no_deity_addressed` when no
  god is named; an unknown id refuses with `unknown_deity`. Refusals spend
  and change nothing.
- **A divination may ask about another god** than the rite's own; an
  offering to a named god always goes to him.
- **Wards do nothing else yet.** They are queryable world state
  (`ward_holds`, `sim_world_warded`) for the engine and later systems (the
  restless dead, demons, city-life §6); what a ward *repels* is the
  designer's to decide.
- **The other four families** (healing and purification, curse and binding,
  substitution, ancestors, divine intervention) have no canon rite, so no
  applier: an unmapped family applies nothing.

### Save format

- Three trailing save sections — `MAGIC_KNOWN`, `RITE_WARDS`, `RITE_OMENS` —
  optional on load, so a save written before K-1 still loads (nothing known,
  no ward, no omen). No version bump.
