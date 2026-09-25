# INVENTED LEDGER — D-020 part 1, every schedule role has a resident

Per D-018 and D-020 ("the game must be as complete and alive as possible"): every choice below is authored glue tagged `INVENTED` (new names are `A`, real attested names). It fits theme and canon, contradicts no CANON/A row or the notes, and is subject to designer veto (a veto is appended to DECISIONS.md and supersedes only that line). No CANON row was changed.

**The recount.** Roles are matched the way the kernel matches them: `schedule_roles()` (kernel/src/Schedule.cpp) collects the roles of non-`OPEN` schedules.csv rows, trimmed and compared case-insensitively, and `seed_people()` (kernel/src/Population.cpp, `matching_schedule_role`) gives a resident that role only on the same trimmed, case-insensitive match. After the `midday_rest` split (craftsman + household, both already held) and the K-2 festival rows (priest of the moon, tavern keeper, both already held), the count is still **seven** orphan roles: herdsman, priest of the sun, paladin, fisherman, field hand, lighthouse keeper, dockworker. Nine schedule rows never played out.

## Residents added (`people.csv`)

| # | id | Name | Role | Faction | home_place / work_place | Schedule rows it brings to life |
|---|---|---|---|---|---|---|
| 1 | `ur_ningirsu_herdsman` | Ur-Ningirsu (names.csv, existing) | herdsman | southern_city_states_alliance | — / `grazing_lands_place` | `herds_out_to_pasture`, `herds_home_before_dark` |
| 2 | `ibni_shamash_priest_sun` | Ibni-Shamash (new name) | priest of the sun | — (clergy) | — / `temple_front_place` | `sun_temple_morning_rite` |
| 3 | `nur_shamash_paladin` | Nur-Shamash (new name) | paladin | retributors_of_utu | — / `temple_front_place` | `paladin_dawn_drill` |
| 4 | `taribum_fisherman` | Taribum (new name) | fisherman | southern_city_states_alliance | — / `lighthouse_wharf_place` | `fishermen_morning_tide` |
| 5 | `geme_enlil_field_hand` | Geme-Enlil (names.csv, existing) | field hand | southern_city_states_alliance | — / `fields_beyond_the_gate_place` | `field_hands_first_light` |
| 6 | `lu_enlilla_lighthouse_keeper` | Lu-Enlilla (new name) | lighthouse keeper | — (civic office) | — / `great_lighthouse_place` | `lighthouse_harbor_watch`, `lighthouse_fire_through_night` |
| 7 | `ur_dumuzida_dockworker` | Ur-Dumuzida (new name) | dockworker | southern_city_states_alliance | — / `lighthouse_wharf_place` | `dockworkers_outbound_cargo` |

- **City.** All seven live in `city_of_the_moon`, the slice city (D-005).
- **Faction.** The same rule as invented-ledger-street.md. Ordinary working residents get `southern_city_states_alliance`. The civic office-holder (the lighthouse keeper, like the gatekeepers and watchmen) and the clergy (the sun priest, like the moon priests) are left blank. The paladin is a Retributor of Utu (factions.csv `retributors_of_utu`, CANON "guild of paladins", notes L50). The guild is headquartered off-map in the City of the Sun, and one of its men is posted in the City of the Moon. dialogues.csv `breaking_retributor_of_utu` already puts a Retributor at the moon gate.
- **home_place blank for all seven.** None of them has a house on the slice street, the same convention as the gatekeepers, traders and brewer (invented-ledger-festivals #10).
- **Neighbour links.** people.csv has no neighbour or household column. The kernel wires `knows` edges itself (same role + same city, and a sorted-id neighbour chain per city). The new residents join that chain with no data change. No new person shares a role with an old one, so no same-role cliques changed.

## Names added (`names.csv`, tag `A`)

Following the table's convention (real attested Sumerian/Akkadian personal names, non-royal where possible):

| id | Name | Culture | Source |
|---|---|---|---|
| `ur_dumuzida` | Ur-Dumuzida | sumerian | attested Ur III onomasticon |
| `lu_enlilla` | Lu-Enlilla | sumerian | attested Ur III onomasticon |
| `nur_shamash` | Nur-Shamash | akkadian | attested Old Babylonian onomasticon ("light of Shamash", fitting a paladin of the sun) |
| `ibni_shamash` | Ibni-Shamash | akkadian | attested Old Babylonian onomasticon (Sippar archive records; Sippar is a real cult city of Utu/Shamash) |
| `taribum` | Taribum | akkadian | attested Old Babylonian onomasticon |

Ur-Ningirsu and Geme-Enlil were already in names.csv and unused. The designer's specific-citation accuracy pass (D-012) covers these like every other `A` name.

## Places added (`places.csv`)

The street data supports these. buildings.csv carries the Great Lighthouse as CANON in the City of the Moon (wb §5, notes L52). quests.csv and dialogues.csv repeatedly stage scenes on "the lighthouse wharf" and "the dockside below the Great Lighthouse". The schedule rows themselves put the fields and grazing lands "beyond the walls", through the Moon Gate.

| id | District | Kind | building_id | Grounding |
|---|---|---|---|---|
| `great_lighthouse_place` | Lighthouse Wharf | lighthouse | `great_lighthouse` (CANON) | schedules.csv lighthouse rows; wb §5; notes L52 |
| `lighthouse_wharf_place` | Lighthouse Wharf | wharf | — | fishermen/dockworker rows; quests.csv lighthouse-wharf quests; dialogues.csv arrival_captain_verdict |
| `fields_beyond_the_gate_place` | Beyond the Moon Gate | field | — | field_hands_first_light; dialogues.csv breaking_retributor_of_utu (the river road below the moon gate); notes L57 (sowing wheat and barley) |
| `grazing_lands_place` | Beyond the Moon Gate | pasture | — | herds_out_to_pasture / herds_home_before_dark |

- **District names.** "Lighthouse Wharf" is the canon texts' own phrase. "Beyond the Moon Gate" is an invented label for the land outside the slice street's gate, like "Moon Gate Quarter" (invented-ledger-street.md).
- **owner_person_id blank** on all four. They are public or civic places, like the gate, the well and the market square.

## Nearest-existing-place choices

- **The paladin drills at `temple_front_place`.** The schedule row says "drill in the yard … facing the rising sun". No guild yard exists in the City of the Moon, and nothing in the street data supports one (the Retributors' house is in the off-map City of the Sun). The nearest sensible existing place is the temple of sun and moon (CANON), the sun god's own temple in the city.
- **The priest of the sun works at `temple_front_place`.** The temple of sun and moon is the only sun temple in the city. This matches `sun_temple_morning_rite`'s existing `place`.
- **The herdsman's rows stay at `moon_gate_place`.** Both herdsman rows already name the gate (the flocks are counted through it), so they are unchanged. `grazing_lands_place` is his `work_place`, which festival and per-person `work` tokens resolve to.

## Schedule rows changed (`schedules.csv`, `place` column only)

K-2 left `place` blank on six rows because their roles had no slice-street place (invented-ledger-festivals #9). Those roles now have residents with a `work_place`, so the six rows get `place = work`: `paladin_dawn_drill`, `fishermen_morning_tide`, `field_hands_first_light`, `lighthouse_harbor_watch`, `dockworkers_outbound_cargo` and `lighthouse_fire_through_night`. Each source_ref gains a D-020 note. The task text, hour, season and tag are unchanged.

## Regression guard

`kernel/tests/test_population.cpp: test_every_schedule_role_has_a_resident_in_real_canon` loads the real canon, seeds people, and asserts two things. Every role returned by `schedule_roles()` is held by at least one seeded resident. Every role-holder's `home_place`/`work_place` is empty or a real places.csv id. A future schedule role added without a resident fails the suite.
