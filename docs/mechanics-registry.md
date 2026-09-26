# MECHANICS REGISTRY — every mechanic in the game, one row each

**Status:** v1 (2026-09-26). The single list of everything the game must do. The plan that builds it is [mechanics-implementation-plan.md](mechanics-implementation-plan.md).
**Checked by:** `python3 tools/mechanics_registry.py --check`. It fails when an id is duplicated, when a status or phase is invalid, when a C API call is not cited by any row (so no kernel function can go unplanned), or when a rule-bearing canon row has no instance row. It writes the per-row instances to [mechanics-instances.md](mechanics-instances.md), and shows which mechanics already have a test tagged `MECH:<id>`.

**Where the rows come from (the `Source` column):**
- `N:L<n>`: the designer's notes, [db/sources/notes.md](../db/sources/notes.md)
- `RS §`: rpg-systems · `LW §`: living-world · `CL §`: city-life · `GDp §`: the parent game-design · `WM §`: world-map · `AR §`: architecture (all in `../../docs/`, the imported systems)
- `WB §`: world-bible · `GD §`: game-design · `EN §`: endings · `CM`: city-of-the-moon · `CP`: completion-plan
- `D-0xx`: DECISIONS.md · `LG-<name>`: `docs/proposals/invented-ledger-<name>.md` · a table name: `db/canon/<table>.csv`

**Status columns:** `K` kernel · `C` C API · `U` Unreal (the verb, the actor, the screen). `Y` done · `P` partial · `N` missing · `-` does not apply.
**Phase:** `P0`–`P5` are the first phases of the implementation plan; a letter is the [completion-plan](completion-plan.md) stage that builds it (`B`…`U`). `MQ` = parked with the main quest (only its system is built now).
**Calls:** the C API calls the mechanic uses, without the `sim_world_` prefix.

A mechanic is **done** only when it passes the 18-point checklist in the plan (§4) and a test tagged `MECH:<id>` exists at each layer it touches.

---

## 1. Foundations and the simulation core (FND, SIM)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| FND-01 | Item stacks v2 | Stacks carry qty, quality tier, condition, owner, stolen flag, made day, bound; merge only on equal fields; the count view stays | LW §3.1, §4 | Y | Y | - | P0 | item_count give_item inventory stack_count stack_at |
| FND-02 | Item catalogue columns | Every item has ui_category, weight_g, stack_max, spoil_days, icon, mesh, flags; lint enforces them | LW §3.3 | Y | Y | - | P0 | item_def |
| FND-03 | Carry capacity and encumbrance | 30 kg + 3 kg per Strength; burdened >100%, pinned >125%; silver has weight | RS §1.1, §9 | Y | Y | N | P0 | capacity_g carried_g encumbrance |
| FND-04 | World items and containers as kernel state | Dropped goods, containers and their contents live in the kernel and are saved; Unreal spawns from them | LW §4 | Y | Y | N | P0 | drop pick_up world_item_count world_item_at add_container container_stack_count container_stack_at put_in take_out |
| FND-05 | The verb-result protocol | Every verb returns a code and a string-table reason key; nothing fails silently | CP §1 r5 | Y | Y | N | P0 | progression_refusal last_reason |
| FND-06 | Timed actions | Activities cost game minutes, advance needs and the clock together, and can be interrupted with partial results | CL §1 | Y | Y | P | P0 | advance_needs advance_minutes |
| FND-07 | The verb wheel | Interactables list all their verbs; tap does the first, hold opens the wheel; greyed verbs give reasons | CL §8 | - | - | P | P0 | |
| FND-08 | The UI kit | Item list, tooltip, quantity picker, confirm, two-pane transfer, toasts, controller navigation, string tables | GD §8 | - | - | P | P0 | |
| FND-09 | Player notice feed | Daily-tick outcomes that concern the player reach the screen and the log | LW §8 | Y | Y | N | P0 | event_count event_at notice_count notice_at |
| FND-10 | Mechanic test protocol | Kernel, C API, UE automation, console, save round trip and smoke per mechanic; the registry checks coverage | CP §1 | - | - | - | P0 | |
| SIM-01 | World creation | A new world loads the canon, seeds markets, people, the calendar and the wild | AR §4.2 | Y | Y | Y | A | create destroy |
| SIM-02 | Deterministic daily tick | Fixed tick order; the same seed and inputs give the same world | AR §4.2 | Y | Y | Y | A | advance_days day |
| SIM-03 | Simulation layers L0–L2 | Full AI within ~150 m, scheduled beyond, statistical off-map; promotion of light residents | LW §1 | P | - | P | F | npc_count |
| SIM-04 | Drought stages | The drought rises and falls; prices, raids, harvests, herds and weather follow it | N:L15, L104 | Y | Y | P | O | set_drought drought |
| SIM-05 | War stage | The war clock drives the raid politics, levies and events | N:L15 | Y | Y | P | Q | set_war war |
| SIM-06 | The historical clock | The acts' horizon runs regardless of the player; magic and play change only local outcomes | WB §4.1; D-021 | P | - | - | MQ | |
| SIM-07 | The world goes on without you | Everything ticks while the player is away, asleep, travelling or in a time skip | CL §8 | Y | - | P | P | |
| SIM-08 | Day length setting | The sim day length is adjustable (default 48 real minutes) and loads keep the hour | D-024 | - | - | Y | A | |

## 2. Time, calendar, moon and weather (TIM)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| TIM-01 | The clock and day/night | The sun follows the sim hour; night is dark | CL §1, §8 | - | - | Y | A | |
| TIM-02 | Date, months and years | 12 months × 30 days; the HUD shows the date and month name | calendar; months | Y | Y | Y | A | date month_name |
| TIM-03 | Seasons | Rains, sowing, harvest, vintage bend schedules, prices, weather and events | seasons; D-015 | Y | Y | P | O | season |
| TIM-04 | Moon phase and light | Integer phase and illumination; the moon is placed and lit by phase; moonlight brightens nights | calendar_days; D-024 | Y | Y | Y | A | moon_phase moon_illumination |
| TIM-05 | Named days | The eššešu days, šapattu, the unfavourable days and bubbulum are shown and read by systems | calendar_days | Y | Y | Y | A | day_observance |
| TIM-06 | Lucky and unlucky days | The day's omen changes rite outcomes, trade and event odds by a measured amount | RS §10.1; LG-calendar | P | Y | N | O | day_omen |
| TIM-07 | Weather | Six weathers by season and drought; visible; changes heat, travel time, raids and events | weather; LG-wild | Y | Y | N | O | weather |
| TIM-08 | The city's hourly clock | Before dawn, dawn, morning, midday, afternoon, evening, night each play as city-life §1 describes | CL §1 | P | P | P | O | task_at npc_task_at |
| TIM-09 | Seasonal bend of the day | Summer midday rest and rooftop sleeping; winter early dark and indoor fires | CL §1 | P | - | N | O | |
| TIM-10 | Wait and skip hours | The player can pass time; sleeping skips the clock without double-charging needs | CL §8 | - | - | Y | P1 | |
| TIM-11 | Time skips between acts | The world runs years forward in coarse steps and writes an interlude report | LW §10 | N | N | N | P | |
| TIM-12 | Interlude decisions | Marriages, investments, schooling and moves decided during a skip | LW §10 | N | N | N | P | |
| TIM-13 | Aging | Every character ages; old age can kill across skips | LW §10; RS §12 | N | N | N | P | |

## 3. The body (BOD)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| BOD-01 | Hunger | Rises by the hour; eating restores it; severity setting scales it | RS §1.2, §6 | Y | Y | Y | B | hunger |
| BOD-02 | Thirst | Rises faster in heat, desert, armour and exertion | RS §6 | Y | Y | Y | B | thirst |
| BOD-03 | Fatigue | Rises awake, falls asleep; exhaustion refuses work | RS §1.2 | Y | Y | Y | B | fatigue |
| BOD-04 | Need effects ladder | Each need has named stages with effects, shown on the HUD | RS §1.2 | Y | Y | Y | B | need_effects set_need |
| BOD-05 | Needs severity setting | A percentage applied to every climb | RS §12; A11 | Y | Y | Y | A | set_needs_severity needs_severity |
| BOD-06 | Heat and cold | From season, weather, hour, shade, interiors, armour layers and activity | RS §1.2, §8 | N | N | N | B | |
| BOD-07 | Heatstroke and chill | Conditions with effects and a path to death | RS §1.2 | N | N | N | B | |
| BOD-08 | Illness | Fever, dysentery, eye disease, plague: contagion, incubation, symptoms, remedies; toggle respected | RS §1.2; LW §11 | N | N | N | B | |
| BOD-09 | Contagion sources | Crowding, famine, ships, bad water, spoiled food, corpses | CL §7 | N | N | N | B | |
| BOD-10 | Purity | Clean → impure → defiled, 0–100 | RS §1.2 | Y | Y | N | B | purity set_purity |
| BOD-11 | What lowers purity | Touching a corpse, blood, certain foods, sex, oath-breaking | RS §1.2; CL §4 | N | N | N | B | |
| BOD-12 | What restores purity | Washing at water, rites, time | RS §1.2 | N | N | N | B | |
| BOD-13 | Intoxication | Beer and wine stages; social bonuses then aim sway and slurred options | RS §1.2, §6 | N | N | N | B | |
| BOD-14 | Diet balance | Five food groups; Well-fed bonus; Weakness from a narrow diet | RS §6 | N | N | N | B | |
| BOD-15 | Health | Hit points from wounds; healing over days | RS §1.2 | Y | Y | P | K | health |
| BOD-16 | Stamina | Spent by sprinting, fighting, armour; recovered by rest and catching breath | RS §8 | Y | Y | P | K | stamina catch_breath |
| BOD-17 | Rest | Resting recovers stamina and fatigue | RS §1.2 | Y | Y | N | P1 | rest |
| BOD-18 | Sleep quality | Bed, guest bed, rough ground and rooftop change recovery; events can wake you | CP B8 | N | - | P | B | |
| BOD-19 | Encumbrance on the body | Overload slows, drains stamina, stops sprinting | RS §1.1 | Y | Y | N | P0 | encumbrance |
| BOD-20 | Death of the player | Starvation, thirst, wounds, illness, execution each lead to a defined outcome | CP B10 | P | Y | N | B | is_dead |
| BOD-21 | Conditions HUD | Every condition visible, with a text tooltip | CP B1 | - | - | P | B | |

## 4. Inventory, world items and loot (INV, WLD)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| INV-01 | Carry and merge | Stacks merge by FND-01; overflow past stack_max starts a new stack | LW §3 | Y | Y | N | P1 | give_item |
| INV-02 | Weight bar | Current/capacity on the inventory screen; burdened/pinned icon on the HUD | RS §1.1 | Y | Y | N | P1 | carried_g capacity_g |
| INV-03 | Inventory screen | Tab toggles; categories, sort, search, purse, weight | GD §8 | Y | Y | P | P1 | inventory stack_count stack_at |
| INV-04 | Inspect | Description, weight, local value, quality, condition, freshness, owner/stolen, uses | GD §8 | Y | Y | N | P1 | item_def |
| INV-05 | Item actions | Use, drop, split, give to follower, offer at a shrine, put in container; greyed with reasons | CL §8 | P | P | N | P1 | |
| INV-06 | Split and merge stacks | 0, 1, all and over-stack amounts all behave | — | Y | N | N | P1 |  |
| INV-07 | Stolen goods | Taken without leave = stolen with owner kept; honest sellers refuse; fences take at 40%; owner and guards recognise | LW §4; CL §3.3 | P | P | N | P1 |  |
| INV-08 | Bound and document items | Quest items can't be dropped or sold; documents open the reader | LW §3.3 | P | P | N | P1 |  |
| INV-09 | Spoilage | Fresh → stale → spoiled by spoil_days; spoiled food risks illness | RS §6 | N | N | N | P1 | |
| INV-10 | Item condition | Arms and tools wear; broken items can't be used until repaired | RS §7.3 | P | P | N | P1 | |
| INV-11 | The purse | Silver by weight, shown in shekels and grains, the same everywhere | GDp §6; D-017 | Y | Y | P | P1 | purse |
| INV-12 | Equipment screen | 4 zones × 2 layers, hands, ammunition, clothing; totals of protection, weight, stamina and heat | RS §8 | Y | Y | N | P1 | equip unequip equipped |
| INV-13 | Quick eat and drink | F/G pick the item that restores most without overshooting, soonest-spoiling first | — | P | Y | Y | P1 | best_food best_drink |
| INV-14 | NPC inventories | NPCs hold stacks the same way; shops sell from them | LW §3.1 | P | P | - | P2 | |
| INV-15 | Save and migrate inventories | Stacks, containers and world items round-trip; old saves migrate | AR §5 | Y | Y | P | P1 | save load save_to_buffer load_from_buffer |
| WLD-01 | Drop | Quantity picker; lands at your feet on the ground; stays yours | LW §4 | Y | Y | N | P1 | drop |
| WLD-02 | Drop into water | Floaters drift and can be recovered; the rest sink and are lost with a notice | CM §2 | N | N | N | P1 | |
| WLD-03 | Pick up | E takes it; partial pickup when too heavy; persists | CL §8 | Y | Y | P | P1 | pick_up |
| WLD-04 | Stealing a world item | Another's item shows "Steal"; seen → theft filed; unseen → flagged stolen | LW §4 | Y | Y | N | P1 | pick_up take_out |
| WLD-05 | Containers | Jars, chests, baskets, granaries, stalls, bodies: capacity, owner, lock; take, take all, put | LW §4 | Y | Y | N | P1 | add_container put_in take_out container_stack_count container_stack_at |
| WLD-06 | Locks | Key, permission, or forcing (noisy, burglary if heard) | CL §3.2 | N | N | N | P1 | |
| WLD-07 | Loot bodies | Transfer screen; legal on bandits and battlefields, theft otherwise | RS; CL §6 | Y | Y | N | P1 | loot |
| WLD-08 | Scavenging of dropped goods | Public goods left a day are taken by passers-by and start a rumour | LW §1 | N | N | N | P1 | |
| WLD-09 | NPCs pick things up | Beggars scavenge; guards take evidence | CL §3.3 | N | N | N | P1 | |
| WLD-10 | Placing items | Precise placement for decor and altar offerings | CL §8 | N | N | N | P1 | |
| WLD-11 | Physics settle | Items settle and never fall through floors; the resting position is stored | — | - | - | N | P1 | |
| WLD-12 | Seeded goods in buildings | Bakery racks, granary jars, workshop tools, placed by building type | CM §3 | N | N | P | P1 | |
| WLD-13 | Evidence traced to you | Known items, seal impressions and a talking fence let crimes be solved later | LW §4; CL §3.3 | N | N | N | H | |

## 5. Food, drink and eating (EAT)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| EAT-01 | Eat a chosen item | Inventory → Use eats that item | RS §6 | Y | Y | N | P1 | eat |
| EAT-02 | Drink a chosen item | Inventory → Use drinks it; water at wells | RS §6 | Y | Y | P | P1 | drink |
| EAT-03 | Eating takes time | Snack 2 min, meal 15 min, interruptible, unfinished part kept | — | N | N | N | P1 | |
| EAT-04 | Satiety cap | Refused when full, except at a feast (fatigue penalty) | — | N | N | N | P1 | |
| EAT-05 | Water sources | Well clean; river and lagoon carry illness risk; fill a waterskin anywhere | RS §6; LW §4 | P | P | P | P1 | |
| EAT-06 | Beer and wine | Water and calories plus intoxication and social bonuses | RS §6 | P | - | N | B | |
| EAT-07 | Meals as a service | Buy a meal at the beerhouse or cookshop; eat as a guest | CL §4 | N | N | N | P2 | |
| EAT-08 | Rations as wages | Barley, oil and wool paid as wages; well-fed workers are more loyal | RS §6 | Y | Y | N | P1 | work |
| EAT-09 | Feasting | Hosting a feast raises standing, favour and your network | RS §6 | N | N | N | O | |
| EAT-10 | The drinking fellowship | A sworn fellowship with a funerary and cult side; feasts in members' houses | RS §6; CL §5 | N | N | N | O | |
| EAT-11 | Offerings consume food | Food offered to a god is spent and counts for favour | RS §6, §10 | Y | Y | N | J | |
| EAT-12 | Famine | Prices soar, rations shrink, hunger dominates; a grain ship becomes a plot | RS §6 | P | - | N | O | |
| EAT-13 | First portion to the gods | Custom expects it; skipping it is noticed | customs | N | N | N | G | |
| EAT-14 | Taboo foods | Patron gods' taboos and priests' food rules; breaking them offends | RS §10.4; CL §4 | N | N | N | J | |
| EAT-15 | Preservation | Drying, salting, oil and fermenting preserve food | RS §6 | N | N | N | D | |

## 6. Economy, trade and services (ECO, TRD)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| ECO-01 | Prices from supply and demand | Grain-anchored prices move with stock, season and the drought | LW §3.1; D-011 | Y | Y | P | C | price |
| ECO-02 | Finite stock | The market holds units; buying the last one empties it | LW §3.1 | Y | P | N | C | |
| ECO-03 | Producers | Every stack traces to a workshop, field, herd or import | LW §3.1 | N | N | N | C | |
| ECO-04 | Restock by production | Shelves refill only from production; interrupting production empties them | LW §3.1; CL §1 | N | N | N | C | |
| ECO-05 | Imports by ship | Ships dock at the wharf on schedule and fill the market | LW §3.1; CM §3 | N | N | N | C | |
| ECO-06 | Imports by caravan | Caravans arrive at the gate on schedule | caravans | Y | Y | N | C | caravan_count caravan |
| ECO-07 | Raiding season empties shelves | Raids consume market goods | LW §3.1 | Y | - | N | C | |
| ECO-08 | Market days and festival closures | Market open or closed by day | festivals | Y | Y | N | C | market_open |
| ECO-09 | Silver by weight on scales | Seller's scales and stone weights; the scales UI | LW §3.2; GD §8 | N | N | N | C | |
| ECO-10 | Cheating weights | Dishonest sellers cheat; Reckoning catches it; fraud is a crime | LW §3.2 | N | N | N | C | |
| ECO-11 | Credit on a tablet | Buy on credit; a debt tablet item; the due date; collection | LW §3.2 | P | N | N | C | |
| ECO-12 | Barley measures | Regional measures and their exchange as trade knowledge | GDp §6 | N | N | N | C | |
| ECO-13 | Price modifiers | Standing, Bargaining, language, talents, rank with the city's polity | LW §3.1; LG-character | P | P | N | P2 | buy_quote sell_quote |
| TRD-01 | Shops with owners and hours | Owner present and awake, market open, festival rules | LW §3.2; CL §1 | N | N | N | P2 | |
| TRD-02 | Trade screen | Two panes, running balance, purses, confirm/cancel; nothing changes until confirm | GD §8 | N | - | N | P2 | |
| TRD-03 | Price explained | The quote returns each factor as a line | CP C4 | P | P | N | P2 | buy_quote sell_quote |
| TRD-04 | Buy | Atomic: purse, stock, weight warning, ownership, silver, market stock, XP, memory | LW §3 | P | Y | N | P2 | buy |
| TRD-05 | Sell | Seller's purse limit, buy list, stolen only to fences, bound never | LW §3 | P | Y | N | P2 | sell |
| TRD-06 | Barter | Goods for goods with silver to balance; fairness by personality | LW §3.2 | N | N | N | P2 | |
| TRD-07 | Haggling | One offer step; odds from Bargaining and opinion | — | N | N | N | P2 | |
| TRD-08 | Buyback | Buy back this visit's sales at the sale price | — | N | N | N | P2 | |
| TRD-09 | Services | Meals, beds, washing, physician, scribe, diviner, lessons, passage, hiring | LW §3.2 | P | P | N | P2 | |
| TRD-10 | Stealing from a shop | From a stall container when unwatched = theft | CL §2 | N | N | N | P2 | |
| TRD-11 | Rank-gated sellers | The palace sells from rank 3; licensed goods | LW §3.2 | N | N | N | P2 | |
| TRD-12 | Fences and smugglers | Buy stolen goods, run embargoed trade, no questions | LW §3.2, §6 | N | N | N | P2 | |
| TRD-13 | Mid-trade interruption | Owner dies, market closes, a raid starts: the screen closes, nothing changes | — | N | N | N | P2 | |
| TRD-14 | Every seller type in the city | Stalls, smith, potter, weaver/dyer, carpenter, bowyer, leatherworker, armourer, jeweller/seal-cutter, perfumer, physician, temple stores, diviners, scribes, animal traders, merchant houses, shipwrights, the palace, fences | LW §3.2; CP C3 | N | N | N | C | |
| TRD-15 | Commissions | Order an item with a price, due date, quality and pickup | CP D8 | N | N | N | D | |
| TRD-16 | Smuggling rule | Trading with the enemy inside the jurisdiction is a profitable crime | RS §4.3 | N | N | N | H | |

## 7. Crafting and production (CRF)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| CRF-01 | Recipes all-or-nothing | A recipe applies fully or leaves inputs untouched | module_Crafting | Y | Y | N | P3 | craft |
| CRF-02 | Station actors | Quern, oven, vat, fire and pot, kiln, forge, loom, dye vat, press, bench, leather frame, brick mould, potter's wheel; owned or rented | RS §6; CP D1 | P | P | N | P3 | |
| CRF-03 | Crafting screen | Known/unknown recipes, have/need, outputs, hours, skill, likely quality | GD §8 | N | N | N | P3 | |
| CRF-04 | Recipe knowledge | Known by skill threshold, a teacher or a text | RS §2.1 | N | N | N | P3 | |
| CRF-05 | Inputs from nearby containers | Own containers within 3 m count | — | N | N | N | P3 | |
| CRF-06 | Batch crafting | ×N up to the inputs | — | Y | Y | N | P3 | |
| CRF-07 | Time and interruption | Finished units kept, the unit in progress refunded | FND-06 | P | N | N | P3 | |
| CRF-08 | Quality | From skill, tool, inputs, station and a roll | RS §7.3 | N | N | N | P3 | |
| CRF-09 | Tools and fuel | Tools needed and worn; fuel burned | RS §5.3 | N | N | N | P3 | |
| CRF-10 | Output too heavy | Drops at your feet | — | N | N | N | P3 | |
| CRF-11 | Craft XP | 3 xp per recipe-hour to the craft's skill | LG-character | Y | Y | N | P3 | |
| CRF-12 | Chains | Grain → flour → bread and beer; dates → syrup and wine; clay → pot; flax → linen; wool → yarn → cloth → dyed; copper + tin → bronze → cast; silt + straw + water → mudbrick; fish → salted/dried; reeds → mats and boats | GDp §4.1; CP D2 | P | P | N | D | |
| CRF-13 | Smithing minigame | Mould, heat, timing; graded quality; porosity and weak tangs | RS §7.3 | N | N | N | D | |
| CRF-14 | Bronze bends | Bent blades straightened; broken ones melted and recast | RS §7.3 | Y | Y | N | D | repair recast |
| CRF-15 | Scrap economy | Smiths buy scrap; metal is never wasted | RS §7.3 | N | N | N | D | |
| CRF-16 | Cooking and brewing minigames | Outcome quality from skill and timing | CP D4 | N | N | N | D | |
| CRF-17 | NPC tasks in visible steps | The baker's and potter's days consume and produce real goods, step by step | CL §1 | N | N | N | D | |
| CRF-18 | Interruptible NPC production | Buying the last loaf, blocking the kiln or hiring the potter away changes the day's output | CL §1 | N | N | N | D | |
| CRF-20 | Water as an input | From a well or river beside the station, or a waterskin | — | N | N | N | P3 | |
| CRF-19 | Material realism in building | Mudbrick needs silt, straw and water; cedar only where traded | AR §4.4 | N | N | N | N | |

## 8. Activities and work (ACT)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| ACT-01 | The activity framework | activities.csv rows run as timed actions at activity spots | CL §8 | N | N | N | P3 | |
| ACT-02 | Sleep | Own bed, rented bed, guest bed, rough ground, rooftop; autosave | CL §1 | P | - | Y | P1 | |
| ACT-03 | Wait | Pick hours; refused near enemies | — | - | - | N | P1 | |
| ACT-04 | Work for wages | 19 roles; rations or silver; XP; output enters the market | work_roles | Y | Y | N | P1 | work |
| ACT-05 | Draw water | Fill a waterskin or drink | LW §4 | P | P | P | P1 | |
| ACT-06 | Wash | Purity restored at water and baths | RS §1.2 | N | P | N | P1 | |
| ACT-07 | Pray | Favour at a shrine or temple | RS §10 | P | Y | N | P3 | add_favour favour |
| ACT-08 | Offer | Consumes offerings for favour | RS §10 | Y | Y | N | P3 | |
| ACT-09 | Practise a skill | 2 xp/h alone, up to 25 | LG-character | Y | Y | N | P3 | practice_skill |
| ACT-10 | Train with a teacher | 5 xp/h up to the teacher's skill; fee to the teacher | RS §2.1 | Y | Y | N | P3 | train_skill skill_teachers skills_taught_by |
| ACT-11 | Study a text | 4 xp/h; needs scribal arts 10 | RS §2.1 | Y | Y | N | P3 | study_skill |
| ACT-12 | Fish | At the lagoon and river; yield by season; fishing skill | LW §4 | N | N | N | P3 | |
| ACT-13 | Forage | Herbs, dates, reeds, clay, silt; spots run down and recover | LW §4 | N | N | N | P3 | |
| ACT-14 | Hunt | Game in the wild; tracking | LW §4 | N | N | N | P3 | |
| ACT-15 | Herd | Tend a herd; wool, milk | LW §4 | P | P | N | P3 | herd_head |
| ACT-16 | Glean and harvest | Field work by the season | CL §5 | P | P | N | P3 | |
| ACT-17 | Dive for sea gems | Divers' huts; gems of Enki washed ashore or dived for | D-016; CM §2 | N | N | N | P3 | |
| ACT-18 | Sit, lean, emote | Bow by rank, mourn, pray; others react | CL §8 | N | N | N | P3 | |
| ACT-19 | Knock | At a door: an answer from a real resident; rude at night | CL §1; CM §3 | N | N | N | S | |
| ACT-20 | Read | Tablets and scrolls in the reader | GD §8 | P | P | P | P1 | |
| ACT-21 | Games of chance | Astragali at the drinking fellowship, betting silver | N:L200 | N | N | N | P3 | |
| ACT-22 | Treat wounds | With medicine or a physician | RS §1.2 | Y | Y | N | K | treat_wounds |
| ACT-23 | Repair and recast at the smith | Service or your own forge | RS §7.3 | Y | Y | N | P3 | repair recast |
| ACT-24 | Petition | At the governor's seat or the council hall: requests, grants, justice | LW §4 | N | N | N | E | |
| ACT-25 | Temple service and vows | Serve as a temple dependant; take a vow | LW §4 | N | N | N | J | |
| ACT-26 | Apprenticeship | Long training in a workshop | LW §4 | N | N | N | E | |
| ACT-27 | Loading work at the wharf | Wages; recruit sailors; meet foreigners | LW §4 | Y | Y | N | P1 | work |
| ACT-28 | Rooftop life | Sleep on roofs in summer; meet astrologers, lovers, thieves | CM §3 | N | N | N | O | |

## 9. The character (CHR, LNG)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| CHR-01 | Six attributes | 1–10; each drives its listed systems | RS §1.1; attributes | Y | Y | N | E | attribute |
| CHR-02 | Attribute growth by exercise | +1 after 12 × the next value skill points under it | LG-character | Y | Y | N | E | |
| CHR-03 | Attribute point every 4 levels | The player picks | RS §1.1 | N | N | N | E | |
| CHR-04 | Rare attribute gains | From quests and rites | RS §1.1 | N | N | N | E | |
| CHR-05 | 27 skills in 6 groups | 0–100 | RS §2.1; skills | Y | Y | N | E | skill effective_skill skill_ids |
| CHR-06 | Skill growth by use | Each use hook gives its XP | RS §2.1 | Y | Y | N | E | |
| CHR-07 | Character XP and levels | Every skill point is XP; cap 40 | RS §2.2 | Y | Y | N | E | level character_xp xp_for_next_level |
| CHR-08 | Talents | One point per level; 41 talents, each with a working effect | RS §2.2; talents | Y | Y | N | E | talent_points talents talents_available choose_talent |
| CHR-09 | Callings | Choose at 5; +50% growth; unlocks its talents | RS §2.3; callings | Y | Y | N | E | calling choose_calling |
| CHR-10 | Specialisations | Choose at 15; perks felt in play | RS §2.3 | Y | Y | N | E | choose_specialisation |
| CHR-11 | Second calling | At 25, slower growth | RS §2.3 | Y | Y | N | E | choose_second_calling |
| CHR-12 | Character sheet and summary | Everything live on one screen | CP E1 | Y | Y | N | E | character_summary |
| CHR-13 | Refusals explained | Every refused choice says why | — | Y | Y | N | E | progression_refusal |
| CHR-14 | Social rank per polity | 0 Outsider … 6 Lugal; gates renting, buying, courts, grants, chariots, seal, kingship | RS §2.4; ranks; D-015 | Y | Y | N | E | rank raise_rank |
| CHR-15 | A patron's act | Rank rises by deeds plus a sealed grant tablet ceremony | RS §2.4 | P | P | N | E | |
| CHR-16 | Ranks die with polities | A fallen polity's ranks become prestige without rights | RS §2.4 | N | N | N | Q | |
| CHR-17 | Outlawry drops rank to 0 | Per jurisdiction | CL §3.3 | Y | Y | N | H | set_outlaw is_outlawed |
| CHR-18 | The prisoner start | Rank 0, Kaldunai rags, the quayside barracks | N:L105; D-009 | P | - | Y | A | |
| CHR-19 | The past-life fragments | Half-remembered names and places from Act II; full recall at rank 4+ | round-2 §3 | N | N | N | MQ | |
| LNG-01 | Languages spoken | Sumerian, Akkadian, the Kaldunai tongue, the eastern tongue, the Suti tongue | RS §2.5; D-024 | N | N | N | E | |
| LNG-02 | Speaking unlocks options and prices | Shared language opens dialogue lines and removes a surcharge | RS §2.5 | N | N | N | E | |
| LNG-03 | Overheard speech per tongue | Speech in unknown tongues is garbled until learned | GD §8 | N | N | N | E | |
| LNG-04 | Scripts read | Cuneiform and the undeciphered Ante-Diluvian script | RS §2.5; round-2 | P | N | N | E | |
| LNG-05 | Unreadable text shown as glyphs | Tablets and signs render as real-looking script until you can read them | GD §8 | N | N | N | T | |
| LNG-06 | Learning languages | By living among speakers, teachers and texts | RS §2.1 | N | N | N | E | |

## 10. Combat, arms and armour (CMB, ARM)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| CMB-01 | The integer combat resolver | Chance, aim, zones, severity, bleed, knockout | module_Combat | Y | Y | N | P5 | attack combat_advance combat_status |
| CMB-02 | Directional melee | Attacks, blocks with the shield, stance; brief and brutal | RS §7; GDp §4.2 | P | P | N | P5 | stance |
| CMB-03 | Ranged | Bows, sling, javelin, throwing stick; ammunition; sway from Agility and drink | RS §7.4 | P | P | N | K | |
| CMB-04 | Zonal wounds | Head, torso, arms, legs; bleeding, fractures, festering, scars, limps | RS §1.2 | Y | Y | N | P5 | wound wounds bleeding player_wounds |
| CMB-05 | Knockout and death | Knockout at 9 blunt to the head or health 15; death leaves a body | LG-combat | Y | Y | N | P5 | is_dead death_count |
| CMB-06 | Morale in a fight | Morale falls with wounds and losses; low morale flees | LG-combat | Y | Y | N | P5 | morale |
| CMB-07 | Combat styles by faction | Eleven styles fight differently | combat_styles | Y | Y | N | K | apply_combat_style combat_style_of |
| CMB-08 | Surrender | Either side can yield | RS; CP K5 | Y | Y | N | K | surrender |
| CMB-09 | Taking prisoners | Captured, held, released | CP K5 | Y | Y | N | K | take_prisoner release_prisoner |
| CMB-10 | Ransom | 30 silver; a shortfall becomes a loan | LG-combat | Y | Y | N | K | ransom |
| CMB-11 | Duels | Agreed fight before witnesses; wounds no crime | LG-combat; laws | Y | Y | N | K | agree_duel |
| CMB-12 | Combat as a crime | Assault, killing in a quarrel, self-defence, slaying a robber | laws | Y | Y | N | K | combat_crime |
| CMB-13 | Group skirmishes | Sides resolved together | module_Combat | Y | Y | N | K | skirmish |
| CMB-14 | NPC combat AI | Guards, bandits, warbands, the Suti, animals each by their style | CP K6 | N | - | N | K | |
| CMB-15 | Band battles | Abstract units with orders: hold, advance, flank, withdraw; morale, discipline | LW §5; CP K7 | N | N | N | K | |
| CMB-16 | Chariots | Two horses, a groom, repairs, terrain limits; rank 4 or palace issue | RS §9 | N | N | N | K | |
| CMB-17 | Weapons in the city | Drawn weapons alarm; guards order you to sheathe | CL §4 | N | N | N | G | |
| CMB-18 | No health-bar slugging | Fights end in seconds; readable hurt | GDp §4.2 | - | - | N | K | |
| CMB-19 | Sieges and set pieces | City assaults and defences; the fall of cities | GDp §4.2 | N | N | N | MQ | |
| ARM-01 | Weapon stats | Damage type, reach, speed, weight, balance, durability, quality | RS §7.1; arms | Y | Y | N | K | |
| ARM-02 | Material tiers | Flint → copper → arsenical → tin bronze; iron a curiosity | RS §7.2 | Y | - | N | K | |
| ARM-03 | Armour zones and layers | 4 zones × 2 layers; protection per damage type | RS §8 | Y | Y | N | K | |
| ARM-04 | Weight, stamina and heat | Armour costs stamina and heat; swimming in it is deadly | RS §8 | P | - | N | K | |
| ARM-05 | Cultural shields and helmets | By faction | RS §8; combat_styles | Y | - | N | K | |
| ARM-06 | Wear and maintenance | Lacing breaks, leather rots, padding goes foul; weapons wear | RS §8 | P | - | N | K | |
| ARM-07 | Armour is identity | Guards and raiders read what you wear before your name | RS §8; CL §4 | N | N | N | G | |
| ARM-08 | Equipment visible | Every equipped item shows on the body | CP S6 | - | - | N | S | |

## 11. Followers, household and animals (FOL, HH, ANM)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| FOL-01 | Followers module | Kind, wage, paid-until, loyalty, order, stance, home, status; saved | LW §5; D-024 | N | N | N | P4 | |
| FOL-02 | Recruit companions | Up to 3; their own conditions and opinion | LW §5 | N | N | N | P4 | |
| FOL-03 | Hire hirelings | Guards, porters, guides, sailors; silver or rations | LW §5 | N | N | N | P4 | |
| FOL-04 | Recruit a band | 5–30; Mercenary calling, rank 3+, or leading a wild group | LW §5 | P | P | N | P4 | player_group |
| FOL-05 | Follow | Formation, doors, no blocking, catch-up only unseen | — | - | - | N | P4 | |
| FOL-06 | Orders | Follow, wait, hold, go home, guard, attack target, stance, use item, talk, dismiss | LW §5 | N | N | N | P4 | |
| FOL-07 | Order wheel | A hotkey while looking at a follower | — | - | - | N | P4 | |
| FOL-08 | Follower inventory | Transfer screen; their own carry capacity | — | N | N | N | P4 | |
| FOL-09 | Follower equipment | Equip with their actor id | — | Y | Y | N | P4 | equip |
| FOL-10 | Pay | Daily wages; auto-pay or debt; complaints at 3 unpaid days, desertion at 7 | LW §5 | N | N | N | P4 | |
| FOL-11 | Follower needs | They eat, drink and sleep | LW §5 | P | P | N | P4 | |
| FOL-12 | Loyalty | From pay, food, treatment, your word, shared danger, faith, rank, reputation | LW §5 | N | N | N | P4 | |
| FOL-13 | Low loyalty | Complaints, refused orders, theft, desertion, betrayal (selling your plans to a rival, reporting you) | LW §5 | N | N | N | P4 | |
| FOL-14 | Approval of your acts | Companions approve or disapprove by their values | LW §5 | N | N | N | P4 | |
| FOL-15 | Followers in combat | Wounds, downed and treatable, permanent death, flight | LW §5 | P | P | N | P5 | |
| FOL-16 | Followers in wild verbs | Camp attacks, band founding, escorts and travel use the real roster; losses land on them | LG-wild | P | P | N | P5 | |
| FOL-17 | Follower screen | Roster, loyalty with reasons, health, needs, wage; tabs for inventory, equipment, orders, relationship, quest | — | - | - | N | P4 | |
| FOL-18 | Companion roster | 6–8 companions with backstories, skills, gods, opinions | LW §5; CP L2 | N | - | N | L | |
| FOL-19 | Companion quest lines | Each finished | LW §8 | N | N | N | L | |
| FOL-20 | Companion lifecycle | Level up, marry, leave, die | LW §5 | N | N | N | L | |
| FOL-21 | Their crimes | Only on order; a share of the blame is yours | CL §2 | N | N | N | P4 | |
| FOL-22 | Travel, skips and saves with followers | They come along and live through time | LW §10 | N | N | N | P4 | |
| FOL-23 | Companions act for you in detention | Gather witnesses, pay compensation, break you out | CL §3.3 | N | N | N | H | |
| FOL-24 | Orphaned followers | Followers of a dead faction are left orphaned, with new quests | RS §4.3 | N | N | N | Q | |
| HH-01 | Household members | Spouse, children, servants, steward live in your house and work your property | LW §5 | N | N | N | L | |
| HH-02 | Courtship and marriage | Bride-price, a contract tablet, alliances by marriage | LW §9; CL §7 | N | N | N | L | |
| HH-03 | Children | Born, grow, school, apprenticeship or temple service; traits show | LW §9 | N | N | N | L | |
| HH-04 | Unfree labour and manumission | Servants bought or inherited and freed by sealed deed; never a reward | RS §5.4 | N | N | N | L | |
| HH-05 | Freed servants as settlers | They keep their own loyalties | RS §5.4 | N | N | N | N | |
| HH-06 | Divorce and inheritance quarrels | Before the hearing | CL §7 | N | N | N | F | |
| HH-07 | Adoption | To secure an heir | LW §9 | N | N | N | P | |
| ANM-01 | Pack donkeys | 65–75 kg loads; feed, water, lameness, loyalty | RS §9 | N | N | N | L | |
| ANM-02 | Dogs | Warn of thieves | LW §5 | N | N | N | L | |
| ANM-03 | Horses and oxen | Riding, carts, ploughs; costly to keep | RS §9 | N | N | N | L | |
| ANM-04 | Breeding | Herds and donkeys breed | RS §5.1, §9 | N | N | N | L | |
| ANM-05 | Animal theft | Animals can be stolen | RS §9 | N | N | N | L | |
| ANM-06 | Herds as assets and targets | Herd heads grow and are raided | LG-wild | Y | Y | N | M | herd_head |
| ANM-07 | Wild animals | Lions, leopards, wolves, bears, boar, snakes, jackals; attack herds, hunt at night | LW §6; wild_encounters | P | P | N | K | |
| ANM-08 | Sacrificial and divination animals | Sheep, goats and birds used in rites | RS §10.1 | P | - | N | J | |

## 12. Property, business and building (PRP, BLD)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| PRP-01 | Asset registry | House, field, orchard, herd, workshop, ship share, caravan partnership | RS §5.1 | Y | N | N | M | |
| PRP-02 | Acquire a house | Rent at rank 1, buy at rank 2, build, or be granted | RS §5.1 | P | N | N | M | |
| PRP-03 | Fields and orchards | Barley, wheat, flax, dates; buy, grant, sharecrop | RS §5.1 | P | N | N | M | |
| PRP-04 | Workshops | Build in a house or on land | RS §5.1 | P | N | N | M | |
| PRP-05 | Ship shares | Voyage profits and risks from the wharf | RS §5.1; GD §7.1 | P | N | N | M | |
| PRP-06 | Caravan partnerships | Fund or join a venture; a contract tablet | RS §5.1 | P | N | N | M | |
| PRP-07 | Loans | 20% silver, 33⅓% barley; debt tablets; collection by law or force | RS §5.1 | Y | N | N | M | |
| PRP-08 | Income to the purse | Assets credit their owner each day | D-017 | Y | Y | P | M | purse |
| PRP-09 | Deeds as tablets | Witnessed and sealed; a lost deed makes the claim contestable | RS §5.2 | P | N | N | M | |
| PRP-10 | Ilku and tribute obligations | Service or goods owed on granted land; failure loses the land | RS §5.2; round-2 §16 | N | N | N | M | |
| PRP-11 | Taxes, dues and corvée | A share to the palace or temple, plus labour | RS §5.2 | N | N | N | M | |
| PRP-12 | The management loop | Staff, supply, time by the calendar, store with losses, sell or feed | RS §5.3 | N | N | N | M | |
| PRP-13 | Storage losses | Pests and damp take stored grain | RS §5.3 | N | N | N | M | |
| PRP-14 | Steward reports | Tablets with errors and dishonesty by loyalty and ability | RS §5.3 | P | N | N | M | |
| PRP-15 | Property ledger screen | Assets, deeds, debts, income | CP M6 | - | - | N | M | |
| PRP-16 | Voyage and caravan risk | Storms, pirates, raids and drought hit returns | RS §5.1 | N | N | N | M | |
| PRP-17 | Debt collection | By lawsuit or by force (a crime) | RS §5.1 | P | N | N | M | |
| PRP-18 | Family shrine and tomb | Houses carry a shrine niche and a house-tomb | RS §5.1; CM §2 | N | N | N | M | |
| PRP-19 | Bankruptcy and seizure | A merchant's house is seized for debt | CL §7 | N | N | N | M | |
| PRP-20 | Your own seal | A cylinder seal as your signature; forging one is fraud | LW §3.2 | N | N | N | E | |
| BLD-01 | Construction | Kit placement with material realism | GDp §4.4 | N | N | N | N | |
| BLD-02 | The settlement ladder | Bedroll → hide camp → mudbrick farmstead → walled village → trade post | GDp §4.4 | N | N | N | N | |
| BLD-03 | Settlement simulation | Population, food, water, morale, defence | GDp §4.4 | N | N | N | N | |
| BLD-04 | Refugee intake | Policies for the drought's refugees; settlers with skills, faiths and grudges | GDp §4.4 | N | N | N | N | |
| BLD-05 | Defence of the settlement | Palisade, watch, band contracts; raiders come | GDp §4.4 | N | N | N | N | |
| BLD-06 | Kingship | People, food security, walls and a force, a temple and patron god with the founding rite, recognition, a royal seal: rank 6 | N:L9, L11; EN §2 | N | N | N | N | |
| BLD-07 | Ruling | Your own law table, land grants, taxes, hearings, marriages, treaties | EN §2 | N | N | N | N | |
| BLD-08 | Alliances | Forge alliances by treaty and marriage | N:L9 | N | N | N | Q | |

## 13. Factions, standing, oaths and treaties (FAC)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| FAC-01 | Standing 0–100 per faction | Moved by deeds, quests, crimes | RS §4.2 | Y | Y | P | Q | standing add_standing |
| FAC-02 | Standing tiers | Stranger, Known, Trusted, Sworn, Oath-bound | RS §4.2 | Y | Y | N | Q | faction_tier |
| FAC-03 | Oaths before the gods | Sworn with witnesses and a curse clause | RS §4.2 | Y | Y | N | Q | oath_count oath |
| FAC-04 | One Great King at a time | Only one great-king oath | RS §4.2 | Y | - | N | Q | |
| FAC-05 | Breaking an oath | The oath-breaker curse; social ruin; Utu's wrath | RS §4.2 | Y | Y | N | Q | break_oath oath_breaker_curse |
| FAC-06 | Mercenary contracts | From any faction at Known | RS §4.2 | N | N | N | Q | |
| FAC-07 | Treaties | Live when both parties sign and neither is outlawed; no_raids blocks raids | treaties; LG-faction-raids | Y | Y | N | Q | treaty_count treaty treaty_live_between |
| FAC-08 | Faction outlawry | An outlawed faction's treaties are void | LG-faction-raids | Y | Y | N | Q | |
| FAC-09 | Faction decay on the clock | Dead factions void oaths and orphan followers | RS §4.3 | N | N | N | Q | |
| FAC-10 | Minor factions' verbs | Fight, negotiate, trade, hire, join or destroy every group | LW §6 | P | P | N | Q | |
| FAC-11 | Faction screen | Standing, tiers, oaths, treaties, curses | CP Q1 | - | - | N | Q | |
| FAC-12 | The Brotherhood's hidden standing | No meter shown; a silent sequence of deeds | GD §5.1; endings | N | N | N | MQ | |
| FAC-13 | Reputation carriers | Your deeds travel with the caravans | GDp §5 | N | N | N | G | |

## 14. Raids, bands, camps, captives and the wild (RAD, WLA)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| RAD-01 | The raid formula | Hunger + opportunity − defence − fear, daily | LW §6; LG-wild | Y | Y | N | P5 | raid_chance |
| RAD-02 | Faction politics in raids | War, grudge, treaty and outlaw points | LG-faction-raids | Y | Y | N | P5 | band_politics |
| RAD-03 | Bands form, eat, recruit, desert, feud, move and re-form | Five bands with drought/war triggers | wild_groups | Y | Y | N | P5 | group_count group |
| RAD-04 | Camps | Occupied, cleared, abandoned; loot pile; captives | LG-wild | Y | Y | N | P5 | camp_count camp |
| RAD-05 | Raids on the city made physical | Raiders come from the camp's side, take goods, fight guards and you, and leave | LW §6; CP K8 | P | P | N | P5 | raid_count raid |
| RAD-06 | Raids on your property | Your fields, herds, containers and assets are targets | GDp §4.4 | N | N | N | P5 | |
| RAD-07 | Warning signs | Rumours, smoke, scouts, omens, the watch's alarm | LW §6 | P | P | N | P5 | hear_rumours |
| RAD-08 | Aftermath | Goods lost, people slain or taken, mourning, prices, notices | CL §7 | P | P | N | P5 | npc_fate |
| RAD-09 | Defending | Guards, your followers and band; gates close; hired guards and dogs add defence | LW §6 | P | N | N | P5 | |
| RAD-10 | Kidnapping in raids | Raiders carry off people as captives (at most 5 a camp) | LG-wild | Y | Y | N | P5 | npc_fate |
| RAD-11 | Captives sold east | After 45 days a captive is lost | LG-wild | Y | Y | N | P5 | |
| RAD-12 | Rescue quests | Each capture starts a world-driven rescue; completing it needs a real rescue | LG-wild; A part 2 | Y | Y | P | P5 | |
| RAD-13 | Ransoming captives | Pay the band for a captive | LG-wild | Y | Y | N | P5 | ransom_captive |
| RAD-14 | The player taken captive | Defeated by raiders: carried to the camp; escape, ransom by friends, or rescue by followers | LW §6 | N | N | N | P5 | |
| RAD-15 | The player taking hostages | Seize a person and demand ransom; a crime | CL §3.3 | P | P | N | P5 | take_prisoner |
| RAD-16 | Camp screen | Scout, infiltrate, attack, pay off, ally, join, challenge, ransom | LW §6 | Y | Y | N | P5 | scout_camp infiltrate_camp attack_camp pay_off_group ally_with_group join_group challenge_leader |
| RAD-17 | Band screen | Lead raids, share loot, recruit, move camp, dismiss, leave | LW §5 | P | P | N | P5 | found_band lead_raid leave_group |
| RAD-18 | Infamy and bounties | Raiding raises infamy and a bounty; fear of you lowers raid odds | LW §6 | Y | Y | N | P5 | infamy |
| RAD-19 | Raid causes you can fight | Feed the hungry, raise defence, frighten them: each changes the formula | LW §6 | Y | Y | N | P5 | |
| RAD-20 | Pirates and sea raiders | Ships and coastal villages hit; captives sold | LW §6 | N | N | N | Q | |
| RAD-21 | Deserters | Desperate armed men; cheap to hire | LW §6 | N | N | N | Q | |
| RAD-22 | Harbouring fugitives | A crime; fugitives returned by treaty | LW §6; laws | P | N | N | H | |
| WLA-01 | Wild places and links | 15 places, 23 links, danger levels | wild_places; wild_links | Y | Y | N | Q | wild_place_count wild_place wild_link_count wild_link |
| WLA-02 | Where the player stands | The kernel knows the player's place | module_Wild | Y | Y | N | Q | player_place set_player_place |
| WLA-03 | Travel | Leg by leg; time by mode, terrain and weather; needs rise; encounters per hour | RS §9; LG-wild | Y | Y | N | Q | travel route_minutes |
| WLA-04 | Encounters | Bandits, raiders, refugees, merchants, animals; stance fight/pay/flee or rob/give/pass | wild_encounters | Y | Y | N | Q | encounter_count encounter |
| WLA-05 | Caravans on the road | Depart, travel, arrive; escort or rob | caravans | Y | Y | N | Q | escort_caravan abandon_escort rob_caravan |
| WLA-06 | Searching sites | Ruins, caves and tombs have loot and risks; tombs never refill | LG-wild | Y | Y | N | Q | search_site |
| WLA-07 | Walkable wild lands | Built physically around the terrain | CP Q4 | - | - | N | Q | |
| WLA-08 | Fast travel | Optional, off by default, using kernel travel time | CL §10 | Y | Y | N | Q | |
| WLA-09 | Transport modes | Foot, donkey, ox-cart, horse, chariot, reed boat | transport_modes | Y | Y | N | Q | |
| WLA-10 | Off-map ventures | Caravans, voyages, pilgrimages, war contracts as event cards; the world keeps ticking | WM §4 | N | N | N | Q | |
| WLA-11 | The three journeys | To the Cities of the Abyss, the Sun and the Warrior Spirit | LG-wild | P | P | N | Q | |
| WLA-12 | Ruins of older ages | Lost objects and forgotten tablets in the tell | LW §4 | P | P | N | Q | |
| WLA-13 | The other cities through people | Envoys, merchants, pilgrims and refugees from all 8 cities | D-002 | N | N | N | Q | |
| WLA-14 | The map you draw | Explored areas fill in; your own marks; a bought royal map | GDp §4.3 | N | - | N | T | |
| WLA-15 | Seasonal migration | The Suti move to the pastures by season; water and grazing tensions | LW §6 | N | N | N | Q | |
| WLA-16 | Night in the wild | Predators prowl; danger rises | LW §6 | P | - | N | Q | |

## 15. People, memory, relationships and life (NPC)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| NPC-01 | Real names | No id strings anywhere in the UI | LW §2 | Y | Y | P | F | npc_name npc_id |
| NPC-02 | Schedules | Tasks, not positions; role rows and person rows; seasonal and festival overrides | LW §2 | Y | Y | Y | F | npc_schedule_at npc_place_at npc_task_at |
| NPC-03 | The full NPC record | Identity, household, livelihood, rank, faction, faith, needs | LW §2 | P | P | N | F | |
| NPC-04 | Goals | Short and long term; they act on them (a debt spiral plays out alone) | LW §2 | N | N | N | F | |
| NPC-05 | Memory | Source, date, place, certainty | LW §2 | P | Y | N | F | npc_memory_count npc_memory_at |
| NPC-06 | Relationships | Opinion of you and of each other: friendship, rivalry, love, grudges, debts | LW §2 | N | N | N | F | |
| NPC-07 | Rumour at walking speed | Along social links; overheard gossip; rumours in the journal | LW §2 | P | P | N | F | hear_rumours |
| NPC-08 | Social links | Household, same role, neighbours | LG-npc | Y | - | - | F | |
| NPC-09 | Promotion of light residents | A light resident becomes a full NPC when you get involved | LW §1 | N | N | N | F | |
| NPC-10 | Population to target | ~160 named people; a household for every building | CM §1 | P | - | P | F | |
| NPC-11 | Crowds | Market days and festivals at historical density | LW §1 | - | - | N | F | |
| NPC-12 | NPC animation and presence | Task animations, sitting, eating, sleeping, talking, bows, barks | CP F9 | - | - | P | F | |
| NPC-13 | Life events | Weddings, births, funerals, divorce, a missing child | LW §11 | N | N | N | F | |
| NPC-14 | The world responds to you | The supplied smith opens a second forge; a child is named after you | LW §11 | N | N | N | F | |
| NPC-15 | Everyone has somewhere to be | Following an NPC for a day shows a whole day | CL §8 | P | P | P | F | |
| NPC-16 | NPC fates | Slain, captive, sold east; the director mirrors it | module_Wild | Y | Y | Y | F | npc_fate |
| NPC-17 | Faith of NPCs | Patron gods, household gods, taboos, festivals attended | LW §2 | P | - | N | F | |
| NPC-18 | NPC needs | Food, water, sleep, safety, money, status, faith, family | LW §2 | P | - | N | F | |

## 16. Reaction, reputation and custom (RCT, REP, CUS)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| RCT-01 | Perceive | Sight lines, light, sound, smell; only perceivers react | CL §2.1 | P | - | N | G | |
| RCT-02 | Judge | Against law, custom and personal values; communities differ | CL §2.1 | N | N | N | G | |
| RCT-03 | Feel | Fear, anger, disgust, pity, amusement, greed, scaled by relationships | CL §2.1 | N | N | N | G | |
| RCT-04 | The response ladder 0–7 | Ignore, remark, confront, refuse, alarm, flee, intervene, attack; by role, courage and position | CL §2.2 | N | N | N | G | |
| RCT-05 | Remember and tell | The act enters memory and becomes rumour | CL §2.1 | P | P | N | G | |
| RCT-06 | Barks | Each rung has its lines and animation | CP G3 | - | - | N | G | |
| REP-01 | Personal reputation | Each NPC's memory of you | CL §2.3 | P | P | N | G | |
| REP-02 | Community reputation | Per quarter, guild, temple and foreign community | CL §2.3 | N | N | N | G | |
| REP-03 | Legal status | Clean, suspected, accused, wanted, outlawed, per jurisdiction | CL §2.3 | P | P | N | G | |
| REP-04 | Effects of reputation | Greetings, prices, credit, doors, marriage offers, guards looking away, rumours following you | CL §2.3 | N | N | N | G | |
| REP-05 | Reputation screen | All three layers | CP G4 | - | - | N | G | |
| REP-06 | Your fear | A killer's reputation deters raiders | LW §6 | Y | Y | N | G | |
| CUS-01 | Customs engine | Every custom has a trigger and a reaction; breaking one costs reputation, access and help | CL §4; customs | N | N | N | G | |
| CUS-02 | Greeting and rank | Bow to rank; formulas; deference | CL §4 | N | N | N | G | |
| CUS-03 | Hospitality | Guests get bread, water and a bed; stealing from a host is worse than theft | CL §4 | N | N | N | G | |
| CUS-04 | Private spaces | Entering a family's inner rooms uninvited is an insult | CL §4 | N | N | N | G | |
| CUS-05 | Dress as a signal | Clothes read rank and origin; wrong dress draws stares | CL §4; GDp §4.1 | N | N | N | G | |
| CUS-06 | Gifts | A gift creates a debt of honour; refusal insults; unreturned gifts damage | CL §4 | N | N | N | G | |
| CUS-07 | Warding engravings | Dog and owl engravings ward houses | customs | N | N | N | G | |
| CUS-08 | Imperial nakedness | The emperor wears no regalia | D-016 | N | - | N | G | |

## 17. Law and justice (LAW)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| LAW-01 | Commit a crime | Witnesses remember; the watch responds; filed on the docket | CL §3.3; LG-actions | Y | N | N | H | |
| LAW-02 | Evidence persists | Unwitnessed crimes can still be solved | CL §3.3 | N | N | N | H | |
| LAW-03 | Enforcers | Gatekeepers, the watch, palace men, elders, the victim's family, temple personnel | CL §3.1 | P | - | P | H | |
| LAW-04 | Gates on the clock | Open at dawn, shut at dusk; strangers questioned; tolls | CL §3.1 | P | P | P | H | |
| LAW-05 | Guard AI | Patrols, alarm radius, chase with stamina, search last seen, question witnesses | CL §3.1 | N | N | N | H | |
| LAW-06 | Your description | Clothes, weapon, build; changing clothes partly shakes it | CL §3.1 | N | N | N | H | |
| LAW-07 | Bribery | Works by personality; a crime if seen | CL §3.1 | N | N | N | H | |
| LAW-08 | Collective liability | The town pays when a foreign merchant's killer isn't found | CL §3.1 | N | N | N | H | |
| LAW-09 | Arrest choices | Surrender, run (wanted), fight (a new crime), talk your way out | CL §3.3 | N | N | N | H | |
| LAW-10 | Detention | A holding room; possessions held as evidence; time passes | CL §3.3 | P | N | N | H | |
| LAW-11 | The hearing | Evidence, witnesses, oaths before the gods; a false oath curses | CL §3.3 | Y | N | N | H | verdict_count |
| LAW-12 | The sealed verdict tablet | An item; legal status updates | CL §3.3 | P | P | N | H | |
| LAW-13 | Sentences | Compensation, confiscation, debt service, corvée, flogging, exile, death | CL §3.3 | P | N | N | H | |
| LAW-14 | Rank scales penalties | Free, dependent and unfree treated differently | CL §3.2 | N | N | N | H | |
| LAW-15 | Escape | Guards on schedules, locks, bribes, friends outside; a crime | CL §3.3 | N | N | N | H | |
| LAW-16 | Outlawry | Lose the city's protection; the hills take you in | CL §3.3 | Y | Y | N | H | set_outlaw |
| LAW-17 | Night rules | Strangers questioned; knocking at night is rude; urgent needs pay extra | CL §1 | N | N | N | H | |
| LAW-18 | Sorcery is a crime | A curse seen goes to trial | RS §10.3 | N | N | N | J | |

## 18. Death, burial and the dead (DTH)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| DTH-01 | Bodies in the world | Found by sight or smell; the finder alarms, runs or robs | CL §6 | P | - | N | I | |
| DTH-02 | The family is told | Mourning begins | CL §6 | P | - | N | I | |
| DTH-03 | Mourning | Wailing, torn clothes, dust on the head, for the right days | CL §4, §6; customs | N | N | N | I | |
| DTH-04 | Burial and procession | To the house-tomb or the Garden of Tombs; grave goods | CL §6; CM §2 | N | N | N | I | |
| DTH-05 | Offerings to the dead | Over the following days and at set times; the ancestor rite | CL §6; customs | N | N | N | I | |
| DTH-06 | Tomb neglect tracked | Per tomb | CP I4 | N | N | N | I | |
| DTH-07 | The restless dead | Neglected or unburied dead haunt: an affliction (Mythic) or bad luck (Chronicle) | CL §6 | N | N | N | I | |
| DTH-08 | Investigation of a killing | Witnesses, the watch, the family's demands | CL §6 | P | - | N | I | |
| DTH-09 | Your dead | You bury companions and family, or the neighbours notice | CL §6 | N | N | N | I | |
| DTH-10 | Your funeral | Who comes and what they say reflects your reputation | CL §6 | N | N | N | I | |
| DTH-11 | Tomb robbery | A crime, a feud and the underworld queen's wrath | CL §3.2 | P | P | N | I | |
| DTH-12 | Royal burial | Kings mummified in catacombs (the City of Kings' custom, through its people) | customs | N | - | N | I | |

## 19. Magic of the gods (MAG)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| MAG-01 | The five powers | Favour, knowledge, materials, purity and place, time weigh every rite | RS §10.1 | Y | Y | N | J | perform_rite |
| MAG-02 | Knowing rites | Learned from teachers or texts | RS §10.1 | Y | Y | N | J | knows_rite known_rites learn_rite_from_teacher learn_rite_from_text rites_taught_by rites_taught_in |
| MAG-03 | Materials spent | One of each, spent even on failure | LG-rites | Y | Y | N | J | |
| MAG-04 | The rite's place | Performed at the right place | RS §10.1 | Y | Y | N | J | set_rite_place rite_place |
| MAG-05 | The rite flow | Prepare → perform (a sequence of actions and words in the right language) → outcome → effect | RS §10.1 | P | P | N | J | |
| MAG-06 | Favour per god | 0–100 per deity; the favour screen | RS §10.4 | Y | Y | N | J | favour add_favour |
| MAG-07 | Equated gods share favour | Inanna/Ishtar, Enki/Ea, Nanna/Sîn, Utu/Shamash | RS §10.4 | N | N | N | J | |
| MAG-08 | Neglected gods get angry | Misfortune events | RS §10.4 | N | N | N | J | |
| MAG-09 | Patron god | A strong bond, a unique high rite, taboos to keep | RS §10.4 | N | N | N | J | |
| MAG-10 | Protection | Wards on places for days; amulets | RS §10.3 | Y | Y | N | J | ward_until warded |
| MAG-11 | Healing and purification | Cure illness, close wounds, restore purity, lift curses; transferred illness goes somewhere | RS §10.3 | N | N | N | J | |
| MAG-12 | Divination family | Omens with probabilities, clarity by skill | RS §10.3 | Y | Y | N | J | omen_count omen |
| MAG-13 | Blessing | Fields, voyages, battle luck, births by season and domain | RS §10.3 | N | N | N | J | |
| MAG-14 | Curse and binding | Hexes, oath-curses, binding a rival's tongue; sorcery is a crime | RS §10.3 | N | N | N | J | |
| MAG-15 | Substitution | A figurine or animal takes the blow | RS §10.3 | N | N | N | J | |
| MAG-16 | Ancestors | The dead of your house advise and protect; needs a house-tomb | RS §10.3 | N | N | N | J | |
| MAG-17 | Divine intervention | Rare world events at high favour plus a crisis | RS §10.3 | N | N | N | J | |
| MAG-18 | Divine wrath | Per offender and god; ill omens, disfavour, the curse; decay | LG-divine | Y | Y | N | J | divine_wrath divine_tier divine_cursed divine_entry_count divine_entry |
| MAG-19 | Atonement | The šu-ila lifts wrath and the curse | LG-divine | Y | Y | N | J | |
| MAG-20 | Wrath made visible | Failed harvests, lightning on a temple, plague | CP J9 | N | N | N | J | |
| MAG-21 | Guard rails | Rites take hours to days and real goods; no combat spells; omens never certain; the clock never changes | RS §10.5 | P | - | - | J | |
| MAG-22 | Chronicle mode | The same rites and costs; statistical, deniable effects | RS §0 | N | N | N | J | |
| MAG-23 | Mythic mode default | Rites work, omens land, intervention happens; the demonic claim never confirmed | D-004 | P | - | - | J | |
| MAG-24 | Magic XP and Sacred skills | Rites, divination and incantation grow and weight outcomes | RS §2.1 | P | P | N | J | |

## 20. The notes' magick (MGK) — D-025: every form is a working system

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| MGK-01 | Numerology, basic | The gods' numbers (An 60, Enlil 50, Enki 40, Nanna 30, Utu 20, Inanna 15) modify rites | N:L189; D-024 | N | N | N | J | |
| MGK-02 | Numerology, advanced | Name numbers modify rites and omens | N:L189 | N | N | N | J | |
| MGK-03 | As above, so below | Sky omens mirror earthly events; correspondences of planets, gods, metals, days and directions | N:L190 | N | N | N | J | |
| MGK-04 | Astrology | 12 primitive zodiac signs plus some extra constellations; birth signs; auspicious hours; the Star Terrace | N:L191; CM §2 | N | N | N | J | |
| MGK-05 | Barûtu haruspicy | The liver of a burned goat reads the future | N:L192, L202 | Y | Y | N | J | |
| MGK-06 | Sex magick | Inanna's sacred rites, off-screen | N:L193; D-025 | N | N | N | J | |
| MGK-07 | Hymns | Vibrating the names of the gods raises favour | N:L194 | Y | Y | N | J | |
| MGK-08 | Astrotheology | Sun, Moon, Saturn, Ishtar/Venus and Mercury-Enki as rite powers | N:L195 | N | N | N | J | |
| MGK-09 | Demonology | Evil, neutral and benevolent demons; medicine as exorcism | N:L196 | N | N | N | J | |
| MGK-10 | Amulets and talismans | Worn to repel demons and bad spirits | N:L197 | N | N | N | J | |
| MGK-11 | Zisurru | Flour circles protect people and places: SAG.BA.SAG.BA | N:L198 | Y | Y | N | J | |
| MGK-12 | Earth: wealth and crop yield | Nature magic on fields and herds | N:L204 | N | N | N | J | |
| MGK-13 | Moon: dream reading and incubation | Sleep at the temple for a dream answer | N:L205 | N | N | N | J | |
| MGK-14 | Moon: fortune telling | Read a person's future | N:L205 | N | N | N | J | |
| MGK-15 | Moon: psychological magic | Change an NPC's mood | N:L205 | N | N | N | J | |
| MGK-16 | Moon: animal telepathy | Calm, call or read herds and mounts | N:L205 | N | N | N | J | |
| MGK-17 | Moon: mind control | Works, is sorcery, and costs divine wrath (bad karma) | N:L205 | N | N | N | J | |
| MGK-18 | Moon: astral travel | A trance that scouts a distant place on the region map | N:L205 | N | N | N | J | |
| MGK-19 | Moon: telepathy | Read an NPC's intent | N:L205 | N | N | N | J | |
| MGK-20 | Moon: psychometry | Read an object's history and owner; crime evidence | N:L205 | N | N | N | J | |
| MGK-21 | Mercury: high ceremonial rites | Long formal rituals | N:L206 | N | N | N | J | |
| MGK-22 | Mercury: invocations | Invoke a god's presence | N:L206 | N | N | N | J | |
| MGK-23 | Mercury: pathworking | Guided visions that teach rites or reveal lore | N:L206 | N | N | N | J | |
| MGK-24 | Venus: love, allure, beauty | Opinion and attraction rites | N:L207 | N | N | N | J | |
| MGK-25 | Venus: fertility | People, herds and fields | N:L207 | N | N | N | J | |
| MGK-26 | Venus: unions | Marriages and alliances blessed | N:L207 | N | N | N | J | |
| MGK-27 | Venus: binding of hearts | Against the will: bad karma, sorcery | N:L207 | N | N | N | J | |
| MGK-28 | Venus: morning star | Battle-fury, victory, conquest, tribute | N:L207 | N | N | N | J | |
| MGK-29 | Venus: evening star | Mercy, peace sworn after war | N:L207 | N | N | N | J | |
| MGK-30 | Sun: kingship and exposure | Legitimacy, oaths exposed, hidden things revealed, vitality | planetary_powers | N | N | N | J | |
| MGK-31 | Saturn: time and boundaries | Endurance, boundaries, binding curses | planetary_powers | N | N | N | J | |
| MGK-32 | Sea gems | Gifts of Enki: magical, found on the shore, used in rites | D-016 | P | - | N | J | |
| MGK-33 | Codex page per form | Every form has its page | CP J6 | N | - | N | T | |

## 21. Divination forms (DIV)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| DIV-01 | Astragalomancy and cleromancy | Roll bones or lots; a probabilistic answer | N:L200 | N | N | N | J | |
| DIV-02 | Pessomancy | Coloured pieces instead of numbered | N:L201 | N | N | N | J | |
| DIV-03 | Haruspicy | See MGK-05 | N:L202 | Y | Y | N | J | |
| DIV-04 | Aeromancy | Clouds, winds and cosmic events | N:L203 | N | N | N | J | |
| DIV-05 | Austromancy | Wind divination (reads the live weather) | N:L203 | N | N | N | J | |
| DIV-06 | Ceraunoscopy | Thunder and lightning | N:L203 | N | N | N | J | |
| DIV-07 | Chaomancy | Aerial vision | N:L203 | N | N | N | J | |
| DIV-08 | Meteormancy | Meteors and shooting stars (reads the comet event) | N:L203 | N | N | N | J | |
| DIV-09 | Nephomancy | Clouds | N:L203 | N | N | N | J | |
| DIV-10 | Clarity by skill | Better skill, clearer answer; never certain | RS §10.3 | Y | - | N | J | |
| DIV-11 | Diviners as a service | Buy a reading | LW §3.2 | N | N | N | J | |

## 22. Festivals, events and omens (FST, EVT)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| FST-01 | Festival days | Four festivals with gatherings, places and exempt roles | festivals | Y | Y | N | O | is_festival is_festival_day festival festival_on |
| FST-02 | Moon rites at the Moon Pool | Crowds and offerings on the eššešu days and šapattu | CM §3; D-024 | N | N | N | O | |
| FST-03 | The full ritual year | Every month has its observances | CP O4 | P | - | N | O | |
| FST-04 | Processions | The crowd switches to procession paths | CL §5 | N | N | N | O | |
| FST-05 | Festival hours | Shops keep festival hours; the market may close | CL §5 | Y | Y | N | O | |
| FST-06 | Festival crime and patrols | Pickpockets rise; so do patrols | CL §5 | P | - | N | O | |
| FST-07 | Favour for taking part | Attending and serving raise favour | CL §5 | N | N | N | O | |
| FST-08 | Matchmaking and deals | Marriages arranged, deals struck | CL §5 | N | N | N | O | |
| FST-09 | Ritual roles for the player | Carry offerings, sing, sacrifice, guard, by rank and calling | CL §5 | N | N | N | O | |
| FST-10 | The tribute basket | Received by the moon priest at Ishtar's Torch | D-016 | P | - | N | O | |
| EVT-01 | Event engine | Trigger-driven; participants, places, persistent consequences | CL §7 | P | Y | N | P | event_count event_at |
| EVT-02 | Event chains | Fire → homeless → shelter or theft → lawsuit | CL §7 | N | N | N | P | |
| EVT-03 | Join, ignore, help, exploit or stop | Every event offers these | CL §7 | N | N | N | P | |
| EVT-04 | Frequency | 1–3 visible a day near the player; majors spaced out | CL §7 | N | - | N | P | |
| EVT-05 | Event staging | Fire and smoke, crowds, a brawl, a funeral, an eclipse | CP P2 | - | - | N | P | |
| EVT-06 | Lunar eclipse | Only at the full moon; the moon darkens and reddens; fear, extra rites, a run on the diviners | events; CL §7 | P | - | N | O | |
| EVT-07 | Solar eclipse | Only at the new moon; the day darkens; the city panics | CL §7 | N | N | N | O | |
| EVT-08 | Comets and shooting stars | Seen from the lighthouse; meteormancy reads them | events | P | - | N | O | |
| EVT-09 | Fire in a quarter | Spreads; buildings damaged; homeless families | CL §7 | N | N | N | P | |
| EVT-10 | Locusts | Fields stripped; prices rise | events | P | - | N | P | |
| EVT-11 | Epidemics | Fever in the quarter; a sick leader with public rites | events | P | - | N | P | |
| EVT-12 | Earthquakes and floods | Damage and relief | CL §7 | N | N | N | P | |
| EVT-13 | Refugees at the gate | Newcomers with needs; the camp grows | events | P | - | N | P | |
| EVT-14 | Strange births and lightning on a temple | Omens that change behaviour | CL §7 | N | N | N | P | |
| EVT-15 | A demon at the cradle | Lamaštu threatens a birth; exorcism answers | events | P | - | N | P | |
| EVT-16 | Political events | Envoys demanding troops or grain, proclamations, levies, arrests | CL §7 | P | - | N | P | |
| EVT-17 | Ship arrivals and caravan days | Crowds, news, goods | CL §5 | P | - | N | C | |

## 23. Quests, dialogue, journal and the story's systems (QST, DLG, END)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| QST-01 | Quest list and state | Offered, active, completed, failed; title, giver, kind, act, stage, deadline | LW §8 | Y | Y | P | P | quest_count quest_at quest_title quest_giver quest_kind quest_act quest_stage quest_deadline |
| QST-02 | Accept, advance, complete, abandon | With rewards of silver and standing | LW §8 | Y | Y | P | P | quest_accept quest_advance quest_complete quest_abandon |
| QST-03 | Quests fail and time out | The world tells you what happened | LW §8 | Y | Y | N | P | |
| QST-04 | The journal | Quests, rumours, learned facts, people you know | LW §8 | P | Y | P | P | journal_count journal_at |
| QST-05 | Systemic quests | Generated from needs and the world: runaway donkey, escort, debt, sick child, poisoned well | LW §8 | N | N | N | P | |
| QST-06 | Emergent quests | From your own acts | LW §8 | P | P | N | P | |
| QST-07 | Faction contracts | By standing | LW §8 | P | P | N | P | |
| QST-08 | Delivery channels | Talk, letters to your house, rumour, the crier, oracles and omens | LW §8 | N | N | N | P | |
| QST-09 | No markers by default | Directions in words; guided mode adds markers | LW §8 | - | - | N | P | |
| QST-10 | Open play | No system gates on act or quest state | D-021 | Y | Y | Y | A | |
| DLG-01 | Dialogue lines | Speaker match; act eligibility | LG-npc | Y | Y | P | P | dialogue_count dialogue_at |
| DLG-02 | Dialogue screen | Choices with conditions from language, rank, reputation, skill and memory | CP P4 | N | N | N | P2 | |
| DLG-03 | Dialogue content for every NPC | Greetings by reputation, work talk, rumours, trade, refusals | CP P5 | P | - | N | P | |
| DLG-04 | Persuasion and intimidation | Skill checks in talk | RS §2.1 | N | N | N | P | |
| DLG-05 | Ancient-language exclamations | Short spoken lines, subtitled | EN §6 | - | - | N | R | |
| END-01 | Heirs | Choose an heir; property by will; debts, oaths, partial rank and family reputation pass; skills don't | LW §9; D-003 | N | N | N | P | |
| END-02 | Wills | A will on a tablet; without one, custom splits and relatives dispute | LW §9 | N | N | N | P | |
| END-03 | The line ends | Epitaph, then a new character in the changed world | LW §9 | N | N | N | P | |
| END-04 | The epitaph | Written from a log of what you actually did | EN §4 | N | N | N | P | |
| END-05 | Permadeath on Historical | Otherwise the heir flow or a wake-up rule | RS §12 | N | N | N | B | |
| END-06 | The four endings' unlock tracking | Standing and Act IV actions, measured silently | endings | N | N | N | MQ | |
| END-07 | The hidden sequence tracker | Five silent steps; missing one closes the path | round-2 §12 | N | N | N | MQ | |

## 24. The interface, diegetic UI, settings and saves (UI, DGT, SET, SAV)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| UI-01 | Main menu, pause, loading | Continue, New Game, Load, Settings, Quit | CP A10 | - | - | Y | A | |
| UI-02 | HUD | Needs, health, conditions, purse, date, moon, omen, observance | CP B1 | - | - | P | B | |
| UI-03 | Toasts and notice history | Every notice readable later | FND-09 | - | - | N | P0 | |
| UI-04 | Every screen follows the rules | Keyboard and controller, Esc/B back, pause rule, string tables, refusals shown, tests, screenshots | plan §2.9 | - | - | P | P0 | |
| UI-05 | Death, heir and epitaph screens | On death | CP B10 | - | - | N | P | |
| UI-06 | Arrest, hearing and verdict screens | When arrested | CP H5, H7 | - | - | N | H | |
| UI-07 | Rites and divination screens | Prepare and perform | CP J1 | - | - | N | J | |
| DGT-01 | The Scribe's Case (codex) | Fills only with what you learned; tags shown; sources toggle | GD §8 | - | - | N | T | |
| DGT-02 | The map you draw | See WLA-14 | GD §8 | - | - | N | T | |
| DGT-03 | The tablet reader | Letters, deeds, debt notes, verdicts, reports, rituals | GD §8 | - | - | P | T | |
| DGT-04 | The scales | Weighing silver | GD §8 | - | - | N | T | |
| DGT-05 | Signs are script | No English signs; shops known by goods and noise | CL §8 | - | - | N | S | |
| DGT-06 | Smells as signals | Smoke, a corpse, the dye works, bread | CL §8 | N | - | N | S | |
| SET-01 | Graphics, audio, controls | Persist per user | A11 | - | - | Y | A | |
| SET-02 | Rebinding | Every action rebindable; conflicts resolved | GD §8 | - | - | Y | A | |
| SET-03 | Gameplay toggles | Mythic/Chronicle, guided mode, fast travel, needs severity, illness, permadeath | A11 | P | P | Y | A | |
| SET-04 | Each toggle read by its system | Stored now; read when the system lands | A11 | P | - | P | A | |
| SET-05 | Accessibility | Subtitles, text size, colour vision, player voice | GD §8 | - | - | Y | A | |
| SET-06 | Player voice on or off | The notes' first line | N:L1 | - | - | P | R | |
| SAV-01 | Save and load slots | Named slots, quicksave, autosave on sleep; bad saves refused | CP A10 | Y | Y | Y | A | |
| SAV-02 | Everything saved | Every new state has a snapshot section | AR §5 | P | - | P | U | |
| SAV-03 | Save versioning | Old saves load after content changes | CP U3 | P | - | N | U | |
| SAV-04 | Save size | ≤ 50 MB | AR §5 | - | - | N | U | |

## 25. The world's presence: audio, visuals, night (AV)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| AV-01 | Soundscape by hour | Querns at dawn, smiths, gulls, prayers, dogs, the watch | CL §8 | - | - | N | R | |
| AV-02 | SFX for every verb | No silent action | CP R3 | - | - | N | R | |
| AV-03 | Music by state | Time, season, festival, tension, combat | CP R4 | - | - | N | R | |
| AV-04 | Voice hooks | Every line has a voice slot and subtitle | D-024 | - | - | N | R | |
| AV-05 | Real darkness and carried light | Lamps and torches; you need a lamp in a moonless alley | CL §8 | - | - | P | O | |
| AV-06 | Weather visuals | Rain, dust storms, haze, heat, fog; the drought on fields and canals | CP O6 | - | - | N | O | |
| AV-07 | The lagoon's clock | Boats out at dawn, back at dusk; ships on schedule; the lighthouse beam turns at night | CM §3 | - | - | N | O | |
| AV-08 | Animations | Every task step, combat, bows, mourning, prayer, sitting, sleeping | CP S8 | - | - | P | S | |
| AV-09 | VFX | Fire, smoke, dust, blood, water, subtle rite effects, omens | CP S9 | - | - | N | S | |
| AV-10 | Interiors and usable places | Every building has an interior, an owner and something to do | CM; CP S1–S3 | P | P | P | S | |
| AV-11 | Clothing | Rank and origin dress for each community | CP S5 | - | - | N | S | |
| AV-12 | Camera | Third person; collision-aware; shoulder swap; aim zoom; interiors | D-024 | - | - | Y | A | |

## 26. Development, quality and shipping (DEV)

| ID | Mechanic | Rule / done when | Source | K | C | U | Phase | Calls |
|---|---|---|---|---|---|---|---|---|
| DEV-01 | Developer console | A sim.* command per system | CP A5 | - | - | Y | A | |
| DEV-02 | CI | Kernel, lint, coverage, staging, registry on every push | CP A2 | - | - | - | A | |
| DEV-03 | Smoke and automation tests | Boot, a day, save/load, market walk, a crafting chain | CP A3 | - | - | P | A | |
| DEV-04 | Soak tests | 30 days headless and 3 in engine, clean | CP U4 | N | - | N | U | |
| DEV-05 | Performance budget | 60 fps in the market at noon; 40–60 L0 NPCs | AR §5 | - | - | N | U | |
| DEV-06 | Localisation | No literal strings in UI code | AR §6 | - | - | P | U | |
| DEV-07 | Packaging | Windows and Linux builds; crash reports; version stamp | CP U6 | - | - | N | U | |
| DEV-08 | Final completeness audit | Every row here done and tested | CP U7 | - | - | - | U | |
