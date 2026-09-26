# MECHANICS IMPLEMENTATION PLAN — every mechanic mapped, specified and tested

**Status:** v2 (2026-09-26). Planning only: no game code has changed yet.
**The complete list is the registry:** [mechanics-registry.md](mechanics-registry.md) holds **every mechanic in the game, 570 rows in 26 areas**, each with its rule, its source (the notes, the imported blueprints, a decision, a ledger or a canon table), its kernel/C API/Unreal status and its phase. [mechanics-instances.md](mechanics-instances.md) holds the **894 canon rows that must each work in play**: every law, custom, event, festival, talent, calling, rite, magick form, divination form, deity, faction, band, wild place, encounter, caravan, item, recipe, building type, place, person, schedule, quest and dialogue. `tools/mechanics_registry.py --check` proves the list has no holes: **every one of the 205 C API calls is cited by a mechanic**, every canon table is either instanced or declared lore, and each mechanic is marked tested only when a test tagged `MECH:<id>` exists. This document is how that list gets built; §2 details the first ten areas in depth, and the registry carries the rest.
**Why this exists:** [completion-plan.md](completion-plan.md) is the *schedule* (stages A–U). This document is the *mechanic-level spec*: every player-facing mechanic down to the smallest one, how it must behave, what already exists in the kernel, the C API and Unreal, what is missing, and the test that proves it is finished. Where a mechanic is already fully specified by a completion-plan step, this plan points at it instead of repeating it.
**Scope order (the designer, 2026-09-26):** inventory, dropping and picking up items, menus, buying, selling, activities, eating, crafting, raiders and their menu, followers and their menu first. Then everything else.
**Runs alongside:** the city/asset session (Stage AA/V). §8 lists the files each side owns, so the two sessions don't collide.

Mechanic ids used below: `FND` foundations · `INV` inventory · `WLD` world items (drop/pickup/containers/loot) · `EAT` eating and drinking · `TRD` buying, selling, services · `CRF` crafting · `ACT` activities · `FOL` followers · `RAD` raiders · `UI` menus · everything else in §3.

---

## 0. Ground truth (measured on `main` @ `11b37ed`, 2026-09-26)

| Mechanic area | Kernel | C API | Unreal | Verdict |
|---|---|---|---|---|
| **Inventory** | `Inventory{ map<item,int> }`, counts only (`Crafting.hpp`). No weight, owner, stolen flag, quality, condition, freshness or containers | `sim_world_inventory` (a `"item:n;…"` string), `item_count`, `give_item` | Tab (held) prints the string as debug text on the HUD | A bag of numbers. No inventory screen. |
| **Pick up** | — | `give_item` | `ASimPickup`: gives the item, then `Destroy()`. World items are not kernel state, so placeholder goods respawn on New Game but not on Load | Works for one hard-coded item per actor. Not persistent, no ownership. |
| **Drop** | — | — | — | **Missing everywhere.** |
| **Buy / sell** | Prices and per-city stock (`Economy.hpp`), purse per owner (`Property.hpp`), and buying and selling against the **city market as a whole** with bargaining, talents and rank in the price (`Progression.hpp` `quote_buy/quote_sell`) | `price`, `purse`, `buy`, `sell`, `buy_quote`, `sell_quote` | — | The kernel trades with an abstract city market. There are **no shops**: no owner, hours, shop stock, seller's purse, barter or price breakdown, and nothing in Unreal. |
| **Eating / drinking** | `eat/drink(item)`, restore values by category (`Needs.hpp`) | `eat`, `drink`, `best_food`, `best_drink` | F eats the "best" food, G drinks the best drink, the well gives water | Works, but the player can't choose what to eat. No time cost, spoilage, diet or satiety cap. |
| **Crafting** | `craft()`: all or nothing, needs stations, 8 recipes | `sim_world_craft` | — (not reached) | Kernel only. Stations exist as *items* (6 rows with category `station`), not as places. |
| **Activities** | `work`, `practice/train/study_skill`, `rest`, rites, divination, `treat_wounds`, `repair/recast` | All present | Bed (sleep), well (drink), tablet (read) | Most activities can't be reached from the game. |
| **Raiders** | Deep: 5 bands, camps, the daily raid formula, faction politics, captives, rescue quests, and 15 player verbs (scout, attack, infiltrate, pay off, ally, join, leave, challenge, found band, lead raid, ransom, escort, abandon escort, rob caravan, search site) | All in `CApiWild.h` | — | A complete simulation that only exists as numbers. **No player can see or reach it.** |
| **Followers** | **No module.** The wild verbs take `companions` as a bare integer | — | — | **Missing everywhere.** |
| **Menus** | — | — | A Slate screen stack: MainMenu, Pause, SaveLoad, Settings, Journal, Tablet, Loading | The shell exists. No gameplay screens exist. |
| **Item data** | `items.csv`: 66 rows, 34 free-text categories (`staple`, `staple food`, `staple ingredient`…), no weight or stack size | — | — | Must be normalised before any UI can filter it. |

**The honest summary:** the kernel's *world* is deep, but the *player's hands* barely exist. Before any single mechanic can be perfected, the item model, world items, the verb/result protocol and the UI kit have to be rebuilt (§1). Everything else sits on those.

---

## 1. Foundations (Phase 0): what every later mechanic sits on

Nothing in §2 starts until these are green. Each one is small, but every mechanic depends on them.

| Id | Foundation | Specification | Done when |
|---|---|---|---|
| **FND-01** | **Item model v2** (kernel, new `sim/Items.hpp`) | Replace `Inventory{map<Id,int>}` with stacks: `ItemStack{ item, qty, quality_tier (0 poor · 1 common · 2 fine · 3 masterwork), condition 0–100, owner ("" / npc id / "player" / faction id), stolen (bool), made_day, bound (quest item) }`. Stacks merge only when every field except `qty` matches, and `made_day` merges by bucket (the same spoilage stage), so stacks don't fragment. **The old count API stays as a view** (`item_count` sums the stacks; `give_item(+n)` makes a common, player-owned, fresh stack), so no current caller breaks. | All 40+ kernel tests still pass untouched; new tests cover merge, split, owner and stolen round-trips |
| **FND-02** | **Item catalogue columns** | `items.csv` gains `ui_category` (a fixed enum: food, drink, ingredient, material, tool, weapon, armour, shield, clothing, ritual, document, trade_good, station_part, animal, quest), `weight_g`, `stack_max`, `spoil_days` (0 = never), `icon`, `mesh`, `flags` (`bound`, `document`, `container:<capacity_g>`, `floats`). `canon_lint` rejects a row with no weight or category | Every one of the 66 rows is filled; lint passes |
| **FND-03** | **Carry capacity and encumbrance** | `capacity_g = 30 000 + 3 000 × Strength` (Strength 1–10 gives 33–60 kg; integer, D-022). Tiers: ≤100% normal; 100–125% **burdened** (walk speed −30%, no sprint, stamina drain ×2); >125% **pinned** (can't move; you can still drop things). Silver in the purse counts at 8 g per shekel. Followers and pack animals have their own capacity (FOL-08, ANM-01) | A C API read gives capacity, carried weight and tier; UE applies the speed changes; tests at each boundary (exactly 100%, 100% + 1 g, exactly 125%) |
| **FND-04** | **World items and containers as kernel state** (new `sim/WorldItems.hpp`) | `WorldItem{ id, stack, place, x/y/z in cm (int), dropped_day, dropped_by }` and `Container{ id, place, kind (jar, chest, basket, granary, stall, body), owner, locked, key_item, capacity_g, stacks }`. Unreal spawns actors *from* the kernel when a place streams in, and each actor holds only its world-item id. **This one change makes dropping, picking up, looting, theft evidence and saving all work the same way.** The placeholder street pickups become seeded world items | Drop, save, quit, load: the item lies where it fell. New Game and Load agree |
| **FND-05** | **One verb-result protocol** | Every mutating C API verb returns `code` (≥0 success, <0 refusal) and writes a **reason key** (a string-table key such as `refuse.too_heavy`, plus arguments). One UE function turns any result into a toast or a line in the open panel. A shared refusal-code table lives in `kernel/contracts/verb_results.md` | No refusal anywhere shows a raw number or is silent |
| **FND-06** | **Timed actions** | One kernel entry advances the clock and every affected actor's needs together for N minutes (it generalises `SkipSimHours`). Every activity declares its minutes. Interruption rules: an attack, a need reaching critical, or the player cancelling stops the action; what's already done is kept, and the unit in progress is refunded (this matches `craft()`'s all-or-nothing rule) | One test per activity: interrupted halfway gives exactly the completed units |
| **FND-07** | **Interaction framework v2** (finishes A8) | Interactables return **a list of verbs** (`GetVerbs()`), not one label. Tap E does the primary verb; holding E opens the **verb wheel** with every verb the target supports; unavailable verbs are greyed out with their reason (FND-05). The focus trace prefers the object under the crosshair, then the nearest one in a 30° cone | Every interactable in the game lists its verbs; a controller can reach them all |
| **FND-08** | **The UI kit** (on the existing screen stack) | Reusable Slate widgets: an **item list** (sort by name, weight, value, quality, freshness or newest; filter by `ui_category`; search), an **item tooltip** (weight, local value, quality, condition, freshness, owner or "stolen from …", what it's used for), a **quantity picker** (one / half / all / slider / typed number), a **confirm dialog**, a **two-pane transfer widget** (used by containers, loot, trade, follower inventory and stashes), a **toast queue** with history, full controller navigation, and every string in string tables | Each widget has a UE automation test and a screenshot in `-SimShotScreens` |
| **FND-09** | **The player notice feed** | The kernel appends player-facing notices (a raid on the date grove, a follower's complaint, wages due, food spoiled) to a log with a cursor: `sim_world_notice_count` / `sim_world_notice_at`. UE shows new ones as toasts and keeps them in the journal's log | Daily-tick outcomes that concern the player always reach the screen |
| **FND-10** | **The per-mechanic test protocol** | Each mechanic ships with: a kernel unit test covering its edge cases, a C API test (plus the no-throw suite), a UE automation test that performs the verb headless, a `sim.*` console command, a save round-trip assertion, and a smoke-test line when it crosses systems | `tools/coverage_check.py` gains a mechanics table and fails when a mechanic id has no test |

---

## 2. The mechanic catalogue: the ten areas the designer named

Each row has the rule, today's state (**K** kernel · **C** C API · **U** Unreal: ✅ done · ◐ partial · ✗ missing), and the test that closes it.

### 2.1 Inventory (INV)

| Id | Mechanic | Rule | K/C/U | Done when |
|---|---|---|---|---|
| INV-01 | Carry items | Stacks merge by FND-01; a stack never exceeds `stack_max` (the overflow starts a new stack) | ◐/◐/✗ | Merge and overflow tests |
| INV-02 | Weight and encumbrance | FND-03 tiers, shown as a weight bar (current / capacity) on the inventory screen and as a HUD icon when burdened or pinned | ✗/✗/✗ | Picking up past 100% slows the walk, and past 125% stops it |
| INV-03 | Inventory screen | **Tab toggles it** (today Tab is held down to show debug text). Category tabs follow `ui_category`, plus All and Recent. It uses the item list (FND-08) with the weight bar and the purse. The world pauses while it's open (decision D-1) | ✗/✗/◐ | Every held item shows with its icon, count, weight and value |
| INV-04 | Inspect | A detail pane shows the description, weight, local value (a quote from TRD-03), quality, condition, freshness (fresh / stale in N days / spoiled), owner or stolen status, and every action it supports | ✗/✗/✗ | The tooltip is right for one item of every category |
| INV-05 | Item actions | Use (eat / drink / read / equip / apply), Drop (with a quantity picker), Split, Give to a follower, Offer (at a shrine), Put in a container (when one is open). Each action is greyed out with a reason when it can't be done | ◐/◐/✗ | Every action on every category is tested |
| INV-06 | Split and merge | Split N from a stack; dragging a stack onto a matching one merges them | ✗/✗/✗ | Tests where the split quantity is 0, 1, all, or more than the stack |
| INV-07 | Stolen goods | Anything taken without the owner's permission gets `stolen=true` and keeps its owner. Honest sellers who know the owner refuse it; fences buy it at 40%; guards and the owner recognise it (H4 evidence). The flag clears only when a fence sells it on, or when the item goes back to its owner | ✗/✗/✗ | Steal, try to sell (refused), fence it, and the owner recognises it on you |
| INV-08 | Bound items | Quest items and documents can't be dropped or sold ("You can't part with this"); documents open in the tablet reader | ✗/✗/◐ | Each refusal shows its reason |
| INV-09 | Spoilage | Freshness = today − `made_day` measured against `spoil_days`, with stages fresh, stale (restores 50%) and spoiled (restores 25% and carries an illness risk, B3). The daily tick ages inventories, containers and world items. Salted and dried variants come from recipes (B7) | ✗/✗/✗ | Fish goes stale on day 1 and spoils on day 2; salted fish keeps for 60 days |
| INV-10 | Condition | Arms and tools lose condition with use (combat wear exists in the kernel); at 0 the item is broken and can't be used until it's repaired (`sim_world_repair`) | ◐/◐/✗ | A broken axe can't be equipped; after repair it can |
| INV-11 | Purse | Silver shown as shekels and grains, counted by weight (FND-03). The same numbers appear in trade, wages and debts | ◐/◐/◐ | The purse agrees everywhere it is shown |
| INV-12 | Equipment screen | A paper doll: 4 zones × 2 armour layers, main hand, off hand (shield), ammunition, clothing (dress signals, G7). Equipping from the inventory uses `sim_world_equip`; the screen totals protection, weight, stamina cost and heat | ✅/✅/✗ | Every slot can be equipped and unequipped; the totals match the kernel |
| INV-13 | Quick eat and drink | F and G stay. "Best" changes to: the food that restores the most **without overshooting**, and the one that spoils soonest when two are tied (EAT-02) | ◐/◐/✅ | A test holding a fresh loaf and a stale one eats the stale one |
| INV-14 | NPC inventories | NPCs hold stacks the same way; a shop's stock is its owner's inventory plus the stall container (TRD-01) | ◐/◐/✗ | One kernel type holds everyone's goods |
| INV-15 | Save | All stacks, containers and world items survive a round trip; the snapshot version goes up, and old saves migrate (each count becomes one common, player-owned, fresh stack) | ✗/✗/✗ | A save made before this change still loads |

### 2.2 World items: dropping, picking up, containers, loot (WLD)

| Id | Mechanic | Rule | K/C/U | Done when |
|---|---|---|---|---|
| WLD-01 | Drop | Choose a quantity; the stack becomes a world item at the player's feet (traced to the ground, never inside a wall). It stays owned by the player. Bound items refuse. You can still drop while pinned | ✗/✗/✗ | Drop, walk away, come back: it's there. Save and load: it's there |
| WLD-02 | Drop into water | In the lagoon, the river or the sea, items flagged `floats` (reeds, wood) drift and stay recoverable; everything else sinks and is lost, with a notice | ✗/✗/✗ | A test drops one of each kind |
| WLD-03 | Pick up | E on a world item. If it would push you past 125%, a quantity picker offers the most you can take. The item keeps its owner | ◐/◐/◐ | Partial pickup works; pickups persist (FND-04) |
| WLD-04 | Picking up what isn't yours | The prompt changes to **"Steal"** in red when the item has another owner. Taking it runs `commit_crime(theft)` with the witnesses who can see you (a line-of-sight and radius stub until G1's perception arrives). Unseen, it's only flagged stolen (INV-07) | ◐/✗/✗ | Taking a stall's bread in view of the owner files a theft; out of view it doesn't |
| WLD-05 | Containers | Jars, chests, baskets, granaries and stalls, each with a capacity, an owner, and optionally a lock and key. Opening one uses the two-pane transfer widget with Take, Take all, Put and Put all | ✗/✗/✗ | Every container kind can be used |
| WLD-06 | Locks | A lock opens with its key item, by permission (the owner or your household), or by **forcing** it: noisy, it takes time, it damages the lock, and it's burglary if anyone hears (the skill list has no lockpicking; decision D-8) | ✗/✗/✗ | All three paths work, and forcing is a crime when heard |
| WLD-07 | Loot bodies | Uses the transfer widget over `sim_world_loot`. Looting is legal on a battlefield, on a bandit you killed, or with the owner's heir's consent; otherwise it's theft (K5) | ◐/◐/✗ | Looting a bandit is clean; looting a townsman is a crime |
| WLD-08 | Persistence and scavenging | World items in public places that nobody picks up for **1 day** are taken by a passer-by (they move into that NPC's inventory and start a rumour: "someone found a bronze axe by the gate"). Items inside your own house or container never disappear | ✗/✗/✗ | Dropped bread in the market is gone the next day and the rumour exists |
| WLD-09 | NPCs pick things up | Beggars and scavengers take unattended goods (see WLD-08); guards take items that are crime evidence | ✗/✗/✗ | Covered by the WLD-08 test plus an evidence test |
| WLD-10 | Placing | Drop with a precise placement preview (on a shelf, on a table), used for decorating your house and for offerings on an altar | ✗/✗/✗ | A placed item keeps its exact position through a save |
| WLD-11 | Physics settle | A dropped item falls and settles, physics turns off after it rests, and the kernel stores the resting position; an item can never fall through the floor (there's a fallback snap to the floor) | ✗/✗/✗ | 100 random drops in a house: none end up under the floor |
| WLD-12 | Seeded world goods | The kernel seeds world goods by building type (bread on bakery racks, jars in granaries, tools in workshops); they replace today's placeholder pickups (`sim.VerbDemoProps`) | ✗/✗/◐ | Every building type that should hold goods does |

### 2.3 Buying, selling and services (TRD)

| Id | Mechanic | Rule | K/C/U | Done when |
|---|---|---|---|---|
| TRD-01 | What a shop is | An owner NPC, a place, stock (the owner's inventory plus the stall container), a **buy list** (the `ui_category`s they'll take), and opening hours from the owner's schedule (the task `sell`). It's open only when the owner is present and awake, `sim_world_market_open` is 1, and no festival closes it | ◐/◐/✗ | A shop closes when its owner goes home, and on a closed festival day |
| TRD-02 | The trade screen | Opened by Talk → Trade. Two panes: their goods at the buy price, yours at the sell price; a running balance; both purses; Confirm and Cancel. **Nothing changes until you confirm** | ✗/✗/✗ | Cancelling leaves both sides byte-for-byte unchanged |
| TRD-03 | Price quotes (kernel) | The existing `quote_buy/quote_sell` (bargaining, talents, rank with the city's polity) extended with quality (poor 60%, common 100%, fine 150%, masterwork 250%), condition, standing, language and scarcity; integer maths; **each factor is returned as a line**, so the UI can explain the price (C4). `sim_world_buy_quote/sell_quote` exist; a breakdown call is added | ◐/◐/✗ | The same item quotes differently with low and high standing, and the breakdown says why |
| TRD-04 | Buying | `sim_world_buy` exists against the city market. It becomes per shop: one atomic transaction that checks the purse and the **shop's** stock, warns (but allows) if it overloads you, moves the stacks (ownership passes to the player), moves the silver **to the owner's purse**, calls `Economy::consume`, gives Bargaining XP (exists), and adds a "traded with" memory | ◐/◐/✗ | A test for every refusal: no money, no stock, shop closed |
| TRD-05 | Selling | `sim_world_sell` exists against the city market. Per shop: the seller can't pay more than their purse holds (the rest can be taken as barter goods or refused); only categories on their buy list; stolen goods only to a fence; bound items never. Sold goods become their stock, and `Economy::deliver` updates the market | ◐/◐/✗ | Selling to a merchant with an empty purse is refused with its reason |
| TRD-06 | Barter | Goods for goods, with silver to make up the difference either way. The seller accepts when the value in ≥ the value out × a fairness threshold that depends on their personality | ✗/✗/✗ | Uneven barters are refused; even ones pass |
| TRD-07 | Haggling | One step: offer a percentage; acceptance chance from Bargaining and opinion; a failure lowers opinion a little and closes haggling for the day (decision D-5) | ✗/✗/✗ | Tests of the odds at skill 0, 50 and 100 |
| TRD-08 | Buyback | Something you sold during this visit can be bought back at the price you got for it | ✗/✗/✗ | A test of a sale that was a mistake |
| ECO-02 | Stock is finite | Buying the last loaf empties the shelf; restock comes only from production (C7, D6) | ◐/✗/✗ | The next customer finds no bread |
| TRD-09 | Services | Buying things that aren't items: a meal at the tavern (buy + eat), a bed for the night, washing and purification, a physician's treatment, a scribe reading your letter, a diviner's reading, lessons with a teacher, passage on a boat, hiring a follower. A `services.csv` row gives the price, the provider kind and the verb it triggers | ✗/✗/✗ | Every service can be bought from its provider |
| TRD-09 | Stealing from a shop | Taking from a stall container while the owner isn't watching is theft (WLD-04); with the owner watching, it's refused or reported | ✗/✗/✗ | Both branches work |
| TRD-11 | Rank and language gates | The governor's palace sells from rank 3 up; merchants who don't share a language with you add a surcharge (E5) | ✗/✗/✗ | A rank-0 player is refused at the palace |
| ECO-11 | Credit, scales, cheating | Covered by C5 (silver weighed on diegetic scales, cheats caught with Reckoning) and C6 (credit on a tablet, then collection) | ✗/✗/✗ | Per completion-plan C5–C6 |
| TRD-13 | Mid-trade interruptions | If the owner dies, the market closes or a raid starts during a trade, the screen closes and nothing is exchanged | ✗/✗/✗ | Each case has a test |

### 2.4 Eating and drinking (EAT)

| Id | Mechanic | Rule | K/C/U | Done when |
|---|---|---|---|---|
| EAT-01 | Eat a chosen item | Inventory → Use calls `sim_world_eat(item)` | ✅/✅/✗ | Any food can be eaten from the inventory |
| INV-13 | Quick eat and drink | Specified in §2.1 | ◐/◐/✅ | — |
| EAT-03 | Eating takes time | A snack takes 2 sim minutes, a meal 15 (with an animation); an attack interrupts it and whatever wasn't eaten stays in the stack | ✗/✗/✗ | FND-06 test |
| EAT-04 | Satiety cap | Eating with hunger below 5 is refused ("You're full"), except at a feast, which allows overeating with a fatigue penalty | ✗/✗/✗ | A test at the boundary |
| EAT-05 | Water sources | Well: clean and free (exists). River and lagoon: free, with an illness risk (B3). **Fill a waterskin** (a container item) at any source | ◐/◐/◐ | Filling and drinking a waterskin works |
| EAT-06 | What food does | Diet groups and the Well-fed bonus (B6); beer and wine intoxicate (B5); some foods lower purity (B4); spoiled food risks illness (INV-09); your patron god's taboo foods anger the god (J4); custom asks for the first portion to the gods (G6) | ✗/✗/✗ | Per the linked steps |
| EAT-07 | Meals as a service | Buy a meal at the tavern, or eat at a host's table as a guest (the hospitality custom) | ✗/✗/✗ | TRD-09 |
| FOL-11 | Followers and household eat | From their own inventory first, then from the shared pack; a hungry follower loses loyalty (FOL-12) | ✗/✗/✗ | An unfed hireling complains on day 2 |
| BOD-20 | Starving and thirsting | The effects ladder (exists) → health loss → death (B10) | ◐/◐/◐ | Per B10 |
| CRF-12 | Cooking | Crafting at a fire or an oven (CRF) | ◐/◐/✗ | Per CRF |

### 2.5 Crafting (CRF)

| Id | Mechanic | Rule | K/C/U | Done when |
|---|---|---|---|---|
| CRF-02 | Station actors | Quern, oven, brewing vat, fire and pot, kiln, forge (bellows, crucible, moulds), loom, dye vat, press, carpenter's bench, leather frame, brick mould (D1). Each is placed by building type, has an owner, and can be used if it's yours or public, if you rent it, or with the owner's permission. Using one without leave is trespass (G6) | ◐/◐/✗ | Every station kind has an actor in at least one building |
| CRF-03 | The crafting screen | Opened at a station: its recipes, **known and unknown** (a new kernel rule: a recipe is known when your skill reaches its threshold, or when you learn it from a teacher or a text, just like rites), the inputs you have against what's needed, the outputs, the hours, the skill it uses and the likely quality range | ✗/✗/✗ | Unknown recipes are shown as "?" with how to learn them |
| CRF-05 | Where inputs come from | Your inventory, plus containers you own within 3 m of the station | ✗/✗/✗ | A test with the inputs split between your pack and your jar |
| CRF-06 | Batch crafting | Craft ×N, up to the most your inputs allow | ✅/✅/✗ | — |
| CRF-07 | Time and interruption | FND-06: the units you finished are kept and the unit in progress is refunded | ◐/✗/✗ | A test interrupting at 2 of 5 |
| CRF-08 | Quality | Output quality comes from skill, tool, input quality, station quality and a roll; the skill gets XP; talents and perks change it (E6) | ✗/✗/✗ | The same recipe at skill 10 and at skill 80 gives different quality |
| CRF-10 | Output too heavy | Whatever you can't carry lands at your feet as world items | ✗/✗/✗ | A test |
| CRF-09 | Tools and fuel | Tools are needed but not used up (they lose condition); kilns, forges and ovens burn fuel (reeds, dung cakes, charcoal), which is used up | ✗/✗/✗ | You can't fire the kiln without fuel |
| CRF-20 | Water as an input | From the virtual water of a well or river next to the station, or from a waterskin | ✗/✗/✗ | Bread bakes beside a well with no waterskin |
| CRF-12 | Recipe chains and minigames | Recipe storm D2; smithing D3; cooking and brewing D4; commissions D8; repair and recasting (exist in the kernel) | ◐/◐/✗ | Per completion-plan D2–D8 |

### 2.6 Activities (ACT)

**One framework, not forty bespoke verbs:** a new `activities.csv` (id, verb, where (place kinds or actors), requirements (items, skill, rank, permission), minutes, needs changes, outputs, skill XP, whether it repeats, whether it can be interrupted, crime risk) and one kernel function that runs any row as a timed action (FND-06). Unreal gets one `USimActivitySpot` component that buildings and props carry. New activities then only need data.

| Id | Activity | Where / requires | Output | K/C/U |
|---|---|---|---|---|
| ACT-02 | Sleep | Your bed, a rented bed, a guest bed (hospitality), or rough on the ground (worse recovery, risk of theft) | Fatigue recovers by bed quality (B8); autosave | ◐/◐/◐ |
| ACT-03 | Wait / rest | Anywhere safe; a menu picks the hours (refused when enemies are near) | Time passes; stamina | ✗/◐/✗ |
| ACT-04 | Work for wages | At each of the 19 `work_roles`, while the employer is present | Rations or silver (`sim_world_work`), XP, the day's output enters the market (D5) | ✅/✅/✗ |
| ACT-05 | Draw water | A well or river | Fill a waterskin or drink | ◐/◐/◐ |
| ACT-06 | Wash | A well, the river, a bath house | Purity restored (B4) | ✗/◐/✗ |
| ACT-07 | Pray and make offerings | A shrine, a temple or your household shrine | Favour (`sim_world_add_favour`); offerings are used up | ◐/◐/✗ |
| MAG-05 | Perform a rite | J1's flow: prepare → perform → outcome | `sim_world_perform_rite` | ✅/✅/✗ |
| MAG-12 | Divination | J3 | Omens with probabilities | ◐/◐/✗ |
| ACT-09 | Study a text, train with a teacher, practise | E3 | Skill XP (`study/train/practice_skill`) | ✅/✅/✗ |
| ACT-12 | Fish, forage, gather (reeds, clay, silt), hunt, herd, glean | Resource spots, whose yield runs down and recovers daily (like `WildState` sites) | Items, XP | ✗/✗/✗ |
| ACT-18 | Sit, lean, emote (bow by rank, mourn, pray) | Benches, walls, anywhere | Social reactions (G3) | ✗/✗/✗ |
| ACT-20 | Read a tablet | Any document item | The tablet reader | ◐/◐/◐ |
| ACT-22 | Treat wounds | Needs medicine; a physician can do it as a service | `sim_world_treat_wounds` | ✅/✅/✗ |
| ACT-23 | Repair and recast | At a smith's forge | `sim_world_repair/recast` | ✅/✅/✗ |
| FST-09 | Festival roles | O5 | Favour, standing | ✗/✗/✗ |
| ACT-21 | Games of chance | Astragali (knucklebones) at the drinking fellowship; betting silver. **Decision D-11**: in or out | Silver changes hands | ✗/✗/✗ |

Every activity must pass: it can be interrupted (FND-06), it refuses with a reason (FND-05), it costs time and moves needs, and it respects ownership (it's trespass to work someone's field without leave).

### 2.7 Raiders and the raider menus (RAD)

**The world side** (the simulation exists; it has to become something you can see):

| Id | Mechanic | Rule | K/C/U |
|---|---|---|---|
| RAD-01 | The daily raid formula | hunger + opportunity − defence − fear, with faction politics (exists) | ✅/✅/✗ |
| RAD-05 | Raids happen physically (K8) | When a raid fires on a target inside the loaded world, a raider party spawns (its size from the band's strength), comes in from the camp's direction, takes goods (stall stock, fields, herds, **the player's containers and assets**), fights the guards, the player and the followers, and leaves with what it carried. Out of view, the existing abstract result stands. **Both routes change the kernel the same way** | ✅/✅/✗ |
| RAD-07 | Warnings | Rumours of camps (`sim_world_hear_rumours`), smoke on the horizon, scouts seen near the fields, omens, and the watch's alarm when raiders come through the gate | ◐/◐/✗ |
| RAD-08 | Aftermath | Goods leave the market (exists), people are killed or carried off (`npc_fate`), rescue quests start (exist), families mourn (I2), prices rise, and a notice reaches you (FND-09) | ◐/◐/✗ |
| RAD-09 | Defending | Join the guards; your followers and band fight; the gates close; **your hired guards and dogs add defence to your property's score in the formula** (a new kernel input) | ◐/✗/✗ |
| RAD-04 | Camps in the world | Camp actors in the wild lands (Q4): tents, a fire, a loot pile (a container), tied captives, leaders by name, sentries on a schedule | ✅/✅/✗ |
| RAD-18 | Consequences | Infamy, crimes filed (`lead_raid` and `rob_caravan` already file them), outlawry, the factions' reactions, a bounty on you | ✅/✅/✗ |
| RAD-12 | Captives | Rescued by attacking or infiltrating; ransomed (`ransom_captive`); sold east after 45 days (exists) | ✅/✅/✗ |

**The raider menus** (the player-facing side, each button mapped to a call that already exists):

| Id | Menu | Shows | Actions → call |
|---|---|---|---|
| RAD-16 | **The camp screen** (approaching a camp, or from the map once you know where it is) | The band's name, faction and leaders; strength as a guess ("a dozen spears") until you scout it, exact afterwards; morale; the known loot pile; known captives; its politics (`band_politics`: war / grudge / treaty / outlaw); your standing and infamy | **Scout** → `scout_camp` · **Infiltrate** (pick an objective: free captives / steal loot / sabotage / learn) → `infiltrate_camp` · **Attack** (with your followers, FOL-16) → `attack_camp` · **Pay them off** (the price is shown) → `pay_off_group` · **Ally** → `ally_with_group` · **Join** → `join_group` · **Challenge the leader** → `challenge_leader` · **Ransom** (per captive) → `ransom_captive` · Leave. Every refusal code −1…−6 has its string key |
| WLA-05 | **The caravan screen** (met on the road or at the gate) | The route, goods, guards, and how the escort is going | **Escort** → `escort_caravan` · **Abandon the escort** → `abandon_escort` · **Rob** → `rob_caravan` · Trade (TRD) |
| WLA-06 | **The site screen** (ruins, caves, tombs) | What's known, the danger, and whether it's a tomb (a crime) | **Search** → `search_site`; it warns when the search would be tomb robbery |
| RAD-17 | **The band screen** (when you lead a group: `player_group`) | The roster (from FOL), strength, supplies, morale, the loot store, infamy, the current camp | **Lead a raid** (target picker: fields / herds / caravans / market, each showing today's chance from `assess_raid`) → `lead_raid` · **Found a band** at an empty camp → `found_band` · **Share out the loot** (shares change loyalty) · **Recruit** (from refugees, deserters and bandits) · **Move camp** · **Dismiss a member** · **Leave the group** → `leave_group` |
| WLA-04 | **Meeting raiders while travelling** | The encounter, the odds, and your stance | fight / pay / flee against hostiles; rob / give / pass with others (the `travel` stance) |

**Kernel gaps to close for raiders:** `companions` becomes a count taken from the real follower roster, and losses land on actual followers (FOL-16); the band's roster, supplies and loot shares for `player_band`; world coordinates for camps and approach paths; the player's property assets on the raid target list; and defence from hired guards and dogs.

### 2.8 Followers and their menu (FOL): a new kernel module, `sim/Followers.hpp`

**State:** `Follower{ npc, kind (companion | hireling | band | household | animal), joined_day, wage (silver per day or rations), paid_until, loyalty 0–100, order, stance (passive / defensive / aggressive), home_place, status (active / waiting / dismissed / deserted / captive / dead) }`. Followers use the NPCs' existing inventories, needs and combat sheets. It is saved as a new snapshot section.

| Id | Mechanic | Rule | Done when |
|---|---|---|---|
| FOL-02 | Recruiting | In dialogue: **companions** (their own conditions, opinion ≥ 60, at most 3); **hirelings** (paid up front at the wharf, the tavern or the gate; guards, porters, guides, sailors); **band members** (needs the Mercenary calling or rank 3+, or leading a wild group; at most 30); **animals** (bought, FOL-17) | Every kind can be recruited, and each cap is enforced |
| FOL-05 | Following | Pathing with an offset formation; they step aside instead of blocking you in doorways; **catch-up teleport** only when they're more than 50 m away and neither of you can see them; they wait outside places that bar them (a temple bars the impure, B4) | A 10-minute walk through the city loses nobody |
| FOL-06 | Orders | Follow · Wait here · Hold position · Go home · Guard this place · Attack my target · stance passive / defensive / aggressive · Use an item (heal yourself) · Talk · Dismiss. Given from an **order wheel** (a hotkey while looking at a follower) or from the follower screen | Every order has an automation test |
| FOL-08 | Their inventory | Exchanged through the transfer widget; each has a carry capacity; a pious follower refuses stolen goods (personality) | A porter carries 40 kg of your goods |
| FOL-09 | Their equipment | `sim_world_equip` with the follower's id as the actor (the call already takes any actor) | You can give a hireling a spear and they fight with it |
| FOL-10 | Pay | Wages come due daily. **Auto-pay** from your purse, or a debt builds up; rations come from the shared pack. After 3 unpaid days they complain; after 7 they desert | An unpaid hireling deserts on day 7 |
| FOL-11 | Their needs | They eat, drink and sleep through `NeedsState` (exists): their own food first, then the shared pack; they sleep when you sleep | A follower's hunger shows on the follower screen |
| FOL-12 | Loyalty | Integer inputs: pay, food, treatment (orders they disapprove of, being left behind in danger), keeping your word, shared danger (+), compatible faith, your rank and reputation. Outputs at thresholds: complaints (barks), refused orders, stealing from you, desertion, **betrayal** (reporting you to the watch, or turning in a raid) | The kernel scenario: an unpaid band deserts; a mistreated companion betrays you |
| FOL-15 | Combat | On the kernel resolver: wounds; a downed follower can be treated in time; **companions who die stay dead**; low morale makes them flee | A companion can die permanently |
| FOL-14 | Opinions of what you do | Companions have values; they approve or disapprove of your acts with a notice ("Ilum disapproves"), and that moves loyalty | Stealing in front of a Retributor companion costs loyalty |
| FOL-19 | Companion stories | Personal quest lines, levelling up, marriage, leaving, death (L2–L3) | Per L2–L3 |
| FOL-17 | **The follower screen** | A roster (portrait, kind, loyalty bar with its reasons in a tooltip, health, needs, wage and paid-until, current order). A detail view with tabs: **Inventory** (transfer), **Equipment**, **Orders**, **Relationship** (a log of their opinion), **Quest**. Pay all, and Dismiss (with a confirm; a dismissed companion goes home and can be recruited again) | Every tab is live; a controller can reach all of it |
| FOL-16 | Followers in the wild verbs | `attack_camp`, `found_band`, `escort/rob_caravan` and `travel` read the active fighters from the roster instead of an integer; casualties land on real followers | Losing a fight at a camp wounds or kills named followers |
| FOL-22 | Time, travel and saves | Followers travel with you (fast travel too); they live through time skips (P11); every field saves | A save round trip with 3 companions and 5 hirelings |
| FOL-21 | Their crimes | Followers commit crimes only when ordered to; witnesses see who did it; a share of the blame falls on you | Ordering a theft files it against the follower and costs you standing |
| FOL-04 | The band at scale | Up to 8 band members are spawned as actors; the rest fight as numbers in battles (K7) | A 30-strong band runs at 60 fps |
| ANM-01 | Animals | A pack donkey carries 65–75 kg on a lead rope; a dog warns of thieves; they need feeding and watering; they go lame; they can be stolen (L8) | Losing the donkey in the desert is a crisis |
| HH-01 | Household | The same module with kind `household`: they stay at home and work your property (L6) | Per L6 |

### 2.9 Every menu and screen (UI)

| Screen | Opens from | Status | Stage |
|---|---|---|---|
| Main menu, Pause, Save/Load, Settings, Loading | the shell | ✅ | A |
| Journal (quests, journal lines) | J | ◐ (rumours, people and facts still to come: P10) | P |
| Tablet reader | reading a document | ◐ (every document item: T3) | T |
| HUD: needs, health, conditions, purse, date, moon | always | ◐ (heat/cold, purity, intoxication, encumbrance: B1) | B |
| Interaction prompt + **verb wheel** | E / hold E | ◐ → FND-07 | 0 |
| **Toasts + notice history** | automatic | ✗ → FND-08/09 | 0 |
| **Inventory** + item detail + quantity picker | Tab | ✗ → INV-03 | 1 |
| **Equipment** | a tab of the inventory | ✗ → INV-12 | 1 |
| **Container / loot** (transfer) | E on a container or body | ✗ → WLD-05/07 | 1 |
| **Wait / sleep hours picker** | E on a bed / the wait key | ✗ → ACT-02/03 | 1 |
| **Activity panel** (progress bar, cancel) | any timed action | ✗ → FND-06 | 1 |
| **Trade** (+ barter, haggle, buyback, the price explained) | Talk → Trade | ✗ → TRD | 2 |
| **Services** | Talk → Services | ✗ → TRD-09 | 2 |
| **Dialogue** | E on an NPC | ✗ (P4; needed early as the way into trade and recruiting, so a minimal version comes in Phase 2) | 2 / P |
| **Crafting** | E on a station | ✗ → CRF-03 | 3 |
| **Character sheet** (attributes, skills, level, XP, calling) + **talent tree** + the level-up choice | C | ✗ → E1–E2 | E |
| **Followers** + the order wheel | F1 / looking at a follower | ✗ → FOL-17, FOL-07 | 4 |
| **Camp**, **caravan**, **site**, **band** | E at the place / from the map | ✗ → RAD-16, RAD-17, WLA-05, WLA-06 | 5 |
| **Travel** (destination, mode, stance, time estimate) | the map / the gate | ✗ → Q6 | 5 |
| **Map** (you draw it) | M | ✗ → T2 | T |
| **Codex** (the Scribe's Case) | K | ✗ → T1 | T |
| **Factions and reputation** (the three layers) | from the journal | ✗ → Q1, G4 | G/Q |
| **Property ledger** | from the journal | ✗ → M6 | M |
| **Rites** (prepare → perform) and **divination** | at an altar / with materials | ✗ → J1, J3 | J |
| **Arrest choice**, **hearing**, **verdict** | when arrested | ✗ → H5, H7 | H |
| **Death**, **heir**, **epitaph** | on death | ✗ → B10, P12, P13 | B/P |

**Rules every screen follows:** it opens and closes from the keyboard and the controller; Esc / B always goes back one screen; it pauses or doesn't according to D-1; every string comes from a string table; every refusal shows its reason; it has an automation test and a `-SimShotScreens` capture; it's readable at every text-size setting and in every colour-vision mode.

---

## 3. Everything else, mapped

Every mechanic below has its own row, with a rule, a source, a status and a phase, in [mechanics-registry.md](mechanics-registry.md) (areas SIM, TIM, BOD, ECO, CHR, LNG, CMB, ARM, HH, ANM, PRP, BLD, FAC, WLA, NPC, RCT, REP, CUS, LAW, DTH, MAG, MGK, DIV, FST, EVT, QST, DLG, END, UI, DGT, SET, SAV, AV, DEV), and every canon row they act on is an instance in [mechanics-instances.md](mechanics-instances.md). This table is the summary by completion-plan stage.

| Area | Mechanics (each is a row to check off) | Completion-plan steps |
|---|---|---|
| **Body** | hunger, thirst, fatigue (exist); heat and cold; heatstroke and chill; illness (fever, dysentery, eye disease, plague) with contagion and remedies; purity (lowered by corpses, blood, foods, sex, oath-breaking; restored by washing, rites, time; temples bar the impure); intoxication stages; diet groups and Well-fed; spoilage; sleep quality; encumbrance; player death | B1–B10 |
| **Character** | 6 attributes; 27 skills grown by use, teachers and texts; XP and levels to 40; talent points and the 41 talents; callings at 5, specialisation at 15, a second calling at 25; +1 attribute every 4 levels; rank per polity through a patron's ceremony; languages spoken and scripts read; perks felt in play | E1–E6 |
| **People** | real names; the full NPC record; goals; memory and relationships; rumour at walking speed; L0–L2 simulation layers; ~160 people; crowds; NPC animation; life events | F1–F10 |
| **Reaction** | perception (sight, light, sound, smell); judgement by law, custom and values; the response ladder 0–7; reputation in three layers and its effects; customs; dress as a signal; gifts | G1–G8 |
| **Justice** | enforcers; guard AI, chases, descriptions, bribes; the full law table; evidence; arrest choices; detention; hearings with oaths; verdict tablets; every sentence; escape; outlawry; night rules | H1–H12 |
| **Death** | bodies; mourning; burial and processions; offerings and the ancestor rite; the restless dead; your own funeral; tomb robbery | I1–I7 |
| **Magic** | the rite flow; rite coverage of the 8 families; 9 divination forms; per-god favour, taboos, equated gods, neglected gods, a patron god; guard rails; the full magick of the notes; Chronicle mode; sorcery as a crime; wrath made visible; divine intervention | J1–J10 |
| **Combat** | directional melee, blocking, stance, stamina; ranged weapons; zonal wounds; equipment and wear; duels, surrender, prisoners, ransom, looting; NPC combat AI; band battles; physical raids; chariots | K1–K9 |
| **Property** | houses, fields, orchards, herds, workshops, ship shares, caravan partnerships, loans; deeds; obligations; the management loop; steward reports; the property ledger; voyage risk; debt collection | M1–M8 |
| **Build and settle** | construction; the settlement ladder; settlement simulation; kingship; ruling | N1–N5 |
| **Calendar and festivals** | lucky and unlucky days; the city's clock; the seasons; festivals and their mechanics; weather; real night | O1–O7 |
| **Events, quests, dialogue, time** | the event storm, its staging and frequency; the dialogue UI and content; side and systemic quests; delivery channels; no markers by default and guided mode; failure and timeouts; the journal; time skips; heirs; the epitaph; aging | P1–P14 |
| **Factions and the wild** | the faction UI and oaths; minor factions with every verb; faction decay; walkable wild lands; encounters; travel by mode; off-map ventures; the other cities' people; ruins | Q1–Q9 |
| **Audio, world, diegetic UI, quality** | soundscape, SFX for every verb, music, barks, voice hooks; interiors, clothing, arms meshes, animals, animations, VFX, smells, script signs; codex, map, tablet reader, scales, accessibility; performance, save versioning, soak tests, localisation, packaging, the final audit | R1–R6, S1–S11, T1–T5, U1–U7 |

---

## 4. What "fully functional and perfected" means: the checklist every mechanic must pass

A mechanic is done only when **all** of these are true (this extends completion-plan §1 rule 4):

1. **Rule written:** its row here says exactly how it behaves, including its numbers.
2. **Kernel:** deterministic, integer maths (D-022), no wall clock; it writes other modules' state only through their public APIs.
3. **Kernel tests:** the normal case, every refusal, and the boundaries (0, 1, the maximum, the maximum + 1, empty, full).
4. **C API:** exposed; covered by the C API test and the no-throw test; listed in `docs/capi-reach.md` as reached.
5. **Save:** survives a save → load round trip mid-game, and a save made by the previous version still loads.
6. **Reachable in the game:** a verb, a key or a screen gets to it (a kernel function no player can trigger is not done).
7. **Feedback:** success and every refusal show on screen in words (FND-05); nothing fails silently.
8. **Time and body:** costs the right game time and moves needs (FND-06); can be interrupted.
9. **Ownership and law:** respects owners; the crimes it can cause are filed with the right witnesses.
10. **The world does it too:** NPCs use the same rule (NPCs eat, buy, craft, pick things up, raid), so the world stays consistent.
11. **Followers:** works with followers present (they carry, eat, fight, react).
12. **Input:** keyboard, mouse and controller; rebindable.
13. **Text:** every string in a string table; readable at every text size and in every colour-vision mode.
14. **Presence:** an animation hook and a sound hook (filled by Stages R and S).
15. **Console:** a `sim.*` command sets it up or triggers it in seconds.
16. **Automation:** a UE test performs it headless; a smoke line when it crosses systems.
17. **Performance:** no hitch over 16 ms when it's used; nothing that grows every day without a bound.
18. **Played:** the designer has done it in a play session and signed off.

### Edge cases every relevant mechanic is tested against

- An inventory that's full or too heavy; a quantity of 0, a negative one, or one larger than the stack.
- Saving or loading while a menu is open or a timed action is running.
- Dying, or being attacked, during a menu, a trade or a timed action.
- The other party dying, leaving, going to sleep, or the market closing mid-trade.
- An item's owner dying (the ownership passes to their household or heir).
- Two stacks of the same item with different owners or quality; a stolen item bought back by its owner.
- Dropping something in a place that then streams out; a world item in an interior that isn't loaded.
- A follower dying while holding your goods (they become loot on their body).
- The day rolling over, or a festival starting, during an activity.
- Pausing during a timed action; alt-tab; a controller disconnecting; a rebinding conflict.
- Raids during a trade, during sleep, and during travel.

---

## 5. The implementation order (phases)

Each phase follows the designer's cycle: **wave → merge → bug review → fix → gate → push → designer play session**. Each phase gets its own detailed TDD plan in `docs/superpowers/plans/` (task by task, with the tests written first) before any code is written.

| Phase | Contents | Why it comes here | Gate: playable proof |
|---|---|---|---|
| **P0 Foundations** | Every registry row with phase `P0` (FND-01…10, UI-03, UI-04, BOD-19) | Every later mechanic reads or writes items, results, time and the UI kit | All current tests pass on item model v2; the verb wheel works; the transfer widget works; old saves migrate |
| **P1 The core loop** | Every row with phase `P1`: INV, WLD, the eating rows, sleep, wait, work, water, washing, reading | The hands: carry, drop, pick up, eat, drink, sleep, work | Wake up, work at the wharf, get paid in grain, carry it home, drop it into your jar, eat bread, sleep; save, load, and everything is where it was |
| **P2 Trade** | Every row with phase `P2`: TRD, the price modifiers, meals as a service, the minimal dialogue screen (DLG-02); then every seller type (TRD-14, stage C) | Needs P1's items, ownership and transfer widget | Buy bread, sell grain, get refused selling stolen goods, fence them, see the price explained |
| **P3 Crafting and activities** | Every row with phase `P3`: CRF, the activity framework and every ACT row; stations placed in their buildings (coordinated with the city session); the minimum chains (bread, beer, a pot, linen, bronze) | Needs P1 and P2 (inputs are bought, outputs are sold) | Grind grain, bake bread, sell it; fire a pot; learn a recipe from a teacher |
| **P4 Followers** | Every row with phase `P4`: the `Followers` module and FOL rows; the first 2 companions (their full lines come with stage L); hirelings | Needs P1 (inventory, eating) and P2 (hiring as a service) | Hire a porter, load them, forget to pay them, and watch them desert on day 7 |
| **P5 Combat core + raiders** | Every row with phase `P5`: the combat resolver in play (CMB-01, 02, 04–06), every RAD row including kidnapping and the player taken captive, followers in fights and wild verbs; camps built in the wild (coordinated with the city session) | Physical raids need combat; the raider verbs need real followers | Hear a rumour, scout the camp, pay one band off, attack another with your companions, lead your own band's raid on a caravan, and be outlawed for it |
| **P6 onwards** | Every remaining registry row, grouped by its stage letter, in completion-plan order: B body, E character and languages, F people, G reaction, reputation and customs, H justice, I death, J magic, the notes' magick and divination, K the rest of combat, L the rest of followers, household and animals, M property, N building and kingship, O calendar, festivals and eclipses, P events, quests, dialogue, heirs and time skips, Q factions and the wild, R audio, S the world's presence, T the diegetic UI, U ship quality. `MQ` rows get their system now and their story with the main quest | — | Each stage's gate is that every registry row in it is `Y/Y/Y` (or `-`) with a passing `MECH:` test, and every instance of its canon tables is tested |

**Content storms run beside their phase:** item rows and weights (P0–P1), `services.csv` and seller placement (P2), recipes and `activities.csv` (P3), the companion roster (P4), camps and raid content (P5). Each is lint-gated and ledgered under D-018.

---

## 6. Where the code goes (so each phase has a home)

| Layer | New files | Changed files |
|---|---|---|
| Kernel | `sim/Items.hpp` (FND-01), `sim/WorldItems.hpp` (FND-04), `sim/Trade.hpp` (TRD), `sim/Activities.hpp` (ACT), `sim/Followers.hpp` (FOL), `sim/Notices.hpp` (FND-09) | `Crafting.hpp` (stacks, quality, known recipes), `Needs.hpp` (satiety cap, spoilage), `WildActions.hpp` (the roster instead of `companions`, player property as raid targets), `World.hpp` + `Snapshot` (new sections and migration) |
| C API | `CApiItems.h`, `CApiTrade.h`, `CApiActivities.h`, `CApiFollowers.h` | `CApiVerbs.h` (best food rule), `CApiWild.h` (roster-aware verbs) |
| Canon | `services.csv`, `activities.csv`, `world_items` seeding by building type | `items.csv` (FND-02 columns), `recipes.csv`, `work_roles.csv` |
| Unreal | `SimWorldItem`, `SimContainer`, `SimStation`, `SimActivitySpot`, `SimShop`, `SimFollowerController`; `UI/SimScreenInventory`, `…Transfer`, `…Trade`, `…Craft`, `…Followers`, `…Camp`, `…Band`, `…Activity`, the verb wheel, the toast queue | `SimPickup` (becomes `SimWorldItem`), `SimInteractable` (`GetVerbs()`), `SimPlayerController` (Tab toggles; new keys), `SimScreenStack` (new screens), `SimHud` (encumbrance) |
| Tools | — | `coverage_check.py` (the mechanics table), `canon_lint.py` (item weights and categories), `capi_reach.py` (new headers) |

---

## 7. Decisions for the designer (defaults I'll use unless you say otherwise)

| # | Question | Default |
|---|---|---|
| D-1 | Do menus pause the world? | Inventory, character, journal, map, crafting and follower screens **pause**; trade and dialogue **pause**; there's no real-time menu |
| D-2 | Inventory form: a list with weights, or a grid of shapes? | **A list with weights** (it suits a simulation with 60+ item kinds, and it's faster with a controller) |
| D-3 | Carry capacity | 30 kg + 3 kg per point of Strength (33–60 kg) |
| D-4 | Dropped items in public places | Passers-by take them after 1 day, and a rumour starts |
| D-5 | Haggling | Yes, one step |
| D-6 | How many followers are spawned | Companions up to 3, hirelings up to 5, band members up to 8; the rest fight as numbers |
| D-7 | Hotbar or quick-slots | **No**; F and G stay, and the verb wheel covers the rest |
| D-8 | Locks | Keys, permission, or forcing (noisy, a crime); no lockpicking minigame |
| D-9 | Spoiled food | Can be eaten, restores 25%, and carries an illness risk |
| D-10 | Silver has weight | Yes, 8 g per shekel |
| D-11 | Games of chance (astragali) | In, as an activity at the drinking fellowship |
| D-12 | Stolen-flag laundering | Only by selling through a fence, or by returning the item |

## 8. Working beside the city session

- **The city session owns:** `SimEnvironment`, `SimStreetBuilder`, `SimCityData`, `SimMeshKit`, `places.csv`, `building_types.csv`, `city_districts.csv`, art assets.
- **This plan owns:** the kernel, the C API, `UI/`, the player controller, the new actors in §6, `items.csv`, the new canon tables.
- **The seam:** stations, containers, shops, activity spots and seeded goods are **attached by building type** (a new `building_fixtures` mapping). The city builder only has to expose a registry of named anchor points per building, so neither side edits the other's files. P3 and P5 each start with a short sync on that registry.
