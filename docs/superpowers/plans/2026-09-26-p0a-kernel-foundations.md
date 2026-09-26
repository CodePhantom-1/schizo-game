# P0a — Kernel foundations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the kernel the player's hands: item stacks with ownership, the item catalogue's physical columns, carrying weight, goods lying in the world and in containers, one refusal-reason channel, minute-accurate timed actions, and a player notice feed, all behind the C API and saved.

**Architecture:** A new pure module `sim/Items.hpp` (stacks, catalogue reads) replaces the count map inside `Inventory`; every existing caller moves to its small helper API (`count_of`, `add_items`, `take_items`, `counts`). World-level verbs that touch several modules (carrying, dropping, picking up, containers, theft) live in a caller layer `sim/ItemActions.hpp`, like `Actions.hpp`. New state (`WorldItemsState`, `NoticeState`, the needs minute carry) is saved as optional trailing snapshot sections, so every older save still loads.

**Tech Stack:** C++20, CMake, the zero-dependency `sim/Test.hpp` harness, Python 3 tools (`canon_lint.py`, `capi_reach.py`, `mechanics_registry.py`).

**Spec:** [docs/mechanics-implementation-plan.md](../../mechanics-implementation-plan.md) §1 (FND-01…06, FND-09) and [docs/mechanics-registry.md](../../mechanics-registry.md). P0b (the Unreal verb wheel and UI kit, FND-07/08) is a separate plan.

## Global Constraints

- Integer maths only; no floats in kernel state or rules (D-022).
- Deterministic: no wall clock, no unordered containers in state; same seed and inputs give byte-identical saves.
- No C++ exception may cross the C boundary: every C API function wraps its body and returns its documented error value (the `guard` pattern of `CApiVerbs.cpp`).
- String outputs follow the buffer convention: write at most cap-1 bytes plus NUL, return the untruncated length, -1 on a null argument.
- Old saves load: every new snapshot section is optional on load (the `peek_tag` pattern); the header stays `SIMSAVE 2`.
- Canon rows are never altered in meaning; new columns are added, `CANON` rows keep their tag, and new values are `INVENTED` glue ledgered in `docs/proposals/invented-ledger-items.md` (D-018).
- Every new C API call is cited by a row of `docs/mechanics-registry.md` and has a stage in `tools/capi_reach.py` (CI fails otherwise).
- Every test that proves a mechanic carries the tag `MECH:<id>` in a comment.

## Review Focus

1. **Loading a save written before this change** (count-only inventory rows, no new sections): it must load with the same counts, each as one common, unowned, fresh stack. The test lives in Task 1.
2. **Taking more than you hold, or a quantity of 0 or less** (from `take_items`, the C API `give_item` with a negative quantity, `drop`, `pick_up`): it takes at most what is there, never goes negative, and never leaves a zero stack behind. Tests in Tasks 1 and 6.
3. **Picking up something that isn't yours with nobody watching**: no crime is filed, but the stack is flagged stolen with the rightful owner; the rightful owner picking it back up clears the flag. Test in Task 6.
4. **Picking up more weight than you can carry**: you take the most units that keep you at or below 125% of capacity; zero units gives the `too_heavy` refusal and changes nothing. Test in Task 6.
5. **A notice evicted from the feed**: reading a sequence number older than the kept window returns -2, never a wrong notice. Test in Task 8.

---

## File Structure

| File | Responsibility |
|---|---|
| Create `kernel/include/sim/Items.hpp`, `kernel/src/Items.cpp` | `ItemStack`, `Inventory` (normalized stacks), stack helpers, `ItemDef` catalogue reads, stack weight |
| Create `kernel/include/sim/WorldItems.hpp` | `WorldItem`, `Container`, `WorldItemsState` (data only) |
| Create `kernel/include/sim/Notices.hpp`, `kernel/src/Notices.cpp` | `Notice`, `NoticeState`, `push_notice`, `notice_at` |
| Create `kernel/include/sim/ItemActions.hpp`, `kernel/src/ItemActions.cpp` | carrying (capacity, carried, encumbrance) and the world-item and container verbs, including theft |
| Create `kernel/include/sim/CApiItems.h`, `kernel/src/CApiItems.cpp` | the C API for all of the above plus `last_reason`, `advance_minutes`, notices |
| Create `kernel/tests/test_items.cpp`, `test_item_actions.cpp`, `test_capi_items.cpp`, `test_notices.cpp` | tests |
| Create `kernel/contracts/module_Items.md`, `kernel/contracts/verb_results.md` | contracts |
| Create `docs/proposals/invented-ledger-items.md` | the invented weights and constants |
| Modify `kernel/include/sim/Crafting.hpp` | `Inventory` moves to Items.hpp (Crafting includes it) |
| Modify `kernel/src/{Crafting,CApi,CApiVerbs,CombatActions,Progression,Rites,WildWorld,WildTravel,Snapshot,World}.cpp` | callers of the old count map |
| Modify `kernel/include/sim/{World,Needs}.hpp`, `kernel/src/Needs.cpp`, `kernel/src/CApiInternal.hpp` | new state, minute carry, `last_reason` |
| Modify `kernel/include/sim/CApi.h` | includes `CApiItems.h` |
| Modify tests that touch `.counts` | move to the helpers |
| Modify `db/canon/items.csv`, `db/schema/items.md`, `tools/canon_lint.py`, `unreal/Content/Sim/canon/items.csv` (staged copy) | catalogue columns and their lint |
| Modify `tools/capi_reach.py`, `docs/mechanics-registry.md`, `docs/capi-reach.md` | stages, statuses, citations |

---

### Task 1: Item stacks replace the count map (FND-01, INV-15)

**Files:**
- Create: `kernel/include/sim/Items.hpp`, `kernel/src/Items.cpp`, `kernel/tests/test_items.cpp`
- Modify: `kernel/include/sim/Crafting.hpp:29-31` (remove `struct Inventory`, include Items.hpp), and every `.counts` caller listed in File Structure
- Modify: `kernel/src/Snapshot.cpp` INVENTORIES writer (~line 329) and reader (~line 766)

**Interfaces:**
- Produces:
  - `struct ItemStack { Id item; int qty; int quality; int condition; Id owner; bool stolen; DayNumber made_day; bool bound; bool same_kind(const ItemStack&) const; }`
  - `struct Inventory { std::vector<ItemStack> stacks; }` — always normalized: sorted by (item, owner, stolen, bound, quality, made_day, condition), equal kinds merged, no stack with qty ≤ 0
  - `int count_of(const Inventory&, const Id& item)`
  - `std::map<Id,int> counts(const Inventory&)`
  - `int add_stack(Inventory&, ItemStack)` → count of that item after
  - `int add_items(Inventory&, const Id& item, int qty)` → count after (a common, unowned, day-0 stack)
  - `std::vector<ItemStack> take_items(Inventory&, const Id& item, int qty)` → the units taken, oldest `made_day` first, then lowest quality, then stack order
  - `std::optional<ItemStack> take_from_stack(Inventory&, std::size_t index, int qty)`
  - `void normalize(Inventory&)`
  - constants `kQualityPoor=0, kQualityCommon=1, kQualityFine=2, kQualityMasterwork=3`, `kMaxStackQty = 1000000000`

- [ ] **Step 1: Write the failing test** `kernel/tests/test_items.cpp`

```cpp
// test_items.cpp — FND-01: item stacks. MECH:FND-01 MECH:INV-01 MECH:INV-06 MECH:INV-15
#include "sim/Items.hpp"
#include "sim/Snapshot.hpp"
#include "sim/World.hpp"
#include "sim/Test.hpp"

using namespace sim;

static bool test_add_merges_and_counts() {
    Inventory inv;
    SIM_CHECK_EQ(add_items(inv, "bread", 2), 2);
    SIM_CHECK_EQ(add_items(inv, "bread", 3), 5);
    SIM_CHECK_EQ(inv.stacks.size(), 1u);
    ItemStack stolen{"bread", 1};
    stolen.owner = "ea_nasir";
    stolen.stolen = true;
    SIM_CHECK_EQ(add_stack(inv, stolen), 6);
    SIM_CHECK_EQ(inv.stacks.size(), 2u);  // a different owner never merges
    SIM_CHECK_EQ(count_of(inv, "bread"), 6);
    SIM_CHECK_EQ(count_of(inv, "beer"), 0);
    SIM_CHECK_EQ(counts(inv).at("bread"), 6);
    return true;
}

static bool test_zero_and_negative_are_no_ops() {
    Inventory inv;
    SIM_CHECK_EQ(add_items(inv, "bread", 0), 0);
    SIM_CHECK_EQ(add_items(inv, "bread", -4), 0);
    SIM_CHECK(inv.stacks.empty());
    SIM_CHECK(take_items(inv, "bread", 3).empty());
    add_items(inv, "bread", 2);
    SIM_CHECK(take_items(inv, "bread", 0).empty());
    SIM_CHECK(take_items(inv, "bread", -1).empty());
    SIM_CHECK_EQ(count_of(inv, "bread"), 2);
    return true;
}

static bool test_take_never_overdraws_and_takes_oldest_first() {
    Inventory inv;
    ItemStack old{"fish", 2};
    old.made_day = 3;
    ItemStack fresh{"fish", 2};
    fresh.made_day = 9;
    add_stack(inv, fresh);
    add_stack(inv, old);
    std::vector<ItemStack> got = take_items(inv, "fish", 3);
    SIM_CHECK_EQ(got.size(), 2u);
    SIM_CHECK_EQ(got[0].made_day, 3);
    SIM_CHECK_EQ(got[0].qty, 2);
    SIM_CHECK_EQ(got[1].qty, 1);
    SIM_CHECK_EQ(count_of(inv, "fish"), 1);
    got = take_items(inv, "fish", 10);  // more than held: takes what is there
    SIM_CHECK_EQ(got.size(), 1u);
    SIM_CHECK_EQ(got[0].qty, 1);
    SIM_CHECK(inv.stacks.empty());      // no zero stack left behind
    return true;
}

static bool test_split_a_stack() {
    Inventory inv;
    add_items(inv, "grain", 5);
    std::optional<ItemStack> part = take_from_stack(inv, 0, 2);
    SIM_CHECK(part.has_value());
    SIM_CHECK_EQ(part->qty, 2);
    SIM_CHECK_EQ(count_of(inv, "grain"), 3);
    SIM_CHECK(!take_from_stack(inv, 7, 1).has_value());   // no such stack
    SIM_CHECK(!take_from_stack(inv, 0, 0).has_value());   // nothing to take
    part = take_from_stack(inv, 0, 99);                    // clamps to the stack
    SIM_CHECK_EQ(part->qty, 3);
    SIM_CHECK(inv.stacks.empty());
    return true;
}

static bool test_saturates_instead_of_overflowing() {
    Inventory inv;
    add_items(inv, "grain", kMaxStackQty);
    SIM_CHECK_EQ(add_items(inv, "grain", 5), kMaxStackQty);
    return true;
}

static bool test_snapshot_round_trip_keeps_every_field() {
    WorldState w;
    w.init("../db/canon", 7);
    ItemStack s{"copper_dagger", 1};
    s.quality = kQualityFine;
    s.condition = 61;
    s.owner = "nur_ea_coppersmith";
    s.stolen = true;
    s.made_day = 4;
    s.bound = true;
    add_stack(w.inventories["player"], s);
    add_items(w.inventories["player"], "bread", 3);
    const std::string a = save_world(w);
    WorldState back;
    load_world(back, "../db/canon", a);
    SIM_CHECK(back.inventories["player"].stacks == w.inventories["player"].stacks);
    SIM_CHECK_EQ(save_world(back), a);
    return true;
}

static bool test_legacy_count_rows_still_load() {
    WorldState w;
    w.init("../db/canon", 7);
    add_items(w.inventories["player"], "bread", 3);
    std::string data = save_world(w);
    // Rewrite the new 8-field row as the pre-P0a 2-field row "bread\t3".
    const std::string row = "bread\t3\t1\t100\t\t0\t0\t0\n";
    const std::size_t at = data.find(row);
    SIM_CHECK(at != std::string::npos);
    data.replace(at, row.size(), "bread\t3\n");
    WorldState back;
    load_world(back, "../db/canon", data);
    SIM_CHECK_EQ(count_of(back.inventories["player"], "bread"), 3);
    SIM_CHECK_EQ(back.inventories["player"].stacks[0].quality, kQualityCommon);
    SIM_CHECK(back.inventories["player"].stacks[0].owner.empty());
    return true;
}

SIM_MAIN(test_add_merges_and_counts, test_zero_and_negative_are_no_ops,
         test_take_never_overdraws_and_takes_oldest_first, test_split_a_stack,
         test_saturates_instead_of_overflowing, test_snapshot_round_trip_keeps_every_field,
         test_legacy_count_rows_still_load)
```

(Check the exact names `save_world` / `load_world` in `kernel/include/sim/Snapshot.hpp` before running; use whatever it declares.)

- [ ] **Step 2: Run it to see it fail**

Run: `cmake --build kernel/build -j16 2>&1 | grep -m3 error`
Expected: `fatal error: sim/Items.hpp: No such file or directory`

- [ ] **Step 3: Write `Items.hpp` and `Items.cpp`**

```cpp
// kernel/include/sim/Items.hpp
#pragma once
// Items.hpp — FND-01/02: item stacks and the item catalogue's physical columns.
// Pure: no WorldState. An Inventory is always normalized (sorted by kind,
// equal kinds merged, no empty stack), so save bytes are deterministic.
// Contract: kernel/contracts/module_Items.md.
#include "sim/Db.hpp"
#include "sim/Types.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace sim {

constexpr int kQualityPoor = 0;
constexpr int kQualityCommon = 1;
constexpr int kQualityFine = 2;
constexpr int kQualityMasterwork = 3;
constexpr int kMaxStackQty = 1000000000;  // saturation cap (no int overflow)

struct ItemStack {
    Id item;
    int qty = 0;
    int quality = kQualityCommon;
    int condition = 100;     // 0 broken .. 100 new
    Id owner;                // "" = belongs to whoever holds it; else the rightful owner
    bool stolen = false;     // taken without the owner's leave
    DayNumber made_day = 0;  // freshness clock (spoilage reads it)
    bool bound = false;      // quest item: cannot be dropped or sold
    bool same_kind(const ItemStack& o) const {
        return item == o.item && quality == o.quality && condition == o.condition &&
               owner == o.owner && stolen == o.stolen && made_day == o.made_day && bound == o.bound;
    }
    bool operator==(const ItemStack&) const = default;
};

struct Inventory {
    std::vector<ItemStack> stacks;
};

void normalize(Inventory& inv);
int count_of(const Inventory& inv, const Id& item);
std::map<Id, int> counts(const Inventory& inv);
int add_stack(Inventory& inv, ItemStack s);
int add_items(Inventory& inv, const Id& item, int qty);
std::vector<ItemStack> take_items(Inventory& inv, const Id& item, int qty);
std::optional<ItemStack> take_from_stack(Inventory& inv, std::size_t index, int qty);

}  // namespace sim
```

```cpp
// kernel/src/Items.cpp
#include "sim/Items.hpp"

#include <algorithm>
#include <tuple>

namespace sim {
namespace {

auto key(const ItemStack& s) {
    return std::tie(s.item, s.owner, s.stolen, s.bound, s.quality, s.made_day, s.condition);
}

}  // namespace

void normalize(Inventory& inv) {
    std::vector<ItemStack>& v = inv.stacks;
    std::stable_sort(v.begin(), v.end(), [](const ItemStack& a, const ItemStack& b) { return key(a) < key(b); });
    std::vector<ItemStack> out;
    out.reserve(v.size());
    for (ItemStack& s : v) {
        if (s.qty <= 0) continue;
        if (!out.empty() && out.back().same_kind(s)) {
            const long long sum = static_cast<long long>(out.back().qty) + s.qty;
            out.back().qty = static_cast<int>(std::min<long long>(sum, kMaxStackQty));
        } else {
            out.push_back(std::move(s));
        }
    }
    v = std::move(out);
}

int count_of(const Inventory& inv, const Id& item) {
    long long n = 0;
    for (const ItemStack& s : inv.stacks)
        if (s.item == item) n += s.qty;
    return static_cast<int>(std::min<long long>(n, kMaxStackQty));
}

std::map<Id, int> counts(const Inventory& inv) {
    std::map<Id, int> out;
    for (const ItemStack& s : inv.stacks) {
        const long long sum = static_cast<long long>(out[s.item]) + s.qty;
        out[s.item] = static_cast<int>(std::min<long long>(sum, kMaxStackQty));
    }
    return out;
}

int add_stack(Inventory& inv, ItemStack s) {
    if (s.qty <= 0 || s.item.empty()) return count_of(inv, s.item);
    const Id item = s.item;
    inv.stacks.push_back(std::move(s));
    normalize(inv);
    return count_of(inv, item);
}

int add_items(Inventory& inv, const Id& item, int qty) {
    ItemStack s;
    s.item = item;
    s.qty = qty;
    return add_stack(inv, std::move(s));
}

std::vector<ItemStack> take_items(Inventory& inv, const Id& item, int qty) {
    std::vector<ItemStack> taken;
    if (qty <= 0) return taken;
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < inv.stacks.size(); ++i)
        if (inv.stacks[i].item == item) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const ItemStack& x = inv.stacks[a];
        const ItemStack& y = inv.stacks[b];
        return std::tie(x.made_day, x.quality) < std::tie(y.made_day, y.quality);
    });
    int left = qty;
    for (std::size_t i : order) {
        if (left == 0) break;
        ItemStack& s = inv.stacks[i];
        const int n = std::min(left, s.qty);
        ItemStack part = s;
        part.qty = n;
        s.qty -= n;
        left -= n;
        taken.push_back(std::move(part));
    }
    normalize(inv);
    return taken;
}

std::optional<ItemStack> take_from_stack(Inventory& inv, std::size_t index, int qty) {
    if (index >= inv.stacks.size() || qty <= 0) return std::nullopt;
    ItemStack& s = inv.stacks[index];
    ItemStack part = s;
    part.qty = std::min(qty, s.qty);
    s.qty -= part.qty;
    normalize(inv);
    return part;
}

}  // namespace sim
```

- [ ] **Step 4: Move `Inventory` out of Crafting.hpp**

In `kernel/include/sim/Crafting.hpp` delete lines 29-31 (`struct Inventory { std::map<Id, int> counts; };`) and add `#include "sim/Items.hpp"` under the existing includes. Keep the comment block above it but reword "An Inventory is a bag of item counts" to "An Inventory is a normalized list of item stacks (sim/Items.hpp)".

- [ ] **Step 5: Move every caller to the helpers** (the compiler lists each one: `cmake --build kernel/build 2>&1 | grep error`)

| File | Old | New |
|---|---|---|
| `src/Crafting.cpp` check | `inv.counts.find(item)` … `have` | `const long long have = count_of(inv, item);` |
| `src/Crafting.cpp` craft | `inv.counts[item] -= qty * times;` / `+=` | `take_items(inv, item, qty * times);` / `add_items(inv, item, qty * times);` |
| `src/CApi.cpp` eat, drink | `…counts[item] = have - 1;` | `take_items(world->world.inventories[actor], Id(item), 1);` |
| `src/CApi.cpp` item_count | map lookup | `return count_of(ait->second, Id(item));` |
| `src/CApi.cpp` give_item | clamp arithmetic on `counts[item]` | `if (qty > 0) add_items(inv, Id(item), qty); else take_items(inv, Id(item), -qty); return count_of(inv, Id(item));` (with `qty == INT_MIN` treated as `-INT_MAX`) |
| `src/CApiVerbs.cpp` best_held, inventory | `for (… : ait->second.counts)` | `for (const auto& [item, count] : counts(ait->second))` |
| `src/CombatActions.cpp` held / give | map lookup / `int& n` | `count_of` / `if (qty > 0) add_items(...); else take_items(..., -qty);` |
| `src/CombatActions.cpp` loot_body | copies `counts`, `give`, `clear()` | move whole stacks: `for (const ItemStack& s : inv->second.stacks) { … add_stack(w.inventories[looter], s); moved += s.qty; } inv->second.stacks.clear();` (the durability copy keeps using `held(w, looter, s.item)`) |
| `src/Progression.cpp` held, wage, buy, sell | `counts[...]` arithmetic | `count_of`, `add_items`, `add_items`, `take_items` |
| `src/Rites.cpp` held, spend | lookup, `--it->second` | `count_of(inv->second, item)`; `if (take_items(inv, k, 1).empty()) continue;` |
| `src/WildWorld.cpp` player_defeated | `count -= count / 3` over the map | `for (const auto& [item, count] : counts(inv)) take_items(inv, item, count / 3);` |
| `src/WildWorld.cpp` give_player | `counts[item] += units` | `add_items(w.inventories[kPlayer], item, units);` |
| `src/WildTravel.cpp` give food, requires_item, water, eat | lookups and decrements | `count_of(...) > 0` checks and `take_items(inv, x, 1)`; for the hunger loop iterate a copy `for (const auto& [item, count] : counts(inv))` and `take_items(inv, item, 1)` after a successful `eat` |

- [ ] **Step 6: Snapshot: write stacks, read both widths**

Writer (replace the INVENTORIES block):

```cpp
    // INVENTORIES (W2-I; P0a: one row per stack, 8 fields)
    {
        wr.line({"INVENTORIES", s64(static_cast<std::int64_t>(w.inventories.size()))});
        for (const auto& [actor, inv] : w.inventories) {
            wr.line({actor, s64(static_cast<std::int64_t>(inv.stacks.size()))});
            for (const ItemStack& s : inv.stacks)
                wr.line({s.item, s64(s.qty), s64(s.quality), s64(s.condition), s.owner,
                         sbool(s.stolen), s64(s.made_day), sbool(s.bound)});
        }
    }
```

Reader (replace the inner item loop):

```cpp
                for (std::size_t j = 0; j < itemcount; ++j) {
                    std::vector<std::string> itf = rd.next();
                    if (itf.size() == 2) {  // a pre-P0a save: item, count
                        add_items(inv, itf[0], static_cast<int>(Reader::parse_i64(itf[1])));
                        continue;
                    }
                    if (itf.size() != 8) throw std::runtime_error("snapshot: bad inventory item row");
                    ItemStack s;
                    s.item = itf[0];
                    s.qty = static_cast<int>(Reader::parse_i64(itf[1]));
                    s.quality = static_cast<int>(Reader::parse_i64(itf[2]));
                    s.condition = static_cast<int>(Reader::parse_i64(itf[3]));
                    s.owner = itf[4];
                    s.stolen = itf[5] == "1";
                    s.made_day = Reader::parse_i64(itf[6]);
                    s.bound = itf[7] == "1";
                    add_stack(inv, std::move(s));
                }
```

- [ ] **Step 7: Move the existing tests off `.counts`**

`grep -rn "\.counts" kernel/tests` lists them (test_crafting, test_scenario_rite, test_divine, test_scenario_duel, test_character, test_wild and a few one-offs). Replace reads (`inv.counts["x"]`, `.counts.at("x")`, `.counts.count("x")`) with `count_of(inv, "x")` (and `count_of(...) > 0` for `.count`), and writes (`inv.counts["x"] = n` on an empty inventory, `+= n`) with `add_items(inv, "x", n)`. Where a test sets a count to a lower value on a non-empty inventory, write `take_items(inv, "x", count_of(inv, "x") - n)`. Do not change what any test asserts.

- [ ] **Step 8: Build and run everything**

Run: `cmake --build kernel/build -j16 2>&1 | grep -E "error|warning: unused" ; ctest --test-dir kernel/build 2>&1 | tail -3`
Expected: `100% tests passed, 0 tests failed out of 43`

- [ ] **Step 9: Commit**

```bash
git add kernel
git commit -m "P0a T1: item stacks replace the count map (FND-01) — owner, stolen, quality, condition, freshness, bound; old saves still load"
```

---

### Task 2: The item catalogue's physical columns (FND-02)

**Files:**
- Modify: `db/canon/items.csv` (5 new columns at the end, before `tag`? No: **append after `source_ref`** so no existing column index moves: `ui_category,weight_g,stack_max,spoil_days,flags`)
- Modify: `tools/canon_lint.py` (item column checks), `db/schema/items.md`, the staged copy via `python3 tools/stage_canon_for_ue.py`
- Modify: `kernel/include/sim/Items.hpp`, `kernel/src/Items.cpp` (`ItemDef`, `item_def`, `stack_weight_g`)
- Create: `docs/proposals/invented-ledger-items.md`
- Test: `kernel/tests/test_items.cpp` (add), `tools/tests/test_canon_lint_items.py`

**Interfaces:**
- Produces: `struct ItemDef { Id id; std::string ui_category; int weight_g; int stack_max; int spoil_days; std::vector<std::string> flags; bool has_flag(const std::string&) const; }`, `std::optional<ItemDef> item_def(const Db&, const Id&)`, `long long stack_weight_g(const Db&, const ItemStack&)`, `const std::vector<std::string>& ui_categories()`

**Values** (INVENTED, ledgered; weights in grams per unit, from the object's real size): `ui_category` is one of `food, drink, ingredient, material, tool, weapon, ammunition, armour, shield, clothing, ritual, document, trade_good, station_part, animal, medicine, treasure`. Stations (quern, oven, brewing_vat, potters_wheel, loom, smithing_hearth) are `station_part` with their real weights (a quern ~25 000 g) and the flag `fixture` (not normally carried). `pack_donkey` is `animal`, flag `animal`. Documents (`underworld_ritual_scroll`, `zisurru_incantation_tablet`, `clay_liver_model`) carry `document`. Food spoil days: fish 1, bread 3, beer 5, a burned goat's liver 1, dates 60, flour 90, grain and the rest 0 (never). `water` is `drink`, flag `virtual` (never held as a stack weight: 0 g). Every weapon/armour row copies its `arms.csv` `weight_g`.

- [ ] **Step 1: Failing lint test** `tools/tests/test_canon_lint_items.py`

```python
import pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import canon_lint


class ItemColumns(unittest.TestCase):
    def test_bad_values_are_errors(self):
        row = {"id": "x", "ui_category": "snack", "weight_g": "-1", "stack_max": "0",
               "spoil_days": "abc", "flags": "document|nonsense"}
        errs = canon_lint.item_row_errors("items.csv:2", row)
        self.assertEqual(len(errs), 5)

    def test_good_row_passes(self):
        row = {"id": "bread", "ui_category": "food", "weight_g": "250", "stack_max": "20",
               "spoil_days": "3", "flags": ""}
        self.assertEqual(canon_lint.item_row_errors("items.csv:2", row), [])

    def test_real_items_pass(self):
        self.assertEqual(canon_lint.main(), 0)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2:** Run `python3 -m unittest tools/tests/test_canon_lint_items.py` — Expected: `AttributeError: module 'canon_lint' has no attribute 'item_row_errors'`

- [ ] **Step 3: Lint the columns** — add to `tools/canon_lint.py`:

```python
UI_CATEGORIES = {"food", "drink", "ingredient", "material", "tool", "weapon", "ammunition",
                 "armour", "shield", "clothing", "ritual", "document", "trade_good",
                 "station_part", "animal", "medicine", "treasure"}
ITEM_FLAGS = {"bound", "document", "floats", "fixture", "animal", "virtual", "container"}


def item_row_errors(where: str, row: dict) -> list[str]:
    """FND-02: the physical columns every item row must carry."""
    errs = []
    rid = row.get("id", "")
    if row.get("ui_category") not in UI_CATEGORIES:
        errs.append(f"{where}: item '{rid}' ui_category '{row.get('ui_category')}' not in {sorted(UI_CATEGORIES)}")
    for col, lo in (("weight_g", 0), ("stack_max", 1), ("spoil_days", 0)):
        v = row.get(col) or ""
        if not v.isdigit() or int(v) < lo:
            errs.append(f"{where}: item '{rid}' {col} '{v}' must be an integer >= {lo}")
    for flag in filter(None, (row.get("flags") or "").split("|")):
        if flag.split(":")[0] not in ITEM_FLAGS:
            errs.append(f"{where}: item '{rid}' unknown flag '{flag}'")
    return errs
```

and in `main()`, inside the row loop after the tag checks: `if table.name == "items.csv" and tag != "OPEN": errors.extend(item_row_errors(where, row))`.

- [ ] **Step 4: Fill the 66 rows** — append the five columns to every row of `db/canon/items.csv` with the values rule above (a script is fine, but check each weight by hand against the ledger table). Write `docs/proposals/invented-ledger-items.md` listing each value and why. Update `db/schema/items.md`'s field line. Run `python3 tools/stage_canon_for_ue.py`.

- [ ] **Step 5: Kernel reads** — in Items.hpp:

```cpp
struct ItemDef {
    Id id;
    std::string ui_category;
    int weight_g = 0;
    int stack_max = 1;
    int spoil_days = 0;          // 0 = never spoils
    std::vector<std::string> flags;
    bool has_flag(const std::string& f) const;
};
std::optional<ItemDef> item_def(const Db& db, const Id& item);  // nullopt: unknown or OPEN
long long stack_weight_g(const Db& db, const ItemStack& s);     // unknown items weigh 0
```

Items.cpp: parse with a local `to_int(s, fallback)` (digits only, else fallback), split flags on `|`, `has_flag` matches the part before `:`. `stack_weight_g` = `weight_g * qty` (0 for unknown or `virtual`).

Test (add to test_items.cpp, `MECH:FND-02`):

```cpp
static bool test_item_defs_from_canon() {
    WorldState w;
    w.init("../db/canon", 1);
    const std::optional<ItemDef> bread = item_def(w.db, "bread");
    SIM_CHECK(bread.has_value());
    SIM_CHECK_EQ(bread->ui_category, std::string("food"));
    SIM_CHECK(bread->weight_g > 0);
    SIM_CHECK(bread->spoil_days > 0);
    SIM_CHECK(item_def(w.db, "copper_dagger")->weight_g == 300);  // arms.csv weight
    SIM_CHECK(item_def(w.db, "clay_liver_model")->has_flag("document"));
    SIM_CHECK(!item_def(w.db, "no_such_item").has_value());
    ItemStack s{"bread", 4};
    SIM_CHECK_EQ(stack_weight_g(w.db, s), 4LL * bread->weight_g);
    SIM_CHECK_EQ(stack_weight_g(w.db, ItemStack{"water", 9}), 0);
    return true;
}
```

- [ ] **Step 6:** Run `python3 tools/canon_lint.py && python3 -m unittest tools/tests/test_canon_lint_items.py && ctest --test-dir kernel/build -R test_items` — all pass. Add the lint test to `.github/workflows/ci.yml`'s canon_lint step: `run: python3 tools/canon_lint.py && python3 -m unittest tools/tests/test_canon_lint_items.py`.

- [ ] **Step 7: Commit** — `git commit -m "P0a T2: items carry ui_category, weight, stack size, spoil days and flags (FND-02), linted"`

---

### Task 3: One refusal-reason channel (FND-05)

**Files:**
- Modify: `kernel/src/CApiInternal.hpp` (add `std::string last_reason;` to `SimWorld`)
- Create: `kernel/include/sim/CApiItems.h`, `kernel/src/CApiItems.cpp` (first call), `kernel/contracts/verb_results.md`
- Modify: `kernel/include/sim/CApi.h` (`#include "sim/CApiItems.h"` beside CApiVerbs.h)
- Test: `kernel/tests/test_capi_items.cpp`

**Interfaces:**
- Produces: `int sim_world_last_reason(const SimWorld*, char* out, int cap)` — the string-table key of why the last CApiItems verb refused (`""` after a success); buffer convention. In C++: `void set_reason(SimWorld*, const std::string&)` (file-local in CApiItems.cpp).
- The keys (verb_results.md): `refuse.bad_argument` (-1), `refuse.unknown` (-2), `refuse.bound` (-3), `refuse.too_heavy` (-4), `refuse.full` (-5), `refuse.locked` (-6). Every CApiItems verb uses exactly these codes and keys.

- [ ] **Step 1: Failing test** (`test_capi_items.cpp`, `MECH:FND-05`):

```cpp
// test_capi_items.cpp — the CApiItems surface. MECH:FND-05
#include "sim/CApi.h"
#include "sim/Test.hpp"

#include <cstring>
#include <string>

static bool test_last_reason_starts_empty_and_null_is_minus_one() {
    SimWorld* w = sim_world_create("../db/canon", 3);
    char buf[64] = "x";
    SIM_CHECK_EQ(sim_world_last_reason(w, buf, sizeof buf), 0);
    SIM_CHECK_EQ(std::string(buf), std::string(""));
    SIM_CHECK_EQ(sim_world_last_reason(nullptr, buf, sizeof buf), -1);
    SIM_CHECK_EQ(sim_world_last_reason(w, nullptr, 0), -1);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_last_reason_starts_empty_and_null_is_minus_one)
```

- [ ] **Step 2:** build — Expected: `'sim_world_last_reason' was not declared`
- [ ] **Step 3:** implement: header with the conventions comment (copy the CApiVerbs.h header style); `CApiItems.cpp` with the `write_req`/`guard` helpers (same as CApiVerbs.cpp) and:

```cpp
int sim_world_last_reason(const SimWorld* world, char* out, int cap) {
    return guard([&] {
        if (world == nullptr) return -1;
        return write_req(out, cap, world->last_reason);
    });
}
```

Write `verb_results.md`: the code table above, the rule "a CApiItems verb sets `last_reason` to its key on refusal and to `""` on success", and "the engine shows the key's string-table text; codes stay stable forever".
- [ ] **Step 4:** run `ctest --test-dir kernel/build -R capi_items` — PASS
- [ ] **Step 5: Commit** — `"P0a T3: one refusal-reason channel for the item verbs (FND-05)"`

---

### Task 4: Carrying weight and encumbrance (FND-03, BOD-19, INV-02)

**Files:**
- Create: `kernel/include/sim/ItemActions.hpp`, `kernel/src/ItemActions.cpp`, `kernel/tests/test_item_actions.cpp`
- Modify: `kernel/src/CApiItems.cpp`, `kernel/include/sim/CApiItems.h`, `kernel/tests/test_capi_items.cpp`

**Interfaces:**
- Consumes: `actor_attribute(const WorldState&, const Id& actor, const Id& attr)` (Progression.hpp; -1 unknown actor), `purse(const PropertyState&, const Id&)`, `stack_weight_g`.
- Produces (ItemActions.hpp):
  - `constexpr int kBaseCarryG = 30000; constexpr int kCarryPerStrengthG = 3000; constexpr int kSilverGPerShekel = 8; constexpr int kGrainsPerShekel = 180; constexpr int kBurdenedPct = 100; constexpr int kPinnedPct = 125;`
  - `long long capacity_g(const WorldState&, const Id& actor)` — unknown actor: strength 5
  - `long long carried_g(const WorldState&, const Id& actor)` — stacks plus `purse * kSilverGPerShekel / kGrainsPerShekel`
  - `int encumbrance(const WorldState&, const Id& actor)` — 0 normal (≤100%), 1 burdened (≤125%), 2 pinned (>125%)
- C API: `int64_t sim_world_capacity_g(const SimWorld*, const char* actor)`, `int64_t sim_world_carried_g(...)`, `int sim_world_encumbrance(...)` (-1 on null)

- [ ] **Step 1: Failing test** (`test_item_actions.cpp`, `MECH:FND-03 MECH:BOD-19 MECH:INV-02`):

```cpp
static bool test_capacity_and_tiers() {
    WorldState w;
    w.init("../db/canon", 5);
    const int str = actor_attribute(w, "player", "strength");
    SIM_CHECK_EQ(capacity_g(w, "player"), kBaseCarryG + kCarryPerStrengthG * str);
    SIM_CHECK_EQ(carried_g(w, "player"), 0);
    SIM_CHECK_EQ(encumbrance(w, "player"), 0);
    const long long cap = capacity_g(w, "player");
    const int unit = item_def(w.db, "copper_ingots")->weight_g;
    add_items(w.inventories["player"], "copper_ingots", static_cast<int>(cap / unit));
    SIM_CHECK(carried_g(w, "player") <= cap);
    SIM_CHECK_EQ(encumbrance(w, "player"), 0);                    // exactly at or under 100%
    add_items(w.inventories["player"], "copper_ingots", 1);
    SIM_CHECK_EQ(encumbrance(w, "player"), carried_g(w, "player") > cap ? 1 : 0);
    add_items(w.inventories["player"], "copper_ingots", static_cast<int>(cap / unit));
    SIM_CHECK_EQ(encumbrance(w, "player"), 2);                    // well past 125%
    return true;
}

static bool test_silver_has_weight() {
    WorldState w;
    w.init("../db/canon", 5);
    credit_purse(w.property, "player", kGrainsPerShekel * 10);    // ten shekels
    SIM_CHECK_EQ(carried_g(w, "player"), 10LL * kSilverGPerShekel);
    return true;
}
```

(The boundary test: pick an item whose weight divides evenly so `cap / unit * unit == cap` gives an exact 100%; if copper ingots do not divide it, fill with `unit` items then check `carried == cap` using 1-gram `water`-free material, or assert the tier from the computed percentage as shown.)

- [ ] **Step 2:** build — fails on the missing header
- [ ] **Step 3:** implement:

```cpp
long long capacity_g(const WorldState& w, const Id& actor) {
    int str = actor_attribute(w, actor, "strength");
    if (str < 1) str = kAttrStart;
    return kBaseCarryG + static_cast<long long>(kCarryPerStrengthG) * str;
}

long long carried_g(const WorldState& w, const Id& actor) {
    long long g = 0;
    if (const auto it = w.inventories.find(actor); it != w.inventories.end())
        for (const ItemStack& s : it->second.stacks) g += stack_weight_g(w.db, s);
    return g + purse(w.property, actor) * kSilverGPerShekel / kGrainsPerShekel;
}

int encumbrance(const WorldState& w, const Id& actor) {
    const long long cap = capacity_g(w, actor), have = carried_g(w, actor);
    if (have * 100 <= cap * kBurdenedPct) return 0;
    if (have * 100 <= cap * kPinnedPct) return 1;
    return 2;
}
```

C API wrappers: null → -1, else the value.
- [ ] **Step 4:** tests pass; add a C API test that `sim_world_capacity_g(w, "player") > 0` and `sim_world_encumbrance(nullptr, "player") == -1`
- [ ] **Step 5: Commit** — `"P0a T4: carrying weight, silver's weight and the encumbrance tiers (FND-03)"`

---

### Task 5: Reading stacks and item definitions from the engine (INV-03, INV-04)

**Files:** Modify `CApiItems.h/.cpp`, `test_capi_items.cpp`

**Interfaces:**
- `int sim_world_stack_count(const SimWorld*, const char* actor)` — -1 null; 0 for an unseen actor
- `int sim_world_stack_at(const SimWorld*, const char* actor, int index, char* out, int cap)` — `"item;qty;quality;condition;owner;stolen;made_day;bound"`; -2 bad index; buffer convention
- `int sim_world_item_def(const SimWorld*, const char* item, char* out, int cap)` — `"ui_category;weight_g;stack_max;spoil_days;flag|flag"`; -2 unknown item

- [ ] **Step 1: Failing test** (`MECH:INV-03 MECH:INV-04`):

```cpp
static bool test_stacks_and_defs_read_back() {
    SimWorld* w = sim_world_create("../db/canon", 3);
    sim_world_give_item(w, "player", "bread", 3);
    SIM_CHECK_EQ(sim_world_stack_count(w, "player"), 1);
    SIM_CHECK_EQ(sim_world_stack_count(w, "nobody"), 0);
    char buf[256];
    SIM_CHECK(sim_world_stack_at(w, "player", 0, buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("bread;3;1;100;;0;0;0"));
    SIM_CHECK_EQ(sim_world_stack_at(w, "player", 1, buf, sizeof buf), -2);
    SIM_CHECK_EQ(sim_world_stack_at(w, "player", -1, buf, sizeof buf), -2);
    SIM_CHECK(sim_world_item_def(w, "bread", buf, sizeof buf) > 0);
    SIM_CHECK(std::string(buf).rfind("food;", 0) == 0);
    SIM_CHECK_EQ(sim_world_item_def(w, "nothing", buf, sizeof buf), -2);
    sim_world_destroy(w);
    return true;
}
```

- [ ] **Step 2–4:** implement with a shared `std::string stack_record(const ItemStack&)` (fields joined by `;`, bools as 0/1) — it is reused by Task 6's world-item and container records. Run: PASS.
- [ ] **Step 5: Commit** — `"P0a T5: the engine can list stacks and read item definitions (INV-03, INV-04)"`

---

### Task 6: Goods in the world and in containers, with theft (FND-04, WLD-01, WLD-03, WLD-04, WLD-05, INV-07)

**Files:**
- Create: `kernel/include/sim/WorldItems.hpp`
- Modify: `kernel/include/sim/World.hpp` (`WorldItemsState world_items;`), `kernel/src/World.cpp` init (reset it), `ItemActions.hpp/.cpp`, `CApiItems.h/.cpp`, `Snapshot.cpp` (sections `WORLD_ITEMS_NEXT`, `WORLD_ITEMS`, `CONTAINERS`), tests

**Interfaces:**
- `WorldItems.hpp`:

```cpp
struct WorldItem {
    Id id;                 // "witem_<n>"
    ItemStack stack;
    Id place;              // places.csv id (or a wild place)
    int x_cm = 0, y_cm = 0, z_cm = 0;
    DayNumber dropped_day = 0;
    Id dropped_by;
};
struct Container {
    Id id;                 // chosen by whoever places it (building seeding, the engine)
    Id place;
    std::string kind;      // jar | chest | basket | granary | stall | body
    Id owner;              // "" = anyone's
    bool locked = false;   // locks are WLD-06 (P1); a locked container refuses today
    Id key_item;
    int capacity_g = 0;    // 0 = no limit
    Inventory contents;
};
struct WorldItemsState {
    std::vector<WorldItem> items;          // in id order of creation
    std::map<Id, Container> containers;
    std::int64_t next_id = 1;
};
```

- `ItemActions.hpp`:

```cpp
struct ItemResult {
    int code = 0;          // >= 0 units moved; < 0 refusal (verb_results.md)
    std::string reason;    // "" or the refusal key
    Id id;                 // the world item created (drop)
    bool theft = false;    // the move took someone else's goods
};
ItemResult drop_item(WorldState& w, const Id& actor, std::size_t stack_index, int qty,
                     const Id& place, int x_cm, int y_cm, int z_cm);
ItemResult pick_up(WorldState& w, const Id& actor, const Id& world_item, int qty,
                   const std::vector<Id>& witnesses);
ItemResult add_container(WorldState& w, const Id& id, const Id& place, const std::string& kind,
                         const Id& owner, int capacity_g);
ItemResult put_in(WorldState& w, const Id& actor, const Id& container, std::size_t stack_index, int qty);
ItemResult take_out(WorldState& w, const Id& actor, const Id& container, std::size_t stack_index,
                    int qty, const std::vector<Id>& witnesses);
std::vector<const WorldItem*> world_items_at(const WorldState& w, const Id& place);
int units_that_fit(const WorldState& w, const Id& actor, const ItemStack& s);  // by the 125% line
```

- The ownership rule (one function, used by `pick_up` and `take_out`): `rightful = stack.owner.empty() ? container_owner : stack.owner`. If `rightful` is empty or equals the actor, the goods become the actor's (`owner = ""`, `stolen = false`). Otherwise `owner = rightful`, `stolen = true`, `theft = true`, and when `witnesses` is not empty `commit_crime(w, actor, "theft", city_of(place), witnesses, rightful)` files it. `city_of(place)` = the `city` column of places.csv, else `"city_of_the_moon"`.
- Dropping keeps the goods the dropper's: a dropped stack with `owner == ""` gets `owner = actor`, so anyone else picking it up is taking it.
- C API (all set `last_reason`):

```c
int sim_world_drop(SimWorld*, const char* actor, int stack_index, int qty, const char* place,
                   int x_cm, int y_cm, int z_cm, char* id_out, int cap);           // units or refusal
int sim_world_pick_up(SimWorld*, const char* actor, const char* world_item, int qty,
                      const char* witnesses_semicolon);                           // units or refusal
int sim_world_world_item_count(const SimWorld*, const char* place);
int sim_world_world_item_at(const SimWorld*, const char* place, int index, char* out, int cap);
    // "id;x;y;z;" + the stack record
int sim_world_add_container(SimWorld*, const char* id, const char* place, const char* kind,
                            const char* owner, int capacity_g);                  // 0 or refusal
int sim_world_container_stack_count(const SimWorld*, const char* container);    // -2 unknown
int sim_world_container_stack_at(const SimWorld*, const char* container, int index, char* out, int cap);
int sim_world_put_in(SimWorld*, const char* actor, const char* container, int stack_index, int qty);
int sim_world_take_out(SimWorld*, const char* actor, const char* container, int stack_index, int qty,
                       const char* witnesses_semicolon);
```

- [ ] **Step 1: Failing tests** (`test_item_actions.cpp`, `MECH:WLD-01 MECH:WLD-03 MECH:WLD-04 MECH:WLD-05 MECH:INV-07 MECH:FND-04`):

```cpp
static WorldState fresh() { WorldState w; w.init("../db/canon", 11); return w; }

static bool test_drop_then_pick_up_round_trip() {
    WorldState w = fresh();
    add_items(w.inventories["player"], "bread", 3);
    ItemResult d = drop_item(w, "player", 0, 2, "moon_gate_place", 100, 200, 0);
    SIM_CHECK_EQ(d.code, 2);
    SIM_CHECK(!d.id.empty());
    SIM_CHECK_EQ(count_of(w.inventories["player"], "bread"), 1);
    SIM_CHECK_EQ(world_items_at(w, "moon_gate_place").size(), 1u);
    SIM_CHECK_EQ(world_items_at(w, "moon_gate_place")[0]->stack.owner, std::string("player"));
    ItemResult p = pick_up(w, "player", d.id, 5, {});
    SIM_CHECK_EQ(p.code, 2);                  // takes what lies there, no more
    SIM_CHECK(!p.theft);
    SIM_CHECK_EQ(count_of(w.inventories["player"], "bread"), 3);
    SIM_CHECK(w.inventories["player"].stacks[0].owner.empty());   // yours again, no owner mark
    SIM_CHECK(world_items_at(w, "moon_gate_place").empty());
    return true;
}

static bool test_refusals_change_nothing() {
    WorldState w = fresh();
    add_items(w.inventories["player"], "bread", 1);
    SIM_CHECK_EQ(drop_item(w, "player", 5, 1, "moon_gate_place", 0, 0, 0).code, -2);   // no such stack
    SIM_CHECK_EQ(drop_item(w, "player", 0, 0, "moon_gate_place", 0, 0, 0).code, -1);   // nothing to drop
    SIM_CHECK_EQ(pick_up(w, "player", "witem_999", 1, {}).code, -2);
    ItemStack q{"clay_liver_model", 1};
    q.bound = true;
    add_stack(w.inventories["player"], q);
    const std::size_t qi = 1;  // bread < clay? stacks sort by item id: "bread" then "clay_liver_model"
    ItemResult r = drop_item(w, "player", qi, 1, "moon_gate_place", 0, 0, 0);
    SIM_CHECK_EQ(r.code, -3);
    SIM_CHECK_EQ(r.reason, std::string("refuse.bound"));
    SIM_CHECK_EQ(count_of(w.inventories["player"], "clay_liver_model"), 1);
    return true;
}

static bool test_taking_anothers_goods_is_theft() {
    WorldState w = fresh();
    add_items(w.inventories["ea_nasir"], "bread", 2);
    ItemResult d = drop_item(w, "ea_nasir", 0, 2, "moon_gate_place", 0, 0, 0);
    const std::size_t crimes_before = w.justice.open.size();
    ItemResult unseen = pick_up(w, "player", d.id, 1, {});
    SIM_CHECK(unseen.theft);
    SIM_CHECK_EQ(w.justice.open.size(), crimes_before);        // nobody saw: no crime filed
    const ItemStack& s = w.inventories["player"].stacks[0];
    SIM_CHECK(s.stolen);
    SIM_CHECK_EQ(s.owner, std::string("ea_nasir"));
    ItemResult seen = pick_up(w, "player", d.id, 1, {"ur_utu_gatekeeper"});
    SIM_CHECK(seen.theft);
    SIM_CHECK(w.justice.open.size() > crimes_before);          // witnessed: filed
    // The owner picks his own goods back up: no longer stolen.
    ItemResult back = drop_item(w, "player", 0, 2, "moon_gate_place", 0, 0, 0);
    ItemResult his = pick_up(w, "ea_nasir", back.id, 2, {});
    SIM_CHECK(!his.theft);
    SIM_CHECK(!w.inventories["ea_nasir"].stacks[0].stolen);
    return true;
}

static bool test_too_heavy_takes_what_fits() {
    WorldState w = fresh();
    add_items(w.inventories["ea_nasir"], "copper_ingots", 1000);
    ItemResult d = drop_item(w, "ea_nasir", 0, 1000, "moon_gate_place", 0, 0, 0);
    ItemResult p = pick_up(w, "player", d.id, 1000, {});
    SIM_CHECK(p.code > 0 && p.code < 1000);
    SIM_CHECK(carried_g(w, "player") * 100 <= capacity_g(w, "player") * kPinnedPct);
    add_items(w.inventories["player"], "copper_ingots", 1000);     // now far past the line
    ItemResult none = pick_up(w, "player", d.id, 1, {});
    SIM_CHECK_EQ(none.code, -4);
    SIM_CHECK_EQ(none.reason, std::string("refuse.too_heavy"));
    return true;
}

static bool test_containers_put_take_capacity_and_owner() {
    WorldState w = fresh();
    SIM_CHECK_EQ(add_container(w, "jar_1", "moon_gate_place", "jar", "ea_nasir", 1000).code, 0);
    SIM_CHECK_EQ(add_container(w, "jar_1", "moon_gate_place", "jar", "", 0).code, -1);  // id taken
    const int unit = item_def(w.db, "bread")->weight_g;
    add_items(w.inventories["ea_nasir"], "bread", 1000 / unit + 5);
    ItemResult put = put_in(w, "ea_nasir", "jar_1", 0, 1000 / unit + 5);
    SIM_CHECK_EQ(put.code, 1000 / unit);                         // only what fits
    SIM_CHECK_EQ(put_in(w, "ea_nasir", "jar_1", 0, 1).code, -5);  // full
    ItemResult took = take_out(w, "player", "jar_1", 0, 1, {});
    SIM_CHECK(took.theft);                                        // the jar is ea_nasir's
    SIM_CHECK(w.inventories["player"].stacks[0].stolen);
    ItemResult own = take_out(w, "ea_nasir", "jar_1", 0, 1, {});
    SIM_CHECK(!own.theft);
    w.world_items.containers["jar_1"].locked = true;
    SIM_CHECK_EQ(take_out(w, "ea_nasir", "jar_1", 0, 1, {}).code, -6);
    return true;
}

static bool test_world_items_and_containers_are_saved() {
    WorldState w = fresh();
    add_items(w.inventories["player"], "bread", 2);
    drop_item(w, "player", 0, 1, "moon_gate_place", 10, 20, 30);
    add_container(w, "jar_1", "moon_gate_place", "jar", "", 0);
    put_in(w, "player", "jar_1", 0, 1);
    const std::string a = save_world(w);
    WorldState back;
    load_world(back, "../db/canon", a);
    SIM_CHECK_EQ(save_world(back), a);
    SIM_CHECK_EQ(back.world_items.items.size(), 1u);
    SIM_CHECK_EQ(back.world_items.items[0].z_cm, 30);
    SIM_CHECK_EQ(count_of(back.world_items.containers["jar_1"].contents, "bread"), 1);
    SIM_CHECK_EQ(back.world_items.next_id, w.world_items.next_id);
    return true;
}
```

(Use real person ids: check `db/canon/people.csv` for Ea-nasir's and a gatekeeper's `id` and substitute; check `JusticeState`'s open-crime container name in Justice.hpp.)

- [ ] **Step 2:** build — fails on the missing header
- [ ] **Step 3:** implement the verbs. Core of `pick_up`:

```cpp
ItemResult pick_up(WorldState& w, const Id& actor, const Id& world_item, int qty,
                   const std::vector<Id>& witnesses) {
    if (actor.empty() || qty <= 0) return refuse(-1);
    auto it = std::find_if(w.world_items.items.begin(), w.world_items.items.end(),
                           [&](const WorldItem& wi) { return wi.id == world_item; });
    if (it == w.world_items.items.end()) return refuse(-2);
    ItemStack part = it->stack;
    part.qty = std::min({qty, it->stack.qty, units_that_fit(w, actor, it->stack)});
    if (part.qty <= 0) return refuse(-4);
    ItemResult r;
    r.theft = claim(w, actor, part, "", it->place, witnesses);
    it->stack.qty -= part.qty;
    r.code = part.qty;
    add_stack(w.inventories[actor], std::move(part));
    if (it->stack.qty == 0) w.world_items.items.erase(it);
    return r;
}
```

with `refuse(code)` building `{code, key_for(code)}` from the verb_results table, `units_that_fit` = `unit == 0 ? INT_MAX : max(0, (capacity*kPinnedPct/100 - carried) / unit)`, and `claim(...)` implementing the ownership rule (it mutates `part.owner`/`part.stolen` and returns whether it was theft; it calls `commit_crime` when witnessed). `put_in` refuses -6 when locked, fits by `capacity_g` minus the contents' weight, refuses -3 for bound stacks. `take_out` refuses -6 when locked. Snapshot: after the DIVINE sections, write

```
WORLD_ITEMS_NEXT <next_id>            (no rows)
WORLD_ITEMS <n>                       rows: id place x y z dropped_day dropped_by + 8 stack fields
CONTAINERS <n>                        rows: id place kind owner locked key_item capacity_g nstacks,
                                      each followed by nstacks 8-field stack rows
```

and read them under `if (!rd.at_end() && rd.peek_tag() == "WORLD_ITEMS_NEXT")`. Factor the 8-field stack writer/reader used by Task 1 into two file-local helpers (`stack_fields`, `stack_from`) and use them in all three places.

- [ ] **Step 4:** C API wrappers (split `witnesses_semicolon` on `;`, skipping empties; `stack_index < 0` → -2) plus C API tests: drop then `world_item_count == 1`, `world_item_at` starts with the returned id, `pick_up` returns the units, `last_reason` is `refuse.bound` after a bound drop, and every call returns -1 on a null world.
- [ ] **Step 5:** `ctest --test-dir kernel/build` — all pass
- [ ] **Step 6: Commit** — `"P0a T6: goods lie in the world and in containers, saved; taking another's goods is theft (FND-04)"`

---

### Task 7: Timed actions by the minute (FND-06)

**Files:** Modify `kernel/include/sim/Needs.hpp`, `kernel/src/Needs.cpp`, `Snapshot.cpp` (optional section `NEEDS_MINUTES`), `CApiItems.h/.cpp`; test in `kernel/tests/test_needs.cpp` (add)

**Interfaces:**
- `NeedsState::minute_carry` (`std::map<Id, int>`, 0..59 per actor)
- `void advance_needs_minutes(NeedsState&, const Id& actor, int minutes, bool sleeping, double severity = 1.0)` — adds minutes to the carry and calls `advance_needs` once per whole hour reached; minutes ≤ 0 do nothing
- `void sim_world_advance_minutes(SimWorld*, const char* actor, int minutes, int sleeping)` — uses the world's `severity_pct` the same way `sim_world_advance_needs` does

- [ ] **Step 1: Failing test** (`MECH:FND-06`):

```cpp
static bool test_minutes_add_up_to_hours() {
    NeedsState s;
    advance_needs_minutes(s, "player", 40, false);
    SIM_CHECK_EQ(needs_of(s, "player").hunger, 0);   // no whole hour yet
    advance_needs_minutes(s, "player", 30, false);   // 70 minutes: one hour
    NeedsState one;
    advance_needs(one, "player", 1, false);
    SIM_CHECK_EQ(needs_of(s, "player").hunger, needs_of(one, "player").hunger);
    SIM_CHECK_EQ(s.minute_carry["player"], 10);
    advance_needs_minutes(s, "player", -5, false);   // ignored
    SIM_CHECK_EQ(s.minute_carry["player"], 10);
    return true;
}
```

(Check how `sim_world_advance_needs` passes severity — mirror it exactly.) Add a snapshot round-trip check that `minute_carry` survives.
- [ ] **Steps 2–4:** implement; the snapshot section is written after the world-items sections and read optionally.
- [ ] **Step 5: Commit** — `"P0a T7: timed actions advance needs by the minute (FND-06)"`

---

### Task 8: The player notice feed (FND-09)

**Files:** Create `kernel/include/sim/Notices.hpp`, `kernel/src/Notices.cpp`, `kernel/tests/test_notices.cpp`; modify `World.hpp` (`NoticeState notices;`), `World.cpp` (reset in init; the hooks), `Snapshot.cpp` (optional `NOTICES` section), `CApiItems.h/.cpp`

**Interfaces:**

```cpp
struct Notice { std::int64_t seq = 0; DayNumber day = 0; std::string key; std::string text; };
struct NoticeState { std::vector<Notice> recent; std::int64_t next_seq = 0; };
constexpr std::size_t kNoticesKept = 256;
void push_notice(NoticeState& s, DayNumber day, const std::string& key, const std::string& text);
const Notice* notice_at(const NoticeState& s, std::int64_t seq);  // nullptr when never written or evicted
```

C API: `int64_t sim_world_notice_count(const SimWorld*)` (= next_seq; -1 null), `int sim_world_notice_at(const SimWorld*, int64_t seq, char* out, int cap)` → `"day;key;text"`, -2 evicted or not yet written.

**Hooks** (in `WorldState::advance_days`, by diffing after the day's ticks, so no module file changes): each new `RaidRecord` → `notice.raid` with its `summary`; each quest id newly in `quests.failed_list` → `notice.quest_failed` with the quest id; each new verdict whose criminal is `"player"` → `notice.verdict` with the crime and verdict. (Read the `Hearing` fields in Justice.hpp and use them.)

- [ ] **Step 1: Failing test** (`MECH:FND-09`):

```cpp
static bool test_feed_keeps_the_newest_and_evicts_the_rest() {
    NoticeState s;
    for (int i = 0; i < 300; ++i) push_notice(s, 1, "notice.raid", "raid " + std::to_string(i));
    SIM_CHECK_EQ(s.next_seq, 300);
    SIM_CHECK_EQ(s.recent.size(), kNoticesKept);
    SIM_CHECK(notice_at(s, 10) == nullptr);                 // evicted
    SIM_CHECK(notice_at(s, 300) == nullptr);                // not yet written
    SIM_CHECK_EQ(notice_at(s, 299)->text, std::string("raid 299"));
    SIM_CHECK_EQ(notice_at(s, 300 - kNoticesKept)->seq, 300 - static_cast<std::int64_t>(kNoticesKept));
    return true;
}

static bool test_raids_reach_the_feed() {
    WorldState w;
    w.init("../db/canon", 21);
    w.facts.drought_stage = 4;
    w.facts.war_stage = 3;
    const std::size_t raids_before = w.wild.raids.size();
    w.advance_days(120);
    SIM_CHECK(w.wild.raids.size() > raids_before);          // the drought brings raids
    bool saw = false;
    for (const Notice& n : w.notices.recent) saw = saw || n.key == "notice.raid";
    SIM_CHECK(saw);
    return true;
}
```

(If the raid log is capped (`kMaxRaidLog`), diff on `w.wild.next_raid` instead of `raids.size()`.) Add a snapshot round trip for `notices`.
- [ ] **Steps 2–4:** implement; C API test: `sim_world_notice_count` is 0 on a fresh world and `sim_world_notice_at(w, 0, …) == -2`.
- [ ] **Step 5: Commit** — `"P0a T8: the player notice feed — raids, failed quests, verdicts (FND-09)"`

---

### Task 9: Close the loop — no-throw, reach, registry, contracts

**Files:** Modify `kernel/tests/test_capi_nothrow.cpp` (include CApiItems.h; drive every new allocating call under failing allocation and check its documented error value), `tools/capi_reach.py` (a stage for the new calls: `last_reason|stack_|item_def|carried_g|capacity_g|encumbrance|drop|pick_up|world_item|container|put_in|take_out` → `C`; `advance_minutes` → `B`; `notice_` → `P`), `docs/mechanics-registry.md` (statuses: FND-01/02/03/04/05/06/09 K=Y C=Y; INV-01/03/04/06/07/15 and WLD-01/03/04/05 K=Y C=Y; cite every new call), create `kernel/contracts/module_Items.md` (the rules above: normalization, the take order, ownership and theft, the 125% line, the save sections), `docs/mechanics-implementation-plan.md` (P0a done line).

- [ ] **Step 1:** add the no-throw lines; run `ctest --test-dir kernel/build -R nothrow` — PASS
- [ ] **Step 2:** `python3 tools/capi_reach.py && python3 tools/capi_reach.py --check` — PASS; `python3 tools/mechanics_registry.py && python3 tools/mechanics_registry.py --check` — PASS, with the new MECH tags counted as tested
- [ ] **Step 3:** the full gate: `python3 tools/canon_lint.py && python3 tools/coverage_check.py && python3 tools/stage_canon_for_ue.py --check && ctest --test-dir kernel/build` — all green
- [ ] **Step 4: Commit** — `"P0a T9: the new calls are no-throw, staged and cited; registry statuses updated"`
- [ ] **Step 5:** a bug review of the whole branch (`git diff main...HEAD`), fix what it finds, then merge into `main` (fast-forward or a merge commit) once the gate is green.
