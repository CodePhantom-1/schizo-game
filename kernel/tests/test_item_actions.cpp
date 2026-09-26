// test_item_actions.cpp — carrying, world items, containers, theft.
// MECH:FND-03 MECH:BOD-19 MECH:INV-02 MECH:INV-11
#include "sim/ItemActions.hpp"
#include "sim/Progression.hpp"
#include "sim/Property.hpp"
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

SIM_MAIN(test_capacity_follows_strength, test_encumbrance_boundaries, test_silver_has_weight)
