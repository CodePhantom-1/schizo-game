# COMPLETION PLAN — everything except the main quest

**Status:** v1 (2026-09-25) · the step-by-step plan for finishing the game down to the smallest detail.
**Scope (the designer's order, 2026-09-25):** *"the main quest doesn't matter right now as much as everything else, which is of utmost importance. Absolutely everything else must be completed to the most minuscule detail."*
**Sources of "everything":** the notes ([../db/sources/notes.md](../db/sources/notes.md)); the imported systems ([mechanics.md](mechanics.md), which points at the parent blueprints [rpg-systems](../../docs/rpg-systems.md), [living-world](../../docs/living-world.md), [city-life](../../docs/city-life.md), [game-design](../../docs/game-design.md) and [architecture](../../docs/architecture.md)); every decision in [../DECISIONS.md](../DECISIONS.md). Each step below cites the section it completes, so nothing is homeless.

---

## 0. Where we start (measured 2026-09-25, `main` @ `b5a0e5e`)

| Layer | State |
|---|---|
| Kernel | 40/40 test suites. Deep systems exist: needs, economy, crafting, justice, rites and magic, divine wrath, combat and wounds, progression, wild lands and raids, factions and treaties, quests, dialogue, property, save/load. |
| Canon data | 48 tables, 0 `OPEN` rows. Several are thin: rites 5, festivals 4, person_schedules 4, recipes 8, events 16, laws 13. |
| **Bridge (C API → Unreal)** | **The kernel exposes 176 C API calls and Unreal uses 28.** Most built systems cannot be played yet. Dialogue and journal are not in the C API at all. |
| Unreal | One street (25 places, 16 doors), the E/F/G/Tab verbs, a needs HUD, townspeople on schedules, day/night, and the humans and dressing art pass. Tommy's `b5a0e5e` adds the whole city around the quarter: terrain, walls, the Moon Gate, the ziggurat, the lighthouse, sky, and Windows Lumen. |
| Missing everywhere | Menus, settings, save UI, audio, dialogue UI, codex, map, languages, followers, household, death and burial, customs reactions, reputation layers, NPC relationships and goals, building and settling, festivals in play, animals, mounts, packaging, CI |

**Parked (main quest, to be done later):** the act beats and act transitions, the story quest line (`quests.csv` rows whose `act` column is set as a story beat), the ending unlocks and ending sequences, the hidden Brotherhood sequence, the war set pieces, the cliffhanger. **Everything the main quest will need as a system is built here**: kingship, the time-skip machinery, heirs, the deed recap, divine intervention, the faction clock. When the main quest resumes it is only content and wiring.

## 1. How every step is executed (the rules, unchanged)

1. **Wave → merge → bug review of the merged range → fix → gate → push → next wave** (the designer's rule). Push `main` before launching agents, because worktrees are cut from `origin/main`.
2. **Gate:** kernel `ctest` + `canon_lint` + `coverage_check` + `stage_canon_for_ue --check` + a UE build. From step A3 on, add the UE smoke test.
3. **One step = code + data + docs + tests** in one commit. Content that fills a gap is tagged `INVENTED` (D-018) and listed in a ledger under `docs/proposals/`.
4. **A step is "done" only when it is playable**: reachable in the running game with a UI or verb, and covered by a kernel test plus a UE functional or smoke test. A kernel function that no player can trigger is not done.
5. **No hardcoded strings.** All text goes through UE string tables from the first UI step (localisation-ready, plan.md §9).
6. **Two people, one repo:** each stage names an owner. Tommy owns the Windows build and the environment/visual layer (`SimEnvironment`, `SimMeshKit`, style materials). The coordinator owns the kernel, data, tools and gameplay code. Visual and gameplay changes meet through documented actor interfaces.

## 2. Decisions (answered by the designer, 2026-09-25 — D-024)

| # | Question | Answer |
|---|---|---|
| DQ1 | Camera | **Third-person only.** |
| DQ2 | Sim day length | **Adjustable** (setting + cvar); the default is 48 real minutes. |
| DQ3 | The city | **Rebuilt (D-025):** a compact crescent of ~110 buildings around the sacred lagoon, nine quarters, **every building simulated**, ~160 named people. Design: [city-of-the-moon.md](city-of-the-moon.md). Built in Stage AA. |
| DQ4 | The magick | **Everything the notes name is built (D-025)**: numerology, primitive kabbalah/hermeticism, 12-zodiac astrology plus extra constellations, hymns, sex magick (off-screen), astrotheology, demonology, amulets, zisurru, every divination form, and the Earth, Moon (incl. animal telepathy, mind control as bad karma, astral travel, telepathy, psychometry), Mercury (incl. pathworking) and Venus lists. The notes are the canon; the historical record is used where it has something. |
| DQ5 | Voices | **Tommy records them later.** No TTS. The voice-line hooks and the player-voice on/off setting are built now. |
| DQ6 | Git LFS | **On** (done: binaries are in LFS from commit 2 of this plan). Every clone needs git-lfs. |
| DQ7 | The calendar | **On:** moon phases, new-moon and full-moon rites to Nanna, lucky and unlucky days, on top of the existing 12×30 months and 4 festival days. Moved forward to step A14. |
| — | The missing systems | **Built according to lore** (D-024 §8), `INVENTED` under D-018 and ledgered. |

---

## Stage A: Foundation (nothing else starts before this is green)

| # | Step | Done when |
|---|---|---|
| A1 | **Verify Tommy's commit on Linux**: `SimEnvironment`/`SimMeshKit` compile; the ProceduralMeshComponent plugin is enabled; `ue_make_style_materials.py` has been run headless; the iGPU launch still holds 30+ fps. | A Linux `-game` launch shows the city without GPU wedging; any Windows-only paths are guarded |
| A2 | **CI on GitHub Actions**: kernel ctest, canon_lint, coverage_check, stage_canon --check on every push | A red badge blocks the merge |
| A3 | **Smoke tests**: `tools/ue_smoke.sh` boots the real map headless and proves the chain (the city stands, the kernel world is created from the canon, a day and a half turns over with the calendar, no crash/assert/ensure); `tools/ue_test.sh` runs the `Sim.*` automation tests and fails on a crash, a timeout or any test that did not complete. **Each later step adds its own smoke line**: save→load round trip with A10, the market walk with C3, one crafting chain with D1 | Both scripts exit 0 on main; CI runs the runner's own tests |
| A4 | **Bring-up TODOs**: the subsystem initialises twice (a per-world guard); the day length is configurable (DQ2) | Logged once; a cvar sets the day length |
| A5 | **Developer console**: sim.AdvanceDays, sim.AdvanceHours, sim.Give, sim.SetNeed, sim.Standing, sim.Favour, sim.Drought, sim.War, sim.Dump, sim.Teleport, sim.Places for the systems live today; **every later stage adds its own system's commands** (sim.Crime with H, sim.Festival with O, sim.Weather with O6, sim.Kill with I, sim.Spawn with F). Headless use: `-ExecCmds="cmd1, cmd2, quit"` (UE separates with commas) | Each system in stages B–T can be tested in seconds |
| A6 | **The Blueprint-callable bridge**: wrap all 176 C API calls in `USimWorldSubsystem` function libraries grouped by module (Needs, Items, Trade, Craft, Character, Rites, Divine, Combat, Wild, Faction, Justice, Property, Quests, Dialogue), each with string-table-backed results | A generated check lists every C API function mapped to a UFUNCTION (no gaps) |
| A7 | **Expose what the kernel has but the C API lacks**: Dialogue.hpp, journal, quests (list/state/accept/advance), rumours heard, NPC memory of the player, events feed, the calendar (month, moon phase, festival today) | A C API test per new call; no-throw tests extended |
| A8 | **The UI framework** (CommonUI): the widget stack, the interaction prompt, a context verb menu (the full verb set: touch, sit, eat, drink, carry, open, knock, pray, work, talk, trade, steal, attack), modal panels, toast notifications, the tablet reader, controller and keyboard navigation | Every later UI is a panel on this stack |
| A9 | **Enhanced Input with full remap** (game-design §8) | Every action can be rebound, and the binding persists |
| A10 | **Main menu, pause menu, loading screen** (explaining first-launch shader compiles), and **save/load slots**: kernel snapshot plus UE state (player transform, time of day, spawned actors); autosave on sleep; quicksave | Save, quit, relaunch and load restores the identical world; the save is ≤ 50 MB |
| A11 | **Settings**: graphics, audio, controls, gameplay (Mythic/Chronicle, guided mode, fast travel, needs severity, disease, permadeath/Historical difficulty, player voice on/off, subtitles, subtitle size, colour-blind palettes) | Each toggle is read by the system it names |
| A12 | **New game**: the prisoner start is a *system*, not the story. You start in the quayside barracks with rank 0, Kaldunai rags and the starting inventory. The scripted intro stays parked. | New Game drops you into a playable world |
| A13 | Housekeeping: delete merged branches and worktrees (`art/*`, `verify/gameplay`, `claude/admiring-babbage-c0jyzr`) after the designer confirms | `git branch -a` shows only live work |
| A14 | **The calendar turned on** (DQ7): moon phase in the kernel (new on day 1, full on day 15, integer illumination), month names, the named days of the month (`calendar_days.csv`: the eššešu days of Nanna 1/7/15, šapattu, the unfavourable days 7/14/19/21/28, bubbulum), the C API, the HUD's two date lines, and the moon placed and lit by its phase, moving smoothly through the month. **The moon rites that fire with crowds at the Moon Pool move to O4** (they need festival mechanics and the rebuilt city) | The HUD shows the date, the month, the moon, the omen and the observance; the moon follows the month |
| A15 | **Third-person camera polish** (DQ1): collision-aware boom, shoulder swap, aim zoom, interior auto-shorten | The camera never clips through walls in interiors |

**Stage A gate:** start from the menu, change a setting, play, save, load, quit. CI is green.

---

## Stage AA: The City of the Moon rebuilt (D-025; design: [city-of-the-moon.md](city-of-the-moon.md))

| # | Step | Done when |
|---|---|---|
| AA1 | **Agree the change with Tommy**: show him city-of-the-moon.md; he keeps the look (materials, light, the ziggurat, lighthouse and gate builders), and the layout becomes data | Tommy agrees, or the design is adjusted |
| AA2 | **The layout as canon**: `city_districts.csv` (the 9 quarters) and `places.csv` extended to all ~110 buildings with district, kind, owner household and footprint (x, y, yaw, size); `canon_lint` checks footprints don't overlap and every building has an owner | The lint passes; a top-down plot (`tools/plot_city.py` → PNG) reads as a crescent moon |
| AA3 | **The kernel reads it**: every place has an owner household and kind; schedules resolve to the new places | Kernel tests pass with the new places |
| AA4 | **`SimEnvironment` builds the crescent from the data**: the crescent wall and the lagoon shore as curves; the Moon Pool; the reed quarter on stilts and islands; bridges, stairs to the water, rooftop walkways; courtyard gardens and palms; the grid `BuildCity` removed | Screenshots from the ziggurat top and the lighthouse show the crescent; 30+ fps on the iGPU |
| AA5 | **`SimStreetBuilder` generalised to every place**: a door, a registry entry and a door slot for all ~110 buildings; the old single street folded into the Moon Gate quarter | `sim.Places` lists every building; `sim.Teleport` reaches each one |
| AA6 | **The prisoner start moved** to the quayside prison barracks (quarter 1) | New Game starts there |

---

## Stage V: The world made alive — buildings, materials, terrain, flora, fauna ([world-art-plan.md](world-art-plan.md))

The designer's brief (2026-09-26): every kind of building for every trade and class, rich materials and surface detail instead of plain blocks, real terrain relief and textures, regional flora and fauna of all kinds. The full plan, catalogue and steps V0–V10 are in [world-art-plan.md](world-art-plan.md). V0–V3 run together with Stage AA (the crescent is built in the new style from the start); V4 with C, V5–V7 with Q, V8 with O, V9 with Q7; V10 is continuous. Stage S's art rows (S2 interiors, S5–S9) are carried out through Stage V.

---

## Stage B: The body (rpg-systems §1.2, §6; game-design §4.1)

| # | Step | Done when |
|---|---|---|
| B1 | HUD for every condition, diegetic where possible: hunger, thirst, fatigue, **heat/cold**, wounds per zone, illness, **purity** (clean → impure → defiled), intoxication, encumbrance | Every condition is visible and has a text tooltip |
| B2 | **Heat and cold**: from season, weather, time of day, shade and interiors, armour weight and layers, activity. Heatstroke and chill as conditions. | Wearing bronze scale at a summer noon sends you toward heatstroke |
| B3 | **Illness**: fever, dysentery, eye disease, plague. Contagion from crowding, famine, ships and bad water; incubation; symptoms; remedies from the physician's craft; the disease toggle respected | An NPC carries fever through the port quarter; a remedy cures it |
| B4 | **Purity in play**: lowered by touching a corpse, blood, certain foods, sex and oath-breaking; restored by washing at a well or river, by rites and by time. Temples refuse the impure (custom). | Touching a body makes you impure and the temple bars you until you wash |
| B5 | **Intoxication**: stages for beer and wine, social bonuses, then penalties (aim sway, slurred dialogue options) | Three beers change dialogue and aiming |
| B6 | **Diet balance**: 5 food groups, the Well-fed bonus (stamina, healing), Weakness and illness from a narrow diet | A bread-only week gives Weakness |
| B7 | **Spoilage and preservation**: fresh food spoils in hours to days; drying, salting, oil and fermenting preserve it | Fish rots in a day; salted fish keeps |
| B8 | **Sleep**: bed quality, sleeping rough, rooftop sleeping in summer, being woken by events, the guest bed under hospitality | Sleep quality changes how much fatigue you recover |
| B9 | **Inventory with weight**: carry limit from Strength, containers, stack rules; every object has an owner or none, and a stolen flag | Overloading slows you; stolen items are flagged |
| B10 | **Death of the player**: from starvation, wounds, illness or execution. Death leads into the heir flow (step P6) or, on non-Historical difficulties, a reload/wake-up rule. | Every cause of death leads to a defined outcome |

---

## Stage C: Things, money and trade (living-world §3; rpg-systems §5)

| # | Step | Done when |
|---|---|---|
| C1 | **Item catalogue storm** against every category in living-world §3.3: food and drink, raw materials, tools, weapons, armour, clothing and ornaments, containers, furniture, ritual goods, medicine, writing materials, luxuries, animals, vehicles and boats, **documents** (letters, deeds, debt notes, rituals: items with power), trade goods. The notes' own items included: sea gems, fish offerings, the tribute basket, ritual scrolls, amulets. | Every category has rows; `items.csv` grows from 66 to the full set; each row has culture, producer, material, price band and tag |
| C2 | **An icon and a mesh (or generic mesh) for every item** | No item is invisible in the world or the UI |
| C3 | **Every seller type** in living-world §3.2, placed in the City of the Moon with an owner NPC, hours and finite stock: market stalls, bronzesmith, potter, weaver/dyer, carpenter/wheelwright, bowyer/fletcher, leatherworker, armourer, jeweller/seal-cutter, perfumer/oil merchant, herbalist/physician, temple stores, diviners/exorcists (services), scribes, animal traders, merchant houses, shipwrights/ship-owners, the governor's palace (rank-gated), fences/smugglers | You can walk into each one and trade |
| C4 | **The trade UI**: buy/sell quotes from the kernel (`sim_world_buy_quote`, `sim_world_sell_quote`), barter, the price explained (supply, season, standing, Bargaining, **language**) | Prices differ by standing and skill, and the UI says why |
| C5 | **Silver by weight on diegetic scales**: stone weights; sellers with bad weights (by personality) cheat you; Reckoning catches it; fraud is a crime | You can catch a cheat, report him, and he is tried |
| C6 | **Credit on a tablet**: buy on credit; debt tablets as items; the due date; the creditor comes to collect | Unpaid credit starts a collection, then a lawsuit |
| C7 | **Stock is produced, not spawned**: shops restock only from producers (workshop output, fields, herds, imports). Interrupting production empties the shelf. | Buying the last loaf means the next customer finds none |
| C8 | **Imports arrive physically**: ships at the lighthouse wharf and caravans at the gate (`caravans.csv`) on schedule; market day after an arrival; shelves empty in a raiding season | A ship docks and the market fills with its goods |
| C9 | **Every way to get an item** (living-world §4): buy, barter, craft, grow, herd, forage, hunt, fish, loot, gifts, wages, rewards, inheritance, steal, find (lost, buried, **washed ashore: sea gems**) | Each way has a verb or system that produces items |

---

## Stage D: Crafting and work (rpg-systems §6–7; city-life §1; game-design §4.1)

| # | Step | Done when |
|---|---|---|
| D1 | **Station actors**: quern, bread oven, brewing vat, fire and pot, kiln, forge (bellows, crucible, moulds), loom, dye vat, oil/date press, carpenter's bench, leather frame, mudbrick mould | Each station is usable, owned, and rentable |
| D2 | **Recipe storm to complete chains**: grain → flour → bread and beer; dates → syrup and wine; clay → levigated clay → pot; flax → linen; wool → yarn → cloth → dyed cloth; ore and ingot + tin → bronze → cast item; silt + straw + water + sun → mudbrick; fish → salted/dried; reeds → mats, boats | `recipes.csv` grows from 8 to every chain; each chain has a kernel test |
| D3 | **Smithing minigame**: mould choice, heat, timing; graded quality; flaws (porosity, weak tang); bronze bends and doesn't shatter; straighten, re-sharpen, melt and recast; scrap economy | Cast quality varies with play; a bent sword can be straightened |
| D4 | **Cooking and brewing minigames** (light) | Outcome quality depends on the Cooking skill and timing |
| D5 | **Work for wages** at every work role (`work_roles.csv`, 19 roles): harvest, gleaning, pressing, loading at the wharf, the kiln | Wages are paid in rations or silver; the day's output enters the market |
| D6 | **NPC tasks in visible steps** (city-life §1): the baker fetches water, grinds, kneads, shapes, bakes, sells, cleans; the potter digs, levigates, throws, dries, fires, sorts, sells; each step consumes and produces goods, with an animation | You can follow the baker for a day and watch real bread appear |
| D7 | **Interruptible tasks**: buying the last loaf, blocking the kiln, or hiring the potter away changes the day's output and the market | The ripple shows in prices the next day |
| D8 | **Commissions**: order an item from an artisan, with a price, due date, quality and pickup | A commissioned spear is ready on the day promised |

---

## Stage E: The character (rpg-systems §1.1, §2)

| # | Step | Done when |
|---|---|---|
| E1 | **Character sheet UI**: 6 attributes, 27 skills in 6 groups, XP and level (cap 40), talent points and the talent tree (41), callings at level 5, specialisations at 15, a second calling at 25 — all from existing kernel calls | Everything on the sheet is live and choosable |
| E2 | **Attribute +1 every 4 levels** (the player picks), plus the rare quest or rite bonuses | A choice pops up at levels 4, 8, … |
| E3 | **Skill growth in play by use, teachers and texts**: teachers as NPCs you pay in silver, service or standing (`skill_teachings.csv`); texts you can read | Each of the three paths works for at least one skill in every group |
| E4 | **Social rank in play**: per polity; rises by deeds **plus a patron's act** (a grant tablet ceremony); gates renting (1), buying and courts (2), grants (3), chariots (4), a seal (5), kingship (6); outlawry drops you to 0; a fallen polity's ranks become memories | Rank changes are ceremonies that leave a tablet; gates are enforced |
| E5 | **Languages and scripts (missing in the kernel)**: a new kernel module plus `languages.csv`/`scripts.csv` (Sumerian, Akkadian, the Kaldunai tongue, the eastern tribes' tongue, the Suti tongue; cuneiform, and one undeciphered script as `INVENTED: CONTENT`). Speaking unlocks dialogue options and better prices; reading unlocks texts; overheard speech is rendered per tongue; unreadable text is shown as real-looking script | A shopkeeper's speech is garbled until you learn Akkadian; a tablet's text appears as glyphs until you can read it |
| E6 | **Callings' perks visible and felt**: each of the 18 specialisations has an effect you can see in play | Each perk has a UE test or checklist line |

---

## Stage F: The people (living-world §1–2, §11)

| # | Step | Done when |
|---|---|---|
| F1 | **Real names**: NPCs show names from `people.csv`/`names.csv`, never kernel ids (open wave-6 item) | No id strings appear anywhere in the UI |
| F2 | **The full NPC record**: identity (origin, languages, appearance variant), household (home, spouse, children, parents, servants, family tomb), livelihood (calling, workplace, skills, income, debts, deeds), rank, faction, oaths, **faith** (patron god, household gods, taboos, festivals attended), needs (food, water, sleep, safety, money, status, faith, family) | A debug inspector shows a complete record for every NPC |
| F3 | **Goals** (missing): short-term goals (sell the harvest, repay the lender) and long-term ones (marry off a daughter, earn rank, flee the raiders). NPCs act on goals: a debtor borrows again, steals, runs to the hills, sells into debt service, or asks the player | An NPC's debt spiral plays out without the player |
| F4 | **Memory and relationships** (missing): each memory has a source, date, place and certainty; opinion of the player and of other NPCs (friendship, rivalry, love, grudges, debts owed) | NPCs greet you differently after what they saw or heard |
| F5 | **Rumour at walking speed** along social links (the kernel has it; it needs wiring and UI): overheard gossip lines, and the rumours you hear in the journal | A crime you commit at the gate reaches the wharf hours later, not instantly |
| F6 | **Simulation layers in UE**: L0 full AI within ~150 m, L1 scheduled (NPCs appear where their schedule puts them), L2 statistical; promotion of light residents to full NPCs when you get involved | Walking across the city never shows pop-in contradictions |
| F7 | **Population to target** (DQ3): the `people.csv` storm to ~160 named people across the nine quarters, with a household for every building; `person_schedules.csv` from 4 rows to one per full NPC | Every simulated house has occupants who live there |
| F8 | **Crowds** (Mass) for market days and festivals, at historical density | The market at noon on market day is packed and stays at 60 fps |
| F9 | **NPC animation and presence**: task animations, sitting, eating, sleeping, conversations between NPCs, greetings by rank (bows), barks | You can watch a street of people doing real things |
| F10 | **Life events**: weddings (bride-price, contract), births, funerals, divorce and inheritance quarrels, a missing child, aging; the world responds to you (the smith you supplied opens a second forge; a village names a child after you) | Over a season, the street visibly lives through events |

---

## Stage G: Reaction, reputation and custom (city-life §2, §4)

| # | Step | Done when |
|---|---|---|
| G1 | **Perception**: sight lines, light (night hides, lamps reveal), sound (a scream carries, a whisper doesn't), smell (smoke, a corpse) | Theft in a dark alley goes unseen; the same theft under a lamp is seen |
| G2 | **Judgement** against three rule sets: law, custom, and personal values (piety, greed, loyalty, fear), which differ by community | A barbarian foreman and a temple steward judge the same act differently |
| G3 | **Feeling and the response ladder 0–7** (ignore, remark, confront, refuse, alarm, flee/hide, intervene, attack), chosen by role, courage and position: guards 4–6, kin 6–7, priests hardest on sacrilege, merchants on theft, beggars and outcasts rarely call the watch | Each rung appears in play with its own barks and animation |
| G4 | **Three layers of reputation**: personal (memory), community (neighbourhood, guild, temple, foreign community: the barbarian quarter, Akkadian officials, southern rebels), legal status (clean, suspected, accused, wanted, outlawed) per jurisdiction | A reputation screen shows all three layers |
| G5 | **What reputation changes**: greetings, prices, credit, doors, marriage offers, which guards look away, and which rumours follow you | Each effect is measurable in a test |
| G6 | **The custom storm**: every domain × every community in the city: greeting and rank, hospitality, purity, weapons in the city, dress, food and drink (first portion to the gods), private rooms, the dead, oaths, mourning, gifts | `customs.csv` grows from 15 rows to full coverage; each custom has an in-world trigger and a reaction |
| G7 | **Dress as signal**: clothing reads rank and origin; wrong dress draws stares and suspicion; guards read armour before your name | Wearing imperial armour into the rebel quarter changes reactions |
| G8 | **Gift economy**: a gift creates a debt of honour; refusing one insults the giver; an unreturned gift damages the relationship | Gifts move opinion both ways |

---

## Stage H: Law and justice (city-life §3; living-world §7)

| # | Step | Done when |
|---|---|---|
| H1 | **Enforcers**: gatekeepers (open and close the gates on the clock, question strangers, collect tolls), the watch (patrol graphs by day and night), palace men, district elders (collective liability), the victim's family, temple personnel | Each enforcer type acts in the running game |
| H2 | **Guard AI**: alarm propagation by radius and priority; chases with the guards' own stamina; search where you were last seen; question witnesses; carry a **description** (clothes, weapon, build) that you can partly shake off by changing clothes; bribery by personality (itself a crime if witnessed) | A chase can end in capture, escape or a bribe |
| H3 | **The complete law table** (review D-013's severity with the designer): theft, burglary (killing a burglar at night), assault by injury tariff, killing in a quarrel (compensation or blood), murder, killing a foreign merchant (the town pays if no killer is found), sorcery, sacrilege, tomb robbery, fraud (false weights, forged seals), harbouring a fugitive, evading ilku, oath-breaking, and embargo-breaking (this world's analogue: trading with the enemy inside imperial jurisdiction) | Every crime has a row, a penalty ladder, a trigger in play, and a test |
| H4 | **Evidence persists**: stolen items are recognised, seal impressions are traced, a fence talks, blood stays visible, bodies are found | A crime with no witness can still be solved later |
| H5 | **Arrest choices**: surrender, run (become wanted), fight (a new crime), or talk your way out (skill and reputation) | All four branches are playable |
| H6 | **Detention**: the holding room at the gate or palace; possessions held as evidence; time passes; companions act for you (gather witnesses, pay compensation, break you out) | You wait in a real room, and your companions' help changes the outcome |
| H7 | **The hearing scene**: the governor's official or the elders; evidence, witnesses, **oaths before the gods** (a false oath brings the oath-breaker curse) | A hearing UI with testimony and your choices |
| H8 | **The sealed verdict tablet** as an item; legal status updates | Verdict tablets pile up in the archive and your inventory |
| H9 | **Sentences, each playable**: compensation, confiscation, debt service (a work loop), corvée labour, flogging, exile, death (on Historical this is final; otherwise the heir flow) | Each sentence has its own consequence scene |
| H10 | **Escape**: guards on schedules, locks, bribes, friends outside | Escape is a playable sequence, and a crime |
| H11 | **Outlawry**: lose the city's protection, rank 0, the hills take you in (bands in `wild_groups.csv`) | An outlaw can live with a bandit band |
| H12 | **Night rules**: curfew-like quiet, strangers questioned at night, knocking on a shop at night is rude (urgent needs pay extra) | Night plays differently from day |

---

## Stage I: Death, burial and the dead (city-life §6; missing in the kernel)

| # | Step | Done when |
|---|---|---|
| I1 | **Kernel module `Death`**: bodies as world objects; the finder reacts by role (alarm, run, rob); the family is told; the investigation opens for a killing | A kernel scenario: a death, then discovery, then mourning, then burial, then investigation |
| I2 | **Mourning in view**: wailing, torn clothes, dust on the head; mourners at the house | Mourning is visible for the right number of days |
| I3 | **Burial**: the body prepared; a **procession** to the family tomb under the house; simpler graves for the poor; grave goods (pottery, food, personal items) | A procession walks the street to the tomb |
| I4 | **After the burial**: offerings over the following days and at set times; the ancestor rite (the marzeah-analogue for this setting) | Neglect is tracked per tomb |
| I5 | **The restless dead**: neglected or unburied dead become a haunting (Mythic: an affliction that rites cure; Chronicle: bad dreams and bad luck) | Leaving a body unburied leads to a haunting |
| I6 | **Your dead**: companions and family you bury (or don't, and the neighbours notice); your own funeral when your line passes to the heir, where who comes and what they say reflects your reputation | Your funeral is attended according to your reputation |
| I7 | **Tomb robbery**: a crime, a feud and divine anger | Robbing a tomb triggers all three |

---

## Stage J: The magic of the gods (rpg-systems §0, §10; game-design §8)

| # | Step | Done when |
|---|---|---|
| J1 | **Rite flow in UE**: prepare (gather, purify, choose the time) → perform (a short sequence minigame: the right actions in order and the right words in the right **language**) → outcome from the five powers (existing kernel) → the effect, side effects, and the gods' judgement | A rite performed at the temple of sun and moon works and shows why |
| J2 | **The rite storm**: from 5 rites to full coverage of the **eight effect families** (protection, healing/purification, divination, blessing, curse/binding, substitution, ancestors, divine intervention) across this setting's traditions: the two-waters worship, hymns (vibrating the names), zisurru exorcism (SAG.BA.SAG.BA), Barûtu, šu-ila, amulets and talismans, demonology (evil, neutral and benevolent demons; medicine as exorcism) | `rites.csv` covers every family; every rite has materials, place, time, language and effect |
| J3 | **Divination in play**: all 9 forms (haruspicy/Barûtu, astragalomancy, cleromancy, pessomancy, aeromancy and its five sub-types, astrology with 12 zodiac signs plus a few extra constellations), answers as **omens with probabilities, never certainties**, clarity scaled by skill | A liver reading before a journey gives a probabilistic warning that matters |
| J4 | **The deity network**: per-god favour meters (UI), domain, cult sites, festivals and **taboos** (missing); **equated gods** share part of their favour (Inanna/Ishtar, Enki/Ea/Mercury, Nanna/Sîn, Utu/Shamash) (missing); **neglected gods get angry** (missing: misfortune events); **choosing a patron god** with a unique high rite and taboos to keep | Neglecting a god produces misfortune; a patron's taboo matters |
| J5 | **Guard rails enforced**: rites take hours to days and cost real goods; no combat spells (wards and blessings prepared beforehand, amulets); **magic cannot change the historical clock** | A test asserts no rite alters the clock |
| J6 | **The notes' full magick (DQ4, D-025)**, each as a working system with a table, rites and tests: numerology (basic and advanced: the gods' numbers, name numbers modifying rites); the primitive kabbalah and hermeticism ("as above, so below": sky omens mirrored in earthly events, correspondences between planets, gods, metals, days and directions); astrology (12 primitive zodiac signs plus extra constellations, birth signs, auspicious hours); astrotheology (Sun, Moon, Saturn, Ishtar/Venus, Mercury-Enki as rite powers); the Earth list (wealth, crop-yield and nature rites on fields and herds); the Moon list (dream reading and incubation, fortune telling, psychological magic on NPC moods, **animal telepathy** with herds and mounts, **telepathy** reading an NPC's intent, **psychometry** reading an object's history and owner (crime evidence!), **astral travel** as a trance that scouts a distant place on the region map, **mind control** that works but carries divine wrath and is sorcery); the Mercury list (high ceremonial rituals, invocations of a god, **pathworking** as guided visions that teach rites or reveal lore); the Venus list (love, allure, fertility, unions and alliances, the binding of hearts as bad karma, the morning-star and evening-star rites; sex magick off-screen) | Every form named in notes L188–L207 is playable and has a codex page |
| J7 | **Chronicle mode**: the same rites and costs, but effects become statistical and deniable | The toggle changes outcomes and never costs |
| J8 | **Sorcery is a crime**: curse rites seen by a witness go to justice (H3) | Casting a curse in view leads to a trial |
| J9 | **Divine wrath and cataclysm in play** (the kernel has it): omens, the curse tiers, atonement, shown in the world (a failed harvest, lightning striking a temple, a plague) | Wrath has visible world consequences |
| J10 | **Divine intervention** as rare world events at high favour plus a crisis (the system exists here; the Prophet's summoning later uses it) | One intervention event runs end to end |

---

## Stage K: Combat (rpg-systems §7–8; game-design §4.2)

| # | Step | Done when |
|---|---|---|
| K1 | **Melee in UE on the kernel resolver**: directional attacks, blocking with the shield, stance, stamina, catching your breath; animation-driven, brief and brutal (no health-bar slugging) | A fight with a bandit lasts seconds and hurts |
| K2 | **Ranged**: bow (simple and composite), sling, javelin, throwing stick; arrows with bronze, flint or bone heads; aim sway from Agility and intoxication | Ranged weapons use their stats |
| K3 | **Zonal wounds in play**: head, torso, arms, legs; bleeding, fractures, festering, scars, lasting limps; treatment (physician craft, rites) | A leg wound slows you until it is treated |
| K4 | **Equipment**: 4 zones × 2 layers of armour, cultural shields and helmets (`combat_styles.csv`); weight to stamina; heat to fatigue; maintenance (lacing breaks, leather rots, padding goes foul); weapon wear, repair and recasting at the smith | The equipment screen and its effects are fully live |
| K5 | **Combat social layer**: duels (agree to a duel), surrender, taking prisoners, ransom, looting (legal or not), combat as a crime (the kernel's combat_crime) | Each is reachable from a fight |
| K6 | **NPC combat AI**: guards, bandits, warbands, the Suti, wild animals (lions, leopards, wolves, bears; they attack herds and hunt at night) | Each enemy type fights by its own style |
| K7 | **Band battles**: your band (5–30) and enemy bands as abstract units with orders (hold, advance, flank, withdraw); morale, discipline, pay and loot shares; crowd-scale soldiers via Mass | A raid on the city can be defended with your band |
| K8 | **Raids happen physically**: when the kernel's raid formula fires against the city or your property, raiders arrive, fight, loot and leave | A raid is something you see and can stop |
| K9 | **Chariots** as drivable vehicles (2 horses, a groom, repairs, terrain limits; rank 4 or palace issue) | You can drive a chariot outside the walls |

---

## Stage L: Followers and household (living-world §5; missing in the kernel)

| # | Step | Done when |
|---|---|---|
| L1 | **Kernel module `Followers`**: companions (cap 3), hirelings, the band (5–30), household, animals; **loyalty 0–100** from pay, food, treatment, keeping your word, shared danger, compatible faith, rank and reputation; low loyalty leads to complaints, theft, desertion and **betrayal** | A scenario: unpaid hirelings desert; a mistreated companion betrays you |
| L2 | **Companion roster** (`INVENTED`, fitted to this setting): 6–8 companions, for example a fellow Kaldunai prisoner, a scribe's daughter of the lighthouse school who reads several scripts, a Suti herdsman who knows every trail, a barbarian foreman tired of the quay, a Retributor of Utu, a debtor priestess, a sea-captain stranded by drought. Each has a backstory, skills, gods, opinions and a **personal quest line** (side content, not the main quest). | Each companion is recruitable and has a finished personal quest line |
| L3 | **Companion lifecycle**: they level up, can marry, can leave you, and can die permanently | All four outcomes are reachable |
| L4 | **Hirelings**: guards, porters, guides, sailors; paid in silver or rations; morale from pay, food and danger | Hiring UI; unpaid hirelings desert |
| L5 | **The band**: recruit, pay, loot shares, morale, discipline (needs the Mercenary calling or rank 3+) | Wired to K7 |
| L6 | **Household**: spouse, children (future heirs), servants, a **steward**; they live in your house and work your property | Your house has people in it |
| L7 | **Marriage and children**: courtship, bride-price, a marriage contract tablet, alliances by marriage; children grow up, go to school, apprenticeships or temple service; traits show as they grow | A child you raise becomes an eligible heir |
| L8 | **Animals** (animal care is `OPEN` in the kernel): dog (warns of thieves), donkeys (carry goods; 65–75 kg loads), oxen, horses; hunger, thirst, lameness, loyalty; breeding; theft | Losing a donkey on a desert leg is a crisis |
| L9 | **Unfree labour policy** (rpg-systems §5.4): servants can be bought or inherited and **freed** (manumission deeds); freed servants become settlers; never a reward | The manumission flow exists and is sealed on a tablet |

---

## Stage M: Property and business (rpg-systems §5; D-017)

| # | Step | Done when |
|---|---|---|
| M1 | **Every asset type** in play: house (rent at rank 1, buy at rank 2, build, grant) with storage, beds, a family shrine, workshop space and a family tomb; fields (barley, wheat, flax); orchards (date palms, the region's fruit); herds (sheep, goats, cattle, donkeys); workshops; **ship shares** (the lighthouse wharf); **caravan partnerships**; **loans** (20% on silver, 33⅓% on barley) | Each asset can be acquired and produces what it should |
| M2 | **Deeds as tablets**: witnessed and sealed; losing one (fire, theft) makes the claim contestable, which becomes quest content | A lost deed becomes a dispute |
| M3 | **Obligations**: ilku service on granted land, taxes and dues, corvée; failure means the land is taken back | Ignoring an ilku call costs you the land |
| M4 | **The management loop**: staff (rations as wages: barley, oil, wool), supply inputs, time by the calendar (sowing, harvest, date harvest, the sailing season), store (grain in jars and silos with pest and damp losses), sell or feed people | A field, worked well, feeds your household and sells the rest |
| M5 | **Steward reports as tablets**, with errors and dishonesty scaled by the steward's loyalty and ability | A dishonest steward skims, and the tablets show it if you read carefully |
| M6 | **Property UI**: a ledger of assets, deeds, debts, income (the purse) | One screen shows your whole estate |
| M7 | **Voyage and caravan risk**: storms, pirates, raids, the drought affect ship-share and caravan returns | A ship lost at sea wipes out a share |
| M8 | **Debt collection**: by law (a lawsuit at the hearing) or by force (a crime) | Both paths are playable |

---

## Stage N: Build and settle — the player's own city-state (notes L9; game-design §4.4; endings §2 as a system)

| # | Step | Done when |
|---|---|---|
| N1 | **Construction**: kit-based placement with material realism (mudbrick needs silt, straw and water; cedar beams only where cedar is traded; stone footings, lime plaster) | You can build with the house kit |
| N2 | **The settlement ladder**: bedroll → hide camp → mudbrick farmstead → walled village → river/sea trade post | Each tier is reachable and buildable |
| N3 | **Settlement simulation**: population, food, morale, defence; refugee intake policies (the drought sends them); settlers from freed servants and followers | A settlement grows or starves by your choices |
| N4 | **Kingship as a system** (the ending wraps it later): the five requirements (people, food security, walls and a defence force, **a temple and a patron god with the founding rite**, recognition by treaty or open independence) plus a royal seal; rank 6 | Founding a polity is possible in open play (D-021: no story gates) |
| N5 | **Ruling**: your own law table, land grants, taxes, hearing cases, marriages, treaties with factions | A king's week of decisions is playable |

---

## Stage O: Calendar, festivals and daily life (city-life §1, §5; living-world §11)

| # | Step | Done when |
|---|---|---|
| O1 | **The calendar's effects** (on top of A14): lucky and unlucky days change rite outcomes, trade and event odds; the moon rites draw crowds to the ziggurat | A rite on an unlucky day is measurably worse |
| O2 | **The city's clock, in full** (city-life §1 table): before dawn (bakers, querns), dawn (shrine offerings, animals watered, **gates open**), morning (peak market, petitions at the governor's seat), midday (summer rest, shops shut), afternoon (the port unloads), evening (main meal, visits, stories, **shops close**, gates close at dusk), night (lamps, feasts, lovers, thieves, the watch, dogs) | Following the clock hour by hour matches the table |
| O3 | **Seasonal bend**: summer long rests, rooftop sleeping, dust; winter rain, early dark, indoor fires; the sailing season | The same street differs by season |
| O4 | **The festival storm**: **the moon rites of every eššešu day (1, 7, 15) and šapattu fire at the Moon Pool with an offering and a crowd (moved from A14)**; from 4 rows to the full ritual year of the City of the Moon: new moon and full moon offerings to Nanna, the king's (governor's) purification days, the new year, harvest and threshing, the date harvest, first fruits, the Feeding of the Dead, ship-arrival and caravan days, the drinking fellowship | `festivals.csv` covers every month |
| O5 | **Festival mechanics**: the crowd switches to procession paths; shops keep festival hours; pickpockets and extra patrols; favour gains for taking part; matchmaking and deal-making; **ritual roles for the player** by rank and calling (carry offerings, sing, sacrifice, guard the procession) | A festival day plays like no other day |
| O6 | **Weather in UE** (`weather.csv`): rain, dust storms, haze, heat waves, storms at sea, drought stages visible on the fields and canals (building on Tommy's withering) | Weather changes visuals, heat/cold, travel time and events |
| O7 | **Night is real**: darkness, lamps and torches (carriable), moonlight by phase | You need a lamp in a moonless alley |

---

## Stage P: Events, quests (not the main quest), dialogue, time (city-life §7; living-world §8–10)

| # | Step | Done when |
|---|---|---|
| P1 | **The event storm**: from 16 rows to the full catalogue across 9 categories (crime, family life, economy, nature, illness, omens, politics, war and raids, the gods), each with participants (real NPCs), locations, **persistent consequences** and **chains** (fire in the potters' quarter → homeless families → shelter or theft → a lawsuit over the burnt kiln) | ~120 events; every category is represented |
| P2 | **Event presentation in UE**: fire and smoke, crowds gathering, an overdue ship, a brawl, a funeral, an eclipse; the player can join, ignore, help, exploit or stop each one | Each category has its visual and audio staging |
| P3 | **Frequency tuning**: 1–3 visible events per day near the player; major ones spaced out | A week-long soak test logs the distribution |
| P4 | **Dialogue UI** (on A7): conditions from language, rank, reputation, skill checks and memory; subtitles; barks; ancient-language exclamations | Every full NPC can be talked to |
| P5 | **The dialogue storm**: every full NPC gets greetings by reputation, work talk, rumour lines, trade lines and refusals | No NPC answers with silence or an id |
| P6 | **Side quests** (all non-story sources): sourced side quests, **systemic quests** generated from NPC needs and the world state (a runaway donkey, a caravan escort, a debt to collect, a sick child, a poisoned well), faction contracts, companion lines (L2), emergent quests from your actions | The generator produces varied quests every week |
| P7 | **Quest delivery channels**: conversation, **letters sent to your house** (you must read them or hire a scribe), rumour, the crier, temple oracles and omens | Each channel delivers at least one quest type |
| P8 | **No markers by default**: directions come in words, you mark your own map; the optional **guided mode** adds markers | Both modes work |
| P9 | **Quests fail and time out**, and the world tells you what happened | A missed deadline produces a consequence and a notice |
| P10 | **The journal**: quests, rumours, learned facts, the people you know | One place holds everything you've learned |
| P11 | **Time-skip machinery** (the story triggers it later): the simulation runs through the skip; property earns or fails; children grow; companions age, marry, leave or die; an interlude report on tablets; interlude decisions | A debug skip of 7 years produces a coherent world and a report |
| P12 | **Heirs** (D-003): choose an heir (adult child, adopted, spouse by will); property by will (or custom and disputes without one); debts, oaths, partial rank and family reputation pass on; skills don't (a "family trade" bonus); followers stay if loyal to the house; with no heir, the line ends in the epitaph and a new character in the changed world | Dying with an heir continues the game as the heir |
| P13 | **The deed recap / epitaph generator**, from a log of what the player actually did | The epitaph after a line ends reflects real deeds |
| P14 | **Aging** for every character | A 7-year skip ages everyone |

---

## Stage Q: Factions, the wild lands and the wider world (rpg-systems §4, §9; living-world §6; world-map §4)

| # | Step | Done when |
|---|---|---|
| Q1 | **Faction UI**: standing 0–100 and its tiers (Stranger → Oath-bound) for the four powers and the minor factions; oaths sworn before the gods; the oath-breaker curse; **one Great King at a time**; mercenary contracts from anyone at "Known" | Swearing, keeping and breaking oaths are all playable |
| Q2 | **Minor factions, each with needs, territory, leaders and standing**, and each one you can **fight, negotiate with, trade with, hire, join or destroy**: the cults of the cities (the temple of sun and moon here; the other cults through their people), the Retributors of Utu, the Suti, the eastern tribes' warband, the rebel partisans, bandits, smugglers, the thieves' and fences' network of the wharf, deserters | Each verb works against each group, where it makes sense |
| Q3 | **Faction decay on the clock**: dead factions void oaths and leave followers orphaned, with new quests | Kernel scenario plus UE notice |
| Q4 | **The wild lands, walkable**: the region's 15 wild places and 26 links from the kernel, built physically around Tommy's terrain (camps, caves, coves, hills, ruins, shrines, wells, pastures), with World Partition streaming | You can walk from the gate to a bandit camp |
| Q5 | **Wild encounters and camps in play** (`wild_encounters.csv`): scout, infiltrate, attack, pay off, ally with or join a camp; rob or escort caravans; search sites | Every wild C API action has a UE entry point |
| Q6 | **Travel**: walking by default; fast travel as an off-by-default setting that uses the kernel's travel time (`sim_world_travel`); mounts and carts (L8/K9); reed boats on the river and the sea coast | Travel costs time by mode, terrain and weather |
| Q7 | **Off-map ventures** as a UI and event-card layer: caravans, voyages from the lighthouse wharf, pilgrimages, war contracts, and the three journeys to the Cities of the Abyss, the Sun and the Warrior Spirit; the world keeps ticking while you're away | A voyage runs as a card sequence and you return to a changed city |
| Q8 | **The other cities through people**: envoys, merchants, pilgrims and refugees from all 8 cities in the City of the Moon, carrying their customs, goods, gods and rumours (the skull-street dead cult's scrolls, Retributors from the Sun, and so on) | Each of the 8 cities has at least one resident presence and trade goods |
| Q9 | **Ruins of older layers**: Ur is ancient; lost objects and forgotten tablets in the tell | Ruins exist and reward exploration |

---

## Stage R: Audio (none exists; city-life §8; game-design §13.5)

| # | Step | Done when |
|---|---|---|
| R1 | Audio pipeline: CC0 sources and a licence ledger (as in `invented-ledger-humans.md`), MetaSounds, submixes, attenuation, interior reverb | The pipeline doc exists and one sound plays end to end |
| R2 | **The city's soundscape by hour**: querns at dawn, smiths hammering, gulls and sailors at the wharf, prayers, dogs, the watch calling at night, market noise by crowd size | The soundscape changes with the clock |
| R3 | **SFX for every verb and system**: footsteps per surface, doors, eating, drinking, crafting stations, combat, weather, fire, the lighthouse | No silent actions |
| R4 | **Music**: ambient by time of day and season, festival music, tension and combat | Music follows state |
| R5 | **NPC barks** in short ancient-language exclamations, subtitled | Every response-ladder rung has barks |
| R6 | **Voice hooks** (DQ5): every dialogue line and bark has a voice-asset slot and a subtitle; the player-voice on/off setting (notes L1); Tommy's recordings drop in later without code changes | A placeholder line plays through the hook, and the toggle silences it |

---

## Stage S: The world made whole (living-world §4; D-023)

| # | Step | Done when |
|---|---|---|
| S1 | **Every place usable**: every simulated building has a door that opens, an owner, and something to do (houses: visit by invitation, trade, rent, buy, rob, sleep as a guest, marry in; workshops; fields and presses; the temple of sun and moon: offer, pray, rites, ritual goods, festivals, purification, vows, temple service; the governor's seat: petitions, court, rank, grants, the archives, dues, ilku calls; the lighthouse scholars' school: scripts, scribes, tablets; the wharf: loading work, selling to ships, imports, ventures, sailors, foreigners; family tombs; wells and springs as gossip spots) | A place-by-place checklist is 100% green |
| S2 | **Interiors** for every building kind, built with the kit in the D-023 style | Every enterable door leads somewhere |
| S3 | **Every building of the crescent finished** (DQ3, Stage AA): all ~110 buildings have an interior, an owner household and something to do | The place-by-place checklist is 100% green |
| S4 | **The ziggurat and the Great Lighthouse** as usable places, not only scenery | Both host rites, work, and events |
| S5 | **Clothing set v1** (plan.md §7 art risk): rank and origin clothing (Sumerian, imperial Akkadian, barbarian, Suti, Kaldunai prisoner), since dress drives reactions | Each community reads visually |
| S6 | **Weapons, armour, shields and helmets** as meshes for all 20 arms and the cultural styles | Every equipped item is visible |
| S7 | **Animals**: donkeys, oxen, horses, sheep, goats, dogs, lions, leopards, wolves, bears, fish, birds | The fields and hills have animals |
| S8 | **Animations**: every task step, combat, emotes (bowing by rank, mourning gestures, prayer), sitting and sleeping | No NPC slides or T-poses |
| S9 | **VFX**: fire, smoke, dust, blood (dismemberment-free), water, subtle rite effects (no light shows), omens (eclipse, comets, lightning) | Each system's effects are visible |
| S10 | **Smells as signals**: subtle text or visual cues for smoke, a corpse, the dye works, the bakeries | Cues appear when relevant |
| S11 | **Signs are script**: readable only if you know the script, otherwise glyphs; shops recognised by goods and noise | No English signs in the world |

---

## Stage T: The diegetic UI (game-design §8; architecture §4.8)

| # | Step | Done when |
|---|---|---|
| T1 | **The Scribe's Case (codex)**: fills only with what you have learned; shows each fact's tag (CANON / A / INVENTED); an optional sources toggle; generated from the same tables (`codex_gen.py` → a UE data asset) | Learning a rite adds its codex page |
| T2 | **The map you draw**: explored areas fill in; you place your own marks; a "royal map" bought from a scribe is a real purchase | The map reflects only what you've seen or bought |
| T3 | **The tablet reader** for letters, deeds, debt notes, verdicts, steward reports, rituals, all as items | Every document item opens in the reader |
| T4 | **The scales**, weighing silver (C5) | Trading uses the scales |
| T5 | **Accessibility**: subtitle-first, colour-blind palettes, text size, full remap (A9), the needs-severity and disease toggles (A11) | Checklist complete |

---

## Stage U: Ship quality (architecture §5; plan.md §9)

| # | Step | Done when |
|---|---|---|
| U1 | **Performance budget**: 60 fps at 1080p medium in the market at noon on the RX 5700 XT (after the LTS-kernel fix) and on Tommy's RTX 3060; 40–60 L0 NPCs; LODs/HLODs; Mass crowds; profiling with Unreal Insights | Measured and recorded in the repo |
| U2 | **The Linux GPU**: test an LTS kernel on the 5700 XT (needs the designer's sudo); offline sky cubemap for Linux ambient (DECISIONS standing rule) | The discrete GPU runs the game, or the reason is documented |
| U3 | **Save versioning and migration**: old saves load after content changes | A save from the previous version loads |
| U4 | **Soak tests**: a 30-day headless run and a 3-day in-engine run without errors; the "follow one NPC for a whole day" bot | Both logs are clean |
| U5 | **Localisation audit**: no hardcoded strings (plan.md rule); every text in string tables | A script finds zero literal strings in UI code |
| U6 | **Packaging**: shipping builds for Windows and Linux; cooked canon; crash reporting; a version stamp; the Early Access store build (Act I is main-quest content, so this waits for it; the build itself is ready) | A packaged build runs on a clean machine |
| U7 | **The final completeness audit**: each row of mechanics.md §1 (38 systems), each section of the four parent blueprints, and each notes element in game-design §12 is marked playable, with where and how it was tested | The audit table has no gaps |

---

## 3. Order and waves

Dependencies decide the order. Inside a stage, steps without shared files run in parallel (one agent per step or per table). Each stage ends with the merge → bug review → fix → push cycle and a **designer play session**.

```
A Foundation ──► AA City rebuilt (+ V0–V3) ──► B Body ──► C Trade ──► D Craft ──► E Character
                                  │
                                  ├──► F People ──► G Reaction ──► H Justice ──► I Death
                                  │                     │
                                  │                     └──► O Calendar/Festivals ──► P Events/Quests/Time
                                  ├──► J Magic (after B4 purity, E5 languages)
                                  ├──► K Combat ──► L Followers ──► M Property ──► N Build & Settle
                                  └──► Q Factions & Wild (after K, L8)
R Audio, S World, T Diegetic UI: run alongside from stage C on, one wave each per stage
U Ship quality: continuous budgets from stage F; final pass last
```

**Content storms run alongside their stage** (items with C, recipes with D, people and schedules with F, customs with G, laws with H, rites with J, festivals with O, events and dialogue with P). Each storm is gated by canon_lint, a tone review, and a ledger entry.

## 4. After this plan: the main quest (parked)

It becomes content on finished systems: the story beats in `story.csv`, the act transitions (using P11 skips), the story quest line, the four ending unlocks and sequences (using N4 kingship, J10 intervention, Q3 faction decay and P13 recap), the Brotherhood's silent sequence, the war set pieces, and the cliffhanger. It will get its own plan when the designer says go.
