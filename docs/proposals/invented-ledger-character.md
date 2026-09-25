# Invented ledger — W4-A (character progression)

This ledger follows D-018, which authorizes creative gap-filling as long as every invented item is
ledgered so the designer can veto it. It also follows D-021 (open play: wave 4 builds character
progression, and nothing gates on story) and D-022 (integer and basis-point maths). Each entry
says what was invented, where it lives, and why it fits the theme and the canon. None of it
contradicts a CANON or A row. Every number is a retunable constant, and every row is tagged
`INVENTED` with a `source_ref`.

The code is in `kernel/include/sim/Character.hpp`, `kernel/src/Character.cpp`,
`kernel/include/sim/Progression.hpp` and `kernel/src/Progression.cpp`. The contract is
`kernel/contracts/module_Character.md`.

## What is imported, not invented

These come from mechanics.md rows 20-24:

- six attributes, range 1-10
- skills in six groups, 0-100, grown by use, teachers and texts, including the Sacred group
  (Rites, Divination, Incantation)
- levels from skill points, capped at 40, with one talent per level
- callings chosen at 5, a specialisation at 15 and a second calling at 25, with +50% growth
- social rank per polity, 0 to 6, rising by deeds plus a patron's act and dropping to 0 on
  outlawry

The rank ladder is canon (ranks.csv, D-015). Row 18 says skills do not pass to an heir; that is
not in scope here. **Skill decay is not implemented**, because the imported spec names none.

## Invented (W4-A)

### Canon tables

| # | What | Where | Why it fits |
|---|---|---|---|
| 1 | **The six attribute names:** strength, agility, endurance, wits, perception, presence | `db/canon/attributes.csv` (new, 6 rows) | Row 20 gives six attributes and no names. These cover the bronze-age body and voice: hauling and the mace, the potter's wheel, the drought-land day, the scribe's signs, reading a liver, vibrating a god's name. |
| 2 | **27 skills in six groups:** arms, craft, commerce, letters, field, sacred | `db/canon/skills.csv` (27 rows; was header-only) | Row 21 gives the six groups and names the Sacred skills. The rest come from canon content: the crafts from recipes.csv and items.csv, trade from row 8, letters from row 25 and the tablet house, field work from the notes' fields of wheat and barley (L57) and the sea trade, and combat from the War Age. Each skill has a governing attribute and `grows_by` use tokens. |
| 3 | **Eight callings, 18 specialisations** | `db/canon/callings.csv` (26 rows; was header-only) | Row 23 already names the fits. The Exorcist and Diviner specialisations "fit canon directly", and the Old Woman and Lector-priest slots are filled: the Old Woman is herb-wife and midwife under the Healer, and the Lector-priest is the singer of hymns under the Priest (round-2-invented.md's suggestions). The Retributors of Utu stay a faction; their calling question is still the designer's. Each specialisation carries a real perk. |
| 4 | **41 talents**, each with a mechanical effect | `db/canon/talents.csv` (41 rows; was header-only) | Row 22 gives one talent per level and no list. These are six attribute talents, five general ones, and two to four per calling or specialisation, with names drawn from the setting (Ninkasi's measure, the flour circle, watcher of the liver, the sexagesimal mind). |
| 5 | **Teachers and texts:** who teaches what, the fee per hour, and the cap | `db/canon/skill_teachings.csv` (new, 43 rows) | Row 21's "teachers/texts". Every teacher is a resident role already on the street (D-020): the watch teaches the spear, the baker cooking, the scribe the signs, the priests of the moon rites and incantation, and the priest of the sun divination (Shamash and Adad as lords of the omen, A). The three texts are existing items: the zisurru tablet, the clay liver model, and the underworld ritual scroll. |
| 6 | **Work for wages** | `db/canon/work_roles.csv` (new, 19 rows) | rpg-systems §6 has "rations as wages" (row 28). This is how the prisoner earns his first grain and silver with no gate. Wages are paid in grain, fish, bread, flour or beer (the employer's own goods), or in silver. Only the scribe's work needs the player to be able to read already. |
| 7 | **Three craft recipes and their stations:** throw_pot (clay at the potter's wheel), weave_woolen_cloth (wool at the loom), cast_bronze_sickle (copper and tin at the smithing hearth); new items clay, potters_wheel, loom, smithing_hearth | `db/canon/recipes.csv` (+3), `db/canon/items.csv` (+4) | These give pottery, weaving and metalwork a use path, using outputs that were already canon items (pottery, woolen_cloth, bronze_sickle). The wheel, the loom and tin bronze are attested for the period (A). The recipes are INVENTED glue. |

### Machinery (constants in Character.hpp and Progression.hpp)

| # | What | Value |
|---|---|---|
| 8 | Skill-point cost | `5 + value/2` xp |
| 9 | Level curve | level L to L+1 costs `5 + L` skill points (level 5 at 30 points, 25 at 420, 40 at 975) |
| 10 | Governing attribute's effect on growth | ±1000 bp per point away from 5; growth never below 1000 bp |
| 11 | **Attribute growth by exercise** | an attribute rises by 1 after `12 × (next value)` skill points under it (5 to 6 takes 72), natural cap 10 |
| 12 | Practice | 2 xp/h, alone, only up to 25 |
| 13 | Teacher | 5 xp/h up to the teacher's own skill; the fee is paid to the teacher's purse |
| 14 | Text | 4 xp/h up to the row's cap; reading needs scribal arts 10 and grows scribal arts by 1 xp/h |
| 15 | Work | 2 xp/h to each listed skill, plus the wage |
| 16 | Use hooks | craft 3 xp per recipe-hour; rite 4 xp (+4 on success); a meal eaten hungry 1 xp; trade `2 + silver/20` (max 10); an unseen crime 6 xp (stealth) |
| 17 | Session | 1-12 hours; refused while "exhausted" (Needs' own threshold); the hours advance hunger, thirst and fatigue |
| 18 | Market prices | buying: −10 bp per bargaining point; selling: 50% of price +20 bp per point; talents' trade_bp; +100 bp per rank tier with the city's polity (city_faction); discount capped at 50%, sale capped at 90% of price |
| 19 | Rank thresholds | standing {0, 10, 25, 45, 60, 75, 95} for tiers 0-6; the patron must be a resident of that polity; Lugal (6) needs only the standing |
| 20 | Talent and perk effects on rites | `rite_favour_bp` multiplies the favour of a successful offering or hymn (Rites.cpp; the K-1 constants are unchanged) |
| 21 | NPC light sheets | a worker knows its job's skills at 30; a teacher knows what it teaches at the row's cap; attributes are 5 |

## Not decided here (left to the designer or other tracks)

- **Languages and speaking** (row 25). Only literacy is modelled, as scribal arts gating texts.
  Which languages exist and which script is undeciphered stay `OPEN`.
- **The Retributors of Utu as a calling** (row 23). They are still only a faction.
- **Skills and outcomes.** The Magic success formula is unchanged (the Sacred skills grow by
  rites but do not yet weight them), and combat maths is W4-B's.
- **Heirs** (row 18). Skills do not pass; nothing here touches heirs.
