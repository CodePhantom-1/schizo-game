# MODULE CONTRACT — Character (W4-A: character progression)

**Status:** W4-A (wave 4, D-021). mechanics.md rows 20-24, and the literacy half of row 25:
six attributes (1-10), skills in six groups (0-100) grown by use / teachers / texts, levels (XP
from skill points, cap 40, one talent per level), callings (choose at 5, specialise at 15,
second calling at 25, +50% growth), social rank per polity (ranks.csv, 0 the Outsider to 6
Lugal).

**Files:**
- `include/sim/Character.hpp`, `src/Character.cpp`: the pure part. The canon catalog
  (`ProgressionCatalog`, from attributes/skills/callings/talents.csv), the player's sheet
  (`CharacterState`), the NPC light sheet (`NpcSheet`), and the functions that read and change
  them. They take no `WorldState`.
- `include/sim/Progression.hpp`, `src/Progression.cpp`: the world verbs. This is a `WorldState&`
  layer like Actions.cpp and Rites.cpp, covering practice, teachers, texts, work, trade and rank,
  plus the use hooks and NPC sheet seeding.

**Owns:** `WorldState::character` (the player's sheet), `WorldState::npc_sheets` (id to light
sheet) and `WorldState::progression` (the catalog, rebuilt from canon by `init`, never saved).
**Writes, only through each module's own free functions:** the purse (`credit_purse` /
`take_from_purse`: fees, wages, trade), market stock (`consume` / `deliver`), needs
(`advance_needs`: a session of practice, lessons, study or work costs its waking hours) and the
inventories (wages, trade).
**Never:** reads act, story or quest state (D-021). Draws randomness or reads a wall clock.
Uses a float in anything that affects state (D-022: integers and basis points only). Writes
from a module tick. Changes another module's contract.

## Rules (imported: mechanics.md rows 20-24)

| Rule | Where |
|---|---|
| 6 attributes, 1..10 | `attributes.csv` (6 rows); `attribute()` clamps base + bonus to 1..10 |
| skills 0..100 in 6 groups | `skills.csv` (27 rows; groups arms, craft, commerce, letters, field, sacred) |
| grow by use / teachers / texts | `note_use` / `note_skill_use`, `train_with_teacher`, `study_text` |
| XP from skill points, cap 40 | every skill point is one xp; `xp_for_level(L) = 5(L-1) + (L-1)L/2` |
| 1 talent per level | `apply_levels` gives one talent point per level |
| callings at 5 / 15 / 25 | `choose_calling`, `choose_specialisation`, `choose_second_calling` |
| +50% growth | `kCallingGrowthBp = 5000` for every skill of a calling or specialisation held |
| rank per polity, 0..6 | `CharacterState::rank_by_polity`; `raise_rank` (deeds and a patron's act); outlawry sets 0 |
| skill decay | **none**: the imported spec names no decay, so none is invented |

## INVENTED machinery (retunable constants; ledger: docs/proposals/invented-ledger-character.md)

- **Growth, in basis points.** A skill point from `value` costs `5 + value/2` xp. An xp is scaled
  by `growth_bp`: 10000, plus or minus 1000 per point of the governing attribute away from 5,
  plus 5000 if the skill is favoured by a calling held, plus talent and perk `growth_bp`. The
  floor is 1000. Progress is stored in xp·bp, so nothing is ever rounded away.
- **Attributes grow by exercise.** Every skill point adds one exercise to the skill's governing
  attribute. At `12 × (value+1)` exercise the attribute rises by one, up to 10. Talents and perks
  add on top of that, and the total is clamped to 10.
- **Effect grammar** (talents `effect`, specialisation `perks`): `skill_bonus:<skill>:<n>`,
  `growth_bp:<skill>:<bp>`, `attribute:<attr>:<n>`, `trade_bp:<bp>`, `wage_bp:<bp>`,
  `fee_bp:<bp>`, `study_bp:<bp>`, `rite_favour_bp:<bp>`. A row that fails to parse is skipped,
  and a test asserts that every canon row parses.
- **Practice:** 2 xp per hour, alone, only up to 25. **Teachers:** 5 xp per hour up to the
  teacher's own skill, for `fee_per_hour` silver paid to the teacher. **Texts:** 4 xp per hour
  (plus `study_bp`) up to the row's `max_level`. Reading needs scribal arts 10 and grows scribal
  arts by 1 xp per hour. **Work:** 2 xp per hour to each skill the job lists; the wage is
  `wage_qty` of `wage_item` per `per_hours` hours (`silver` goes to the purse), times
  `wage_bp`. A session is 1-12 hours, needs the player not to be exhausted (Needs' own
  `exhausted` effect) and advances his needs.
- **Use hooks:** craft `3 × recipe hours × times`; a performed rite 4 xp (8 on success); a meal
  eaten while hungry 1 xp; trade `2 + silver/20` xp (at most 10); an unseen crime 6 xp.
- **Trade:** buying costs `price × qty × (10000 − off)/10000`, where `off` is 10 bp per
  bargaining point, plus `trade_bp`, plus 100 bp per rank tier with the city's polity, capped at
  5000. Every unit costs at least 1. Selling pays `price × qty × (5000 + 20 bp per bargaining
  point + trade_bp + rank)/10000`, capped at 9000. Stock must be on the shelf, and the market
  must be open (a festival can close it).
- **Rank:** tier `t` needs standing `kRankStanding[t]` = {0, 10, 25, 45, 60, 75, 95} with the
  polity, plus a patron: a resident whose faction is that polity. Tier 6 needs no patron. The
  player rises one tier at a time. An outlawed polity refuses, and an exile or death verdict
  (Actions.cpp) drops the rank to 0.
- **NPC light sheets:** seeded at `init` from each resident's role. A worker knows each skill of
  its job at 30. A teacher knows each skill it teaches at the row's `max_level`. Attributes are
  5. Only the player's sheet grows through these verbs.

## Hooks for the other wave-4 tracks (no contract change needed)

- **W4-B combat:** reads `attribute(state, attr)` and `effective_skill(state, skill)`
  (`CharacterState` or `NpcSheet`), or `actor_attribute` / `actor_effective_skill(w, actor, id)`,
  as plain integers. It grows a weapon skill with `note_skill_use(w, actor, skill, xp)`, and
  treating wounds with `note_use(w, actor, "verb:treat", xp)`.
- **W4-C wild lands:** `note_use(w, actor, "verb:hunt" | "verb:fish" | "verb:travel_by_boat" |
  "verb:travel_wild", xp)`.
- **W4-D politics:** reads `rank(state, polity)`. Standing is Faction's; `raise_rank` reads it.

## Snapshot

Trailing optional sections, detected by tag (`Reader::peek_tag`), so a save from before W4-A
loads as a fresh level-1 prisoner with residents seeded from their roles:
`CHAR_CORE level xp talent_points calling specialisation second_calling`, then `CHAR_ATTRS`
(attr, base, exercise), `CHAR_SKILLS` (skill, value, progress), `CHAR_TALENTS`, `CHAR_RANKS`,
`CHAR_NPCS` (npc, n_attr, n_skill, then rows). Derived bonuses are rebuilt on load.

## C API (additive; CApi.h "Character progression (W4-A)")

Reads: `sim_world_attribute`, `sim_world_skill`, `sim_world_effective_skill` (player or npc),
`sim_world_level`, `sim_world_character_xp`, `sim_world_xp_for_next_level`,
`sim_world_talent_points`, `sim_world_talents`, `sim_world_talents_available`,
`sim_world_calling(slot)`, `sim_world_skill_ids`, `sim_world_skill_teachers`,
`sim_world_skills_taught_by`, `sim_world_purse`, `sim_world_buy_quote`, `sim_world_sell_quote`,
`sim_world_rank`, `sim_world_character_summary` (readable text for the UI) and
`sim_world_progression_refusal` (the exact reason for the last refusal).
Verbs: `sim_world_choose_talent`, `sim_world_choose_calling`, `sim_world_choose_specialisation`,
`sim_world_choose_second_calling`, `sim_world_practice_skill`, `sim_world_train_skill`,
`sim_world_study_skill`, `sim_world_work`, `sim_world_buy`, `sim_world_sell`,
`sim_world_raise_rank`. They share one set of refusal codes (-1 null, -2 unknown id,
-3 requirement not met, -4 nothing more to gain, -5 cannot pay / not held, -6 exhausted,
-7 refused by the other side), and a refusal changes no state.

## Definition of done

- `tests/test_character.cpp` covers:
  - the canon catalog is whole, and every skill has a use path;
  - the growth maths, levels, callings and talents (every refusal);
  - attribute growth, and that skills do not decay;
  - practice, teachers, texts, work and trade, including prices, stock and the market-closed
    festival;
  - rank with outlawry;
  - the craft, eat, rite and unseen-crime hooks, and the rite-favour talent;
  - Snapshot round-trip, loading an old save, and rejecting a truncated one;
  - determinism and the C API surface.
- `tests/test_scenario_progression.cpp` is the D-021 proof. A fresh player, through the C API
  verbs alone, reaches every calling (as first and as second calling), every specialisation and
  every talent. The same play run twice produces the same save bytes.
