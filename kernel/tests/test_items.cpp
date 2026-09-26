// test_items.cpp — FND-01/02: item stacks and the catalogue. MECH:FND-01 MECH:FND-02 MECH:INV-01 MECH:INV-06 MECH:INV-15
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

static bool test_set_count_moves_to_the_target() {
    Inventory inv;
    SIM_CHECK_EQ(set_count(inv, "grain", 4), 4);
    SIM_CHECK_EQ(set_count(inv, "grain", 1), 1);
    SIM_CHECK_EQ(set_count(inv, "grain", -3), 0);
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

static bool test_item_defs_from_canon() {
    WorldState w;
    w.init("../db/canon", 1);
    const std::optional<ItemDef> bread = item_def(w.db, "bread");
    SIM_CHECK(bread.has_value());
    SIM_CHECK_EQ(bread->ui_category, std::string("food"));
    SIM_CHECK_EQ(bread->weight_g, 250);
    SIM_CHECK_EQ(bread->stack_max, 20);
    SIM_CHECK_EQ(bread->spoil_days, 3);
    SIM_CHECK_EQ(item_def(w.db, "copper_dagger")->weight_g, 300);  // arms.csv weight
    SIM_CHECK(item_def(w.db, "clay_liver_model")->has_flag("document"));
    SIM_CHECK(!item_def(w.db, "clay_liver_model")->has_flag("fixture"));
    SIM_CHECK(item_def(w.db, "quern")->has_flag("fixture"));
    SIM_CHECK(!item_def(w.db, "no_such_item").has_value());
    SIM_CHECK_EQ(stack_weight_g(w.db, ItemStack{"bread", 4}), 1000);
    SIM_CHECK_EQ(stack_weight_g(w.db, ItemStack{"water", 3}), 3000);   // carried water weighs
    SIM_CHECK_EQ(stack_weight_g(w.db, ItemStack{"no_such_item", 9}), 0);
    return true;
}

SIM_MAIN(test_item_defs_from_canon, test_add_merges_and_counts, test_zero_and_negative_are_no_ops,
         test_take_never_overdraws_and_takes_oldest_first, test_split_a_stack,
         test_set_count_moves_to_the_target,
         test_saturates_instead_of_overflowing, test_snapshot_round_trip_keeps_every_field,
         test_legacy_count_rows_still_load)
