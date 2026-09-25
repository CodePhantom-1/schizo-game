# module_Wild — the wild lands, bandits, raiders and camps (W4-C)

**Imports:** docs/mechanics.md row 11 (minor factions and the raid formula, living-world §6), row 31 (transport), row 35 (no fast travel), row 36 (one map, off-map ventures and journeys). **Decisions:** D-002 (one city and its ~20 km² region), D-018 (invented content, ledgered), D-021 (open play: nothing gates on story or quest progress), D-022 (integer maths only).

**Files:** `include/sim/Wild.hpp` (state types, pure queries, the combat seam), `include/sim/WildActions.hpp` (the verbs over `WorldState`), `include/sim/CApiWild.h` (C ABI, included by `CApi.h`); `src/Wild.cpp` (canon loading, queries, the stub resolver), `src/WildTick.cpp` (weather, caravans, the raid formula, camp life), `src/WildTravel.cpp` (routes, travel, encounters), `src/WildPlayer.cpp` (player verbs), `src/WildWorld.cpp` (cross-module helpers), `src/WildSave.cpp` (snapshot rows), `src/CApiWild.cpp`; private `src/WildInternal.hpp`.

**Canon (all INVENTED, ledgered in docs/proposals/invented-ledger-wild.md):** `wild_places`, `wild_links`, `wild_groups`, `wild_encounters`, `caravans`, `transport_modes`, `weather`. places.csv is **not** changed: its gate, wharf, fields and grazing-lands ids are reused as travel nodes, so every current reader of places.csv keeps working.

## State

`WorldState::wild` (`WildState`): static tables (loaded by `init_wild` from canon, never saved) and dynamic state (saved): groups, camps (lookout, alarm, loot pile, captives, known/scouted), caravan runs, the raid and encounter logs (most recent 64 each), npc fates (`slain` / `captive:<group>` / `sold_east`; Population rows are never deleted), lootable sites, the player's place / band / escort, herds, the player's fear / wounds / infamy.

## The daily tick (`tick_wild`, after `hold_due_hearings`)

1. **Weather** (`weather_on`): a pure function of (world rng stream, day, season, drought) drawn from weather.csv weights.
2. **Caravans:** runs depart on schedule, move two stops a day (not in a sandstorm), and on reaching the Moon Gate deliver their goods to the city market (`Economy::deliver`). An escorted run pays the player `kEscortFee` and +2 standing with the city's faction.
3. **Herds** regrow +1 every 5 days unless drought ≥ 4; **sites** with `restock_days` refill.
4. **Every band:** an inactive band counts the days its `forms_when` has held and forms (first time: after `form_days`) or re-forms (after a clearing: `form_days` again and `repop_days` since the clearing) at its first free camp site, under the next leader. An active band: spreads the rumour of its camp once; eats `(drought + war + 2) / 3` supplies a day; at zero supplies loses morale (−3/day) and deserters (−1 every 5 days); recruits `1 + drought/2` every 15 days in drought or war; posts a lookout (`30 + morale/3`, +30 on alert); scatters at strength or morale 0; **moves camp** when hunted (scouted and grudge ≥ 40), bled (below a third of its peak and under base strength) or afraid of the player (fear ≥ 60); **feuds** (3% a day) with a band it holds a grudge against on shared ground; then rolls its **raid**.
5. Captives held `kCaptiveDays` (45) are **sold east**; the player's fear fades by 1 every 2 days; grudges by 1 every 10 days.

## The raid formula (`assess_raid`)

For each target the band prefers (`fields`, `herds`, `caravans`, `market`) and can reach (a target node in its territory; a caravan on its territory today):

- **hunger** = 12·drought + 6·war + (100 − supplies)/2
- **opportunity** = 6 per missing watchman/gatekeeper/paladin (slain or captive) + band strength + weather + preference/10 + target: fields 25 in harvest / 10 in sowing / 5 otherwise; herds 10 + herd/4 (max 20); market 15; caravans 30 (+20 against a grudge faction's caravan)
- **defence** = 5 per watchman/gatekeeper/paladin in play + standing with the city's faction / 4 (the player's reputation) + 20 behind the walls (market) + 4 per caravan guard + 5 per escort fighter + 20 on a warded place (K-1 `ward_holds`)
- **fear** = player fear / 2 + (100 − morale) / 5
- **score** = hunger + opportunity − defence − fear; **chance** = clamp(score × 20 bp, 0, 2000 bp) a day, for the best-scoring target.

No raid when the band is inactive, led by the player (he decides), under 2 strong, or in a sandstorm. A bribed band (`paid_until`) raids nothing but caravans, and never one the player escorts. Partisans raid only the caravans of the factions they hate.

**Resolution** (`resolve_raid`): caravans are fought through the combat seam (band against guards and escort). Other targets succeed on `clamp(4000 + 400·strength − 50·defence, 1000, 9000)` bp. A success takes goods off the market through **`Economy::consume`** (the theft/raid hook): fields take 2·strength grain; herds take `1 + strength/4` head and strength wool; the market takes 3·strength grain and strength of its richest other good. The loot goes to the camp's pile; the band gains supplies, morale and a recruit. At a 30% chance, a person whose role fits the target (field hand, herdsman, market trader, watchman; never a CANON person) is **slain or carried off** (50/50, at most 5 captives a camp). A failure costs 1–2 fighters and 15 morale. Every raid is logged as an event (`wild_raid_<target>`) and a raid record. Raids that shed blood also travel as rumours.

**Captives:** each capture starts an emergent quest `rescue_<npc>` (deadline 45 days; reward 30 silver and +5 standing with the city's faction). Freeing the captive by clearing the camp, infiltrating it or paying the ransom completes the quest. A captive who is killed or sold east fails it.

## Travel (`travel`, `find_route`)

The cheapest route by minutes for the mode (Dijkstra over the links the mode may use). The leg time is `distance_m × 60 / speed_m_per_h × weather pct / 100` in integers. **No fast travel:** the result is the game-minutes spent, and the engine plays them out. Each hour: `Needs::advance_needs`, plus extra thirst from heat, desert tracks and drought/2. At thirst ≥ 60 the player drinks water from his inventory, or free from a river link; at hunger ≥ 60 he eats the first edible item he carries. At thirst 100 he collapses (travel halts, +1 wound). Each started hour rolls an encounter at `300 + 150·danger + 300 at night + 50·drought + 50·war + weather` bp (cap 6000). The encounter is drawn by weight from wild_encounters.csv, filtered by terrain, hours, season and danger; bandit and raider draws attach the strongest active band whose territory touches the leg. Stances: `fight`, `pay` (toll 3 silver a foe), `flee` (faster mounts flee better), `rob` / `give` / `pass` (for merchants and refugees). A defeat robs the player (half his silver, a third of each stack), wounds him and halts the journey. Friendly bands (joined, led, allied, bribed; partisans towards non-imperial men) let him pass. Journeys to off-map cities leave the player standing at the city id.

## Player verbs (D-021 open play)

Each verb returns `{code, text}`. Codes: ≥ 0 done; −1 bad argument; −2 unknown camp, group, run or site; −3 the player is not there; −4 not possible now; −5 cannot pay; −6 refused.

- `scout_camp`: from the camp or next to it. Reports strength, supplies, morale, lookout, loot and captives. A lookout may spot him (alarm, grudge).
- `attack_camp`: allied bands that hate the target lend half their strength, and the city lends 2 watchmen at standing ≥ 50. An unscouted or alarmed camp fights better. A win **clears** the camp: bounty (6 silver a fighter), +5 standing with the city's faction, −10 with the band's own faction, the loot pile, captives freed, fear +40.
- `infiltrate_camp` (free_captives | steal_loot | sabotage | learn): a stealth roll against the lookout (better at night, worse on alert). Insiders always succeed. Being caught means a fight.
- `pay_off_group`: 5 silver a fighter buys 30 days without raids on the city. `ally_with_group`: needs a reason (bribed, faction standing ≥ 25, or the player has already cleared one of the band's enemies); allies reveal their enemies' camps. `join_group` / `leave_group`. `challenge_leader`: a duel in the band's camp; a win means the player leads the band. `found_band`: his own band (`player_band`) at a free camp site. `lead_raid`: his band raids now; he gets the leader's half or a follower's fifth of the loot, infamy, −5 standing with the city's faction, and a **crime** (`theft`, or `murder` if blood was shed) filed through `Actions::commit_crime`.
- `ransom_captive` (40 silver); `escort_caravan` / `abandon_escort` (the player moves with the run; travel is refused while escorting); `rob_caravan` (goods to the inventory, a theft crime, −5 standing with the city's and the caravan's factions).
- `search_site`: ruins, caves and tombs, 3 units of loot a search. Risks: collapse (a wound), snakes or jackals (a fight), the restless dead (−5 favour with underworld_queen, D-004 Mythic mode). **Robbing a tomb files `tomb_robbery`** (laws.csv, canon), witnessed or not.
- `hear_rumours(npc)`: the wild rumours an NPC has heard. Hearing a camp's rumour marks that camp known to the player.

## Cross-module writes

These happen only in the verbs and the tick, never elsewhere, and only through public APIs: `Economy::consume` and `deliver`; `Population::witness` (rumours); `EventsState::fired` together with `fire_count_by_rule`, so the two sums stay equal; `Quests::accept`, `advance_stage`, `complete` and `fail` and `Actions::complete_quest`; `Actions::commit_crime`; `Faction::add_standing` (standing is read and moved; **Faction.cpp is untouched**, W4-D owns it); `Property::credit_purse` and `take_from_purse`; `Magic::add_favour`; `Needs::advance_needs`, `eat` and `drink`; and the player's inventory.

## Combat seam (W4-B)

`WildState::skirmish` is a `SkirmishResolver` (`SkirmishOutcome (*)(const SkirmishRequest&, Rng&)`). While it is `nullptr`, `stub_skirmish` resolves every fight: power = fighters × prowess × (50 + morale), each side scaled by 75..125%, with ties going to the defender. **At merge:** W4-B writes an adapter from `SkirmishRequest` to `resolve_skirmish` and sets `wild.skirmish = &adapter` in `WorldState::init`. Nothing else changes. The requests carry `context` (encounter | camp_assault | raid | duel | feud | infiltration), place and hour. `player_side` builds the player's side from `1 + companions` at prowess `100 − 10·wounds`. W4-A's progression can replace that prowess at merge, and `player_wounds` can hand over to W4-B's health model.

## Snapshot

One trailing, **optional**, order-independent section: `WILD\t<n>` followed by n rows (S scalars · G group · C camp · V caravan run · R raid · E encounter · F fate · T site). `Reader::peek_tag` (a W4-C block in Snapshot.cpp) lets the section sit in any order among other agents' trailing sections. A save written before W4-C has no WILD section and loads into the fresh `init_wild` state. A malformed row throws, and `load_world` stays atomic.

## Determinism (D-022)

Integer maths throughout: chances are basis points, distances metres, durations minutes. Every roll comes from `w.rng.fork(day ⊕ fnv1a(what))`, and each player verb adds `action_serial`. The world rng itself is never advanced, so no other module's random stream moves. `test_wild.cpp` checks that no `float`/`double` appears in the wild sources.

## Tunables (INVENTED, D-018; `Wild.hpp`)

`kRaidBpPerPoint` 20 and `kRaidMaxBp` 2000; `kHerdStart` 60 and `kHerdMax` 80; `kCaptiveDays` 45; `kBountyPerFighter` 6; `kTributePerFighter` 5 and `kTributeDays` 30; `kRansomSilver` 40; `kEscortFee` 12; `kCaravanLegsPerDay` 2; `kRescueRewardSilver` 30. The formula weights above are listed in the ledger.

## Merge notes

- `CApiWild.cpp` repeats `struct SimWorld { WorldState world; };` token for token (a legal multi-TU class definition). If CApi.cpp's `SimWorld` ever changes, move both into a shared `src/CApiInternal.hpp`.
- The labelled W4-C blocks in shared files are: `World.hpp` (member and include), `World.cpp` (init and tick), `Db.cpp` (table list), `Snapshot.cpp` (peek_tag, save, load), `CApi.h` (include), `tools/export_datatables.py` (table list). `tests/test_scenario_crime.cpp` changed one assertion: it now checks for no memories *of the criminal*, because the city now talks about bandit camps.

**Tests:** `tests/test_wild.cpp` (data, formula, travel, encounters, camps, captives, verbs, caravans, sites, weather, seam, save, determinism, no-float), `tests/test_scenario_wild.cpp` (the year of drought), `tests/test_wild_capi.cpp` (the C boundary).
