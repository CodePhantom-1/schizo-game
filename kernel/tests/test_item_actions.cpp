// test_item_actions.cpp — carrying, world items, containers, theft.
// MECH:FND-03 MECH:BOD-19 MECH:INV-02 MECH:INV-11 MECH:FND-04 MECH:WLD-01 MECH:WLD-03
// MECH:WLD-04 MECH:WLD-05 MECH:INV-07 MECH:INV-08
#include "sim/ItemActions.hpp"
#include "sim/Progression.hpp"
#include "sim/Property.hpp"
#include "sim/Snapshot.hpp"
#include "sim/Test.hpp"

using namespace sim;

static WorldState fresh() {
    WorldState w;
    w.init("../db/canon", 11);
    return w;
}

static bool test_capacity_follows_strength() {
    WorldState w = fresh();
    const int str = actor_attribute(w, "player", "strength");
    SIM_CHECK(str >= 1);
    SIM_CHECK_EQ(capacity_g(w, "player"), kBaseCarryG + static_cast<long long>(kCarryPerStrengthG) * str);
    SIM_CHECK_EQ(capacity_g(w, "nobody_at_all"), kBaseCarryG + static_cast<long long>(kCarryPerStrengthG) * 5);
    return true;
}

static bool test_encumbrance_boundaries() {
    WorldState w = fresh();
    const long long cap = capacity_g(w, "player");
    SIM_CHECK_EQ(cap % 1000, 0);  // tin is 1000 g: the lines fall on whole units
    Inventory& inv = w.inventories["player"];
    SIM_CHECK_EQ(carried_g(w, "player"), 0);
    SIM_CHECK_EQ(encumbrance(w, "player"), 0);
    set_count(inv, "tin", static_cast<int>(cap / 1000));            // exactly 100%
    SIM_CHECK_EQ(carried_g(w, "player"), cap);
    SIM_CHECK_EQ(encumbrance(w, "player"), 0);
    set_count(inv, "tin", static_cast<int>(cap / 1000) + 1);        // just past 100%
    SIM_CHECK_EQ(encumbrance(w, "player"), 1);
    const int at125 = static_cast<int>(cap * kPinnedPct / 100 / 1000);
    set_count(inv, "tin", at125);                                    // at or under 125%
    SIM_CHECK(carried_g(w, "player") * 100 <= cap * kPinnedPct);
    SIM_CHECK_EQ(encumbrance(w, "player"), 1);
    set_count(inv, "tin", at125 + 1);                                // past 125%
    SIM_CHECK_EQ(encumbrance(w, "player"), 2);
    return true;
}

static bool test_silver_has_weight() {
    WorldState w = fresh();
    credit_purse(w.property, "player", kGrainsPerShekel * 10);  // ten shekels
    SIM_CHECK_EQ(carried_g(w, "player"), 10LL * kSilverGPerShekel);
    return true;
}

static const char* kTrader = "ea_nasir_grain_trader";
static const char* kGatekeeper = "ur_utu_gatekeeper_dawn";
static const char* kGate = "moon_gate_place";

static bool test_drop_then_pick_up_round_trip() {
    WorldState w = fresh();
    add_items(w.inventories["player"], "bread", 3);
    ItemResult d = drop_item(w, "player", 0, 2, kGate, 100, 200, 0);
    SIM_CHECK_EQ(d.code, 2);
    SIM_CHECK(!d.id.empty());
    SIM_CHECK_EQ(count_of(w.inventories["player"], "bread"), 1);
    SIM_CHECK_EQ(world_items_at(w, kGate).size(), 1u);
    SIM_CHECK_EQ(world_items_at(w, kGate)[0]->stack.owner, std::string("player"));  // still yours
    SIM_CHECK_EQ(world_items_at(w, kGate)[0]->x_cm, 100);
    ItemResult p = pick_up(w, "player", d.id, 5, {});
    SIM_CHECK_EQ(p.code, 2);  // takes what lies there, no more
    SIM_CHECK(!p.theft);
    SIM_CHECK_EQ(count_of(w.inventories["player"], "bread"), 3);
    SIM_CHECK_EQ(w.inventories["player"].stacks.size(), 1u);  // merged: no owner mark left
    SIM_CHECK(world_items_at(w, kGate).empty());
    return true;
}

static bool test_refusals_change_nothing() {
    WorldState w = fresh();
    add_items(w.inventories["player"], "bread", 1);
    SIM_CHECK_EQ(drop_item(w, "player", 5, 1, kGate, 0, 0, 0).code, -2);  // no such stack
    SIM_CHECK_EQ(drop_item(w, "player", 0, 0, kGate, 0, 0, 0).code, -1);  // nothing to drop
    SIM_CHECK_EQ(drop_item(w, "player", 0, 1, "", 0, 0, 0).code, -1);     // nowhere to drop
    SIM_CHECK_EQ(pick_up(w, "player", "witem_999", 1, {}).code, -2);
    SIM_CHECK_EQ(pick_up(w, "player", "", 1, {}).code, -2);
    ItemStack q{"clay_liver_model", 1};
    q.bound = true;
    add_stack(w.inventories["player"], q);  // stacks sort by item: bread (0), clay_liver_model (1)
    ItemResult r = drop_item(w, "player", 1, 1, kGate, 0, 0, 0);
    SIM_CHECK_EQ(r.code, -3);
    SIM_CHECK_EQ(r.reason, std::string("refuse.bound"));
    SIM_CHECK_EQ(count_of(w.inventories["player"], "clay_liver_model"), 1);
    SIM_CHECK_EQ(count_of(w.inventories["player"], "bread"), 1);
    SIM_CHECK(w.world_items.items.empty());
    return true;
}

static bool test_refusals_for_an_unseen_actor_leave_the_save_untouched() {
    WorldState w = fresh();
    add_container(w, "jar_1", kGate, "jar", "", 0);
    const std::string before = save_world(w);
    SIM_CHECK_EQ(drop_item(w, "ghost", 0, 1, kGate, 0, 0, 0).code, -2);
    SIM_CHECK_EQ(put_in(w, "ghost", "jar_1", 0, 1).code, -2);
    SIM_CHECK_EQ(save_world(w), before);
    return true;
}

static bool test_taking_anothers_goods_is_theft() {
    WorldState w = fresh();
    add_items(w.inventories[kTrader], "bread", 2);
    ItemResult d = drop_item(w, kTrader, 0, 2, kGate, 0, 0, 0);
    const std::size_t crimes_before = w.justice.open_crimes.size();
    ItemResult unseen = pick_up(w, "player", d.id, 1, {});
    SIM_CHECK_EQ(unseen.code, 1);
    SIM_CHECK(unseen.theft);
    SIM_CHECK_EQ(w.justice.open_crimes.size(), crimes_before);  // nobody saw: no crime filed
    SIM_CHECK(w.inventories["player"].stacks[0].stolen);
    SIM_CHECK_EQ(w.inventories["player"].stacks[0].owner, std::string(kTrader));
    ItemResult seen = pick_up(w, "player", d.id, 1, {kGatekeeper});
    SIM_CHECK(seen.theft);
    SIM_CHECK(w.justice.open_crimes.size() > crimes_before);  // witnessed: filed
    // The owner picks his own goods back up: no longer stolen.
    ItemResult back = drop_item(w, "player", 0, 2, kGate, 0, 0, 0);
    SIM_CHECK_EQ(back.code, 2);
    ItemResult his = pick_up(w, kTrader, back.id, 2, {});
    SIM_CHECK(!his.theft);
    SIM_CHECK(!w.inventories[kTrader].stacks[0].stolen);
    SIM_CHECK(w.inventories[kTrader].stacks[0].owner.empty());
    return true;
}

static bool test_too_heavy_takes_what_fits() {
    WorldState w = fresh();
    add_items(w.inventories[kTrader], "tin", 1000);
    ItemResult d = drop_item(w, kTrader, 0, 1000, kGate, 0, 0, 0);
    ItemResult p = pick_up(w, "player", d.id, 1000, {});
    SIM_CHECK(p.code > 0 && p.code < 1000);
    SIM_CHECK(carried_g(w, "player") * 100 <= capacity_g(w, "player") * kPinnedPct);
    SIM_CHECK_EQ(p.code, static_cast<int>(capacity_g(w, "player") * kPinnedPct / 100 / 1000));
    ItemResult none = pick_up(w, "player", d.id, 1, {});
    SIM_CHECK_EQ(none.code, -4);
    SIM_CHECK_EQ(none.reason, std::string("refuse.too_heavy"));
    SIM_CHECK_EQ(world_items_at(w, kGate)[0]->stack.qty, 1000 - p.code);  // nothing moved
    return true;
}

static bool test_containers_put_take_capacity_and_owner() {
    WorldState w = fresh();
    SIM_CHECK_EQ(add_container(w, "jar_1", kGate, "jar", kTrader, 1000).code, 0);
    SIM_CHECK_EQ(add_container(w, "jar_1", kGate, "jar", "", 0).code, -1);  // id taken
    SIM_CHECK_EQ(add_container(w, "", kGate, "jar", "", 0).code, -1);
    add_items(w.inventories[kTrader], "bread", 9);
    ItemResult put = put_in(w, kTrader, "jar_1", 0, 9);
    SIM_CHECK_EQ(put.code, 4);                                     // 250 g loaves in a 1000 g jar
    SIM_CHECK_EQ(put_in(w, kTrader, "jar_1", 0, 1).code, -5);      // full
    SIM_CHECK_EQ(put_in(w, kTrader, "no_jar", 0, 1).code, -2);
    ItemResult took = take_out(w, "player", "jar_1", 0, 1, {});
    SIM_CHECK_EQ(took.code, 1);
    SIM_CHECK(took.theft);                                         // the jar is the trader's
    SIM_CHECK(w.inventories["player"].stacks[0].stolen);
    ItemResult own = take_out(w, kTrader, "jar_1", 0, 1, {});
    SIM_CHECK(!own.theft);
    w.world_items.containers["jar_1"].locked = true;
    ItemResult locked = take_out(w, kTrader, "jar_1", 0, 1, {});
    SIM_CHECK_EQ(locked.code, -6);
    SIM_CHECK_EQ(locked.reason, std::string("refuse.locked"));
    SIM_CHECK_EQ(put_in(w, kTrader, "jar_1", 0, 1).code, -6);
    ItemStack q{"clay_liver_model", 1};
    q.bound = true;
    w.world_items.containers["jar_1"].locked = false;
    add_stack(w.inventories["player"], q);
    SIM_CHECK_EQ(put_in(w, "player", "jar_1", 1, 1).code, -3);     // bound goods stay with you
    return true;
}

static bool test_world_items_and_containers_are_saved() {
    WorldState w = fresh();
    add_items(w.inventories["player"], "bread", 2);
    drop_item(w, "player", 0, 1, kGate, 10, 20, 30);
    add_container(w, "jar_1", kGate, "jar", "", 0);
    put_in(w, "player", "jar_1", 0, 1);
    const std::string a = save_world(w);
    WorldState back;
    load_world(back, "../db/canon", a);
    SIM_CHECK_EQ(save_world(back), a);
    SIM_CHECK_EQ(back.world_items.items.size(), 1u);
    SIM_CHECK_EQ(back.world_items.items[0].z_cm, 30);
    SIM_CHECK_EQ(back.world_items.items[0].stack.owner, std::string("player"));
    SIM_CHECK_EQ(count_of(back.world_items.containers["jar_1"].contents, "bread"), 1);
    SIM_CHECK_EQ(back.world_items.next_id, w.world_items.next_id);
    return true;
}

SIM_MAIN(test_capacity_follows_strength, test_encumbrance_boundaries, test_silver_has_weight,
         test_drop_then_pick_up_round_trip, test_refusals_change_nothing,
         test_refusals_for_an_unseen_actor_leave_the_save_untouched,
         test_taking_anothers_goods_is_theft, test_too_heavy_takes_what_fits,
         test_containers_put_take_capacity_and_owner, test_world_items_and_containers_are_saved)
