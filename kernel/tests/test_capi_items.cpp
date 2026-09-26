// test_capi_items.cpp — the CApiItems surface. MECH:FND-05 MECH:FND-03 MECH:INV-03 MECH:INV-04
#include "sim/CApi.h"
#include "sim/Test.hpp"

#include <cstring>
#include <string>

static bool test_last_reason_starts_empty_and_null_is_minus_one() {
    SimWorld* w = sim_world_create("../db/canon", 3);
    SIM_CHECK(w != nullptr);
    char buf[64] = "x";
    SIM_CHECK_EQ(sim_world_last_reason(w, buf, sizeof buf), 0);
    SIM_CHECK_EQ(std::string(buf), std::string(""));
    SIM_CHECK_EQ(sim_world_last_reason(nullptr, buf, sizeof buf), -1);
    SIM_CHECK_EQ(sim_world_last_reason(w, nullptr, 0), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_carrying_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 3);
    const int64_t cap = sim_world_capacity_g(w, "player");
    SIM_CHECK(cap > 0);
    SIM_CHECK_EQ(sim_world_carried_g(w, "player"), 0);
    SIM_CHECK_EQ(sim_world_encumbrance(w, "player"), 0);
    sim_world_give_item(w, "player", "tin", static_cast<int>(cap / 1000) + 1);
    SIM_CHECK_EQ(sim_world_carried_g(w, "player"), (cap / 1000 + 1) * 1000);
    SIM_CHECK_EQ(sim_world_encumbrance(w, "player"), 1);
    SIM_CHECK_EQ(sim_world_capacity_g(nullptr, "player"), -1);
    SIM_CHECK_EQ(sim_world_carried_g(w, nullptr), -1);
    SIM_CHECK_EQ(sim_world_encumbrance(nullptr, "player"), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_stacks_and_defs_read_back() {
    SimWorld* w = sim_world_create("../db/canon", 3);
    sim_world_give_item(w, "player", "bread", 3);
    SIM_CHECK_EQ(sim_world_stack_count(w, "player"), 1);
    SIM_CHECK_EQ(sim_world_stack_count(w, "nobody"), 0);
    SIM_CHECK_EQ(sim_world_stack_count(nullptr, "player"), -1);
    char buf[256];
    SIM_CHECK(sim_world_stack_at(w, "player", 0, buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("bread;3;1;100;;0;0;0"));
    SIM_CHECK_EQ(sim_world_stack_at(w, "player", 1, buf, sizeof buf), -2);
    SIM_CHECK_EQ(sim_world_stack_at(w, "player", -1, buf, sizeof buf), -2);
    SIM_CHECK_EQ(sim_world_stack_at(w, "nobody", 0, buf, sizeof buf), -2);
    SIM_CHECK_EQ(sim_world_stack_at(w, "player", 0, nullptr, 0), -1);
    SIM_CHECK(sim_world_item_def(w, "bread", buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("food;250;20;3;"));
    SIM_CHECK(sim_world_item_def(w, "quern", buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("station_part;25000;1;0;fixture"));
    SIM_CHECK_EQ(sim_world_item_def(w, "nothing", buf, sizeof buf), -2);
    SIM_CHECK_EQ(sim_world_item_def(w, nullptr, buf, sizeof buf), -1);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_last_reason_starts_empty_and_null_is_minus_one, test_carrying_through_c,
         test_stacks_and_defs_read_back)
