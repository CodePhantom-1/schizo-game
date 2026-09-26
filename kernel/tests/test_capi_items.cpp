// test_capi_items.cpp — the CApiItems surface. MECH:FND-05 MECH:FND-03 MECH:INV-03 MECH:INV-04 MECH:WLD-01 MECH:WLD-03 MECH:WLD-05
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

static bool test_world_items_and_containers_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 3);
    sim_world_give_item(w, "player", "bread", 3);
    char id[64] = {};
    SIM_CHECK_EQ(sim_world_drop(w, "player", 0, 2, "moon_gate_place", 10, 20, 30, id, sizeof id), 2);
    SIM_CHECK(std::string(id).rfind("witem_", 0) == 0);
    char buf[256];
    SIM_CHECK_EQ(sim_world_last_reason(w, buf, sizeof buf), 0);
    SIM_CHECK_EQ(sim_world_world_item_count(w, "moon_gate_place"), 1);
    SIM_CHECK_EQ(sim_world_world_item_count(w, "nowhere"), 0);
    SIM_CHECK(sim_world_world_item_at(w, "moon_gate_place", 0, buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string(id) + ";10;20;30;bread;2;1;100;player;0;0;0");
    SIM_CHECK_EQ(sim_world_world_item_at(w, "moon_gate_place", 1, buf, sizeof buf), -2);
    SIM_CHECK_EQ(sim_world_pick_up(w, "player", id, 9, ""), 2);
    SIM_CHECK_EQ(sim_world_item_count(w, "player", "bread"), 3);
    SIM_CHECK_EQ(sim_world_pick_up(w, "player", id, 1, nullptr), -2);  // gone
    SIM_CHECK(sim_world_last_reason(w, buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("refuse.unknown"));
    SIM_CHECK_EQ(sim_world_drop(w, "player", 0, 1, "moon_gate_place", 0, 0, 0, nullptr, 0), 1);  // id optional

    SIM_CHECK_EQ(sim_world_add_container(w, "jar_1", "moon_gate_place", "jar", "", 0), 0);
    SIM_CHECK_EQ(sim_world_add_container(w, "jar_1", "moon_gate_place", "jar", "", 0), -1);
    SIM_CHECK_EQ(sim_world_put_in(w, "player", "jar_1", 0, 2), 2);
    SIM_CHECK_EQ(sim_world_container_stack_count(w, "jar_1"), 1);
    SIM_CHECK_EQ(sim_world_container_stack_count(w, "no_jar"), -2);
    SIM_CHECK(sim_world_container_stack_at(w, "jar_1", 0, buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("bread;2;1;100;player;0;0;0"));
    SIM_CHECK_EQ(sim_world_take_out(w, "player", "jar_1", 0, 1, "ur_utu_gatekeeper_dawn;"), 1);
    SIM_CHECK_EQ(sim_world_take_out(w, "player", "jar_1", 5, 1, ""), -2);
    SIM_CHECK_EQ(sim_world_put_in(w, "player", "jar_1", -1, 1), -2);

    SIM_CHECK_EQ(sim_world_drop(nullptr, "player", 0, 1, "p", 0, 0, 0, nullptr, 0), -1);
    SIM_CHECK_EQ(sim_world_pick_up(w, nullptr, id, 1, ""), -1);
    SIM_CHECK_EQ(sim_world_world_item_count(nullptr, "p"), -1);
    SIM_CHECK_EQ(sim_world_add_container(w, nullptr, "p", "jar", "", 0), -1);
    SIM_CHECK_EQ(sim_world_put_in(w, "player", nullptr, 0, 1), -1);
    SIM_CHECK_EQ(sim_world_take_out(nullptr, "player", "jar_1", 0, 1, ""), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_bound_drop_reports_its_reason() {
    SimWorld* w = sim_world_create("../db/canon", 3);
    sim_world_give_item(w, "player", "bread", 1);
    // Nothing gives bound goods through the C API yet; a bound refusal is
    // covered by test_item_actions. Here: a bad stack index names its reason.
    SIM_CHECK_EQ(sim_world_drop(w, "player", 4, 1, "moon_gate_place", 0, 0, 0, nullptr, 0), -2);
    char buf[64];
    sim_world_last_reason(w, buf, sizeof buf);
    SIM_CHECK_EQ(std::string(buf), std::string("refuse.unknown"));
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_last_reason_starts_empty_and_null_is_minus_one, test_carrying_through_c,
         test_stacks_and_defs_read_back, test_world_items_and_containers_through_c,
         test_bound_drop_reports_its_reason)
