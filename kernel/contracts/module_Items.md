# module_Items — item stacks, carrying, goods in the world (P0a)

**Headers:** `sim/Items.hpp` (pure), `sim/WorldItems.hpp` (state), `sim/ItemActions.hpp` (WorldState verbs), `sim/Notices.hpp`, `sim/CApiItems.h`. **Mechanics:** FND-01…06, FND-09, INV-01…08, INV-15, WLD-01, WLD-03…05 in `docs/mechanics-registry.md`.

## Stacks (FND-01)
- An `ItemStack` is item, qty, quality (0 poor, 1 common, 2 fine, 3 masterwork), condition (0–100), owner, stolen, made_day, bound.
- An `Inventory` is always **normalized**: sorted by (item, owner, stolen, bound, quality, made_day, condition), equal kinds merged, no stack with qty ≤ 0. Save bytes are therefore deterministic, and a stack index is stable until the next change.
- Quantities saturate at `INT_MAX` (the C API's `give_item` contract); nothing ever goes negative.
- `take_items` takes the oldest `made_day` first, then the lowest quality: spoiling food is eaten first.
- `owner == ""` means the goods belong to whoever holds them.

## The catalogue (FND-02)
`items.csv` carries `ui_category`, `weight_g`, `stack_max`, `spoil_days`, `flags`; `canon_lint.py` enforces them. Values are ledgered in `docs/proposals/invented-ledger-items.md`.

## Carrying (FND-03)
Capacity = 30 000 g + 3 000 g × Strength (Strength 5 for an unknown actor). Carried = the stacks plus the purse at 8 g a shekel of 180 grains. Tier 0 at ≤ 100%, 1 (burdened) at ≤ 125%, 2 (pinned) above. Nothing picks up past 125%.

## Goods in the world and in containers (FND-04)
- Dropped goods become a `WorldItem` at a place and a position in cm, and stay the dropper's (`owner` set to the dropper unless already someone else's).
- **The ownership rule** (pick up, take out): the rightful owner is the stack's owner, else the container's. If it is someone else, the goods move flagged `stolen` with that owner kept, and **if anyone witnessed it** a theft is filed (`commit_crime`, in the place's city from `places.csv`, else the City of the Moon). Taking your own goods clears both marks.
- Goods put into a container that is not yours stay yours.
- A container has an owner, a kind, an optional capacity in grams (0 = unlimited) and a lock (a locked container refuses; keys and forcing are WLD-06).
- Refusal codes and keys: `kernel/contracts/verb_results.md`. **A refusal changes nothing**, including never creating an inventory for an unseen actor.

## Time and notices (FND-06, FND-09)
- `advance_needs_minutes` carries minutes per actor toward the next whole hour.
- The notice feed keeps the newest 256 notices with sequence numbers that never repeat; `advance_days` posts `notice.raid`, `notice.quest_failed` and `notice.verdict` (the player's) by comparing each day's before and after.

## Save
Inventory rows are 8 fields (a 2-field row from an older save loads as one common, unowned stack). Optional trailing sections, in order: `WORLD_ITEMS_NEXT`, `WORLD_ITEMS`, `CONTAINERS`, `NEEDS_MINUTES`, `NOTICES_NEXT`, `NOTICES`. A save without them loads with nothing on the ground, no minute carry and an empty feed.
