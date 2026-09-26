// test_capi_items.cpp — the CApiItems surface. MECH:FND-05 MECH:FND-03
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

SIM_MAIN(test_last_reason_starts_empty_and_null_is_minus_one, test_carrying_through_c)
