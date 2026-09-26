// test_capi_items.cpp — the CApiItems surface. MECH:FND-05
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

SIM_MAIN(test_last_reason_starts_empty_and_null_is_minus_one)
