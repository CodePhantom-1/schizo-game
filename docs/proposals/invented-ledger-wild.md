# INVENTED LEDGER — W4-C, the wild lands, bandits, raiders and camps

Per D-018 and D-021, every choice below is authored glue tagged `INVENTED`. It fits the theme and the canon, contradicts no CANON or A row and nothing in the notes, and the designer may veto it. A veto is appended to DECISIONS.md and supersedes only that line. **No CANON row was changed. places.csv was not changed at all.** Its gate, wharf, fields and grazing-lands ids are reused as travel nodes. Machinery is documented in [kernel/contracts/module_Wild.md](../../kernel/contracts/module_Wild.md).

## Map (D-002: one city and its ~20 km² region)

| # | Choice | Where |
|---|---|---|
| 1 | 15 wild places around the City of the Moon, laid out in metres from the Moon Gate within a ~4.8 × 3.9 km box (~19 km²): the River Road, the Reed Banks, the Canal Head, the Date Grove, the Northern Ford, the Tell of the Old Town, the Southern Marsh, the Reed Islands, the Old Graves, the Desert Edge, the Rocky Caverns, the Sutean Wells, the Eastern Road, the Eastern Waystation, the Broken Shrine | `wild_places.csv` |
| 2 | The compass. The river and the fields lie north and east, the marsh south-east (the sage of the swamps), the desert and the Suti south-west (regions.csv south), and the eastern road runs toward the raiders' mountains | `wild_places.csv` direction / x_m / y_m |
| 3 | The Old Graves stand for the real Royal Cemetery of Ur beside the city (`A`: Woolley's excavations), since City of the Moon = Ur (D-018) | `wild_places.csv:old_tombs_place` |
| 4 | Danger 0–10 per place and per link | `wild_places.csv`, `wild_links.csv` |
| 5 | 23 links (road, track, river, marsh path, desert track), measured on the layout. 3 journeys leave the map: Sutean Wells → City of the Abyss (Eridu, 22 km), Canal Head → City of the Sun (Larsa, 45 km), Eastern Waystation → City of the Warrior Spirit (240 km) | `wild_links.csv` |
| 6 | Lootable contents: pottery, a clay liver model and copper in the tell; gold, lapis, pottery and an underworld scroll in the old graves (the real grave goods of Ur are gold and lapis, `A`); a bronze sickle, casting lots and copper in the caverns; amulets, zisurru flours and pottery in the shrine. Risks: collapse, snakes, jackals, and the restless dead at the graves. Ruins and caves refill after 90–120 days; tombs never do | `wild_places.csv` loot / risks / restock_days |

## Bands (mechanics.md row 11: "eastern tribes, the southern alliance, … drought raiders")

| # | Choice | Where |
|---|---|---|
| 7 | **The Jackals of the Caverns**: outlaw bandits (runaway debt-slaves, deserters, tomb robbers), present from day 1, camping in the Rocky Caverns | `wild_groups.csv:cavern_jackals` |
| 8 | **The Reed Men**: marsh bandits who form after 30 days of drought, lift sheep and vanish by reed boat | `wild_groups.csv:reed_men` |
| 9 | **The Warband of the Eastern Road**: the canon eastern raiders (`the_barbarians`), who form after 45 days at drought ≥ 2 and camp in the Broken Shrine. Their grudges: the Empire, and the rebellion's zealots | `wild_groups.csv:eastern_warband` |
| 10 | **The Sutean Riders**: the canon Suti, who form at drought ≥ 3 at their wells. Their grudge is against the Jackals, their rivals for the desert | `wild_groups.csv:suti_riders` |
| 11 | **The Sons of the Swamp Sage**: the canon southern rebellion's partisans, who form at war ≥ 2. They fall only on the Empire's caravans and on eastern raiders, never on southern fields (wb §4.2: "push back the invading barbarians") | `wild_groups.csv:sons_of_the_swamp_sage` |
| 12 | Leader names for all five bands (three each, one per re-formation): Nidnusha the Jackal, Ur-Gigir the Runaway, Kharzu Bull-Horn, Yadih-El of the Wells, Lu-Utu the Zealot and the rest | `wild_groups.csv` leaders |
| 13 | Strengths, supplies, morale, prowess, raid preferences, territories, camp sites and grudges | `wild_groups.csv` |

## Roads, beasts, weather

| # | Choice | Where |
|---|---|---|
| 14 | Three caravans. Tin, copper and lapis from the east every 15 days (`A`: the eastern tin and lapis trade). Salt and dates from Eridu every 10 days, under imperial tally, so the zealots want it. Grain and wool from Larsa every 15 days, under the Retributors' sun-disc | `caravans.csv` |
| 15 | Nine encounters: bandit ambush, raider patrol, refugees from the east (echoing events.csv), travelling merchants, a lion in the reeds (`A`: the Mesopotamian lion), a wolf pack, a wild boar, a viper, a jackal pack (tied to wb §5's dog engravings) | `wild_encounters.csv` |
| 16 | Six transport modes and their speeds: foot 4 km/h, donkey 4, ox-cart 3, horse 8, chariot 10, reed boat 5. Only the boat may take the river links. The ox_cart, riding_horse and war_chariot items are not in items.csv yet; the engine grants them until an items pass authors them | `transport_modes.csv` |
| 17 | Six weathers with season weights and drought pulls: clear, hot, scorching, sandstorm (no raids, the desert tracks slow to a crawl), rain (mud; the drought pushes it out), river fog (the raiders' friend) | `weather.csv` |

## Tunables (machinery constants, `kernel/include/sim/Wild.hpp` and the contract)

| # | Choice |
|---|---|
| 18 | The raid formula's weights: hunger 12·drought + 6·war + (100 − supplies)/2; opportunity per target (fields 25 in harvest, 10 in sowing, 5 otherwise; herds 10 + herd/4; market 15; caravans 30, +20 on a grudge); 6 per missing watchman; + band strength; defence 5 per watchman, standing/4, walls +20, guard +4, escort +5, ward +20; fear = player fear/2 + (100 − morale)/5; chance = 20 bp a point, capped at 20% a day |
| 19 | Raid yields: fields 2 grain a fighter; herds 1 + strength/4 head and wool; the market 3 grain a fighter plus the richest other good. A 30% chance of a casualty, slain or taken 50/50, and at most 5 captives a camp |
| 20 | Camp life: upkeep (drought + war + 2)/3 supplies a day; starvation −3 morale a day and a deserter every 5 days; recruits 1 + drought/2 every 15 days in drought or war; lookout 30 + morale/3, +30 on alert; moving when hunted, bled or afraid; a 3%-a-day feud chance |
| 21 | Captives are sold east after 45 days. Each capture starts the emergent quest `rescue_<npc>` (30 silver and +5 standing with the city on success) |
| 22 | Prices: a bounty of 6 silver a fighter, tribute of 5 a fighter for 30 days, ransom 40, escort fee 12, road toll 3 a foe |
| 23 | Travel: the encounter chance an hour, the flee chance, extra thirst from heat, desert and drought, auto-drinking at thirst 60 and auto-eating at hunger 60, collapse at thirst 100 |
| 24 | Herds start at 60 head (cap 80) and regrow 1 every 5 days unless the drought reaches stage 4 |

**Not decided here:** how many named people the region holds (the herdsman and field hand stay the street's), the city watch's size, and what becomes of the slain beyond their fate flag. The kernel never deletes a Population row.
