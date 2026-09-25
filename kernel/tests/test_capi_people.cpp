// test_capi_people.cpp — A7: names, memories and the events feed through C.
#include "sim/CApi.h"
#include "sim/CApiPeople.h"
#include "sim/Test.hpp"

#include <string>

static bool test_npc_names_are_not_ids() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const int n = sim_world_npc_count(w);
    SIM_CHECK(n > 0);
    char id[128], name[256];
    for (int i = 0; i < n; ++i) {
        SIM_CHECK(sim_world_npc_id(w, i, id, sizeof id) > 0);
        SIM_CHECK(sim_world_npc_name(w, id, name, sizeof name) > 0);
        SIM_CHECK(std::string(name).find('_') == std::string::npos);  // a name, not an id
    }
    SIM_CHECK_EQ(sim_world_npc_name(w, "no_such_npc", name, sizeof name), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_memory_bounds() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    char id[128];
    sim_world_npc_id(w, 0, id, sizeof id);
    const int m = sim_world_npc_memory_count(w, id);
    SIM_CHECK(m >= 0);
    int64_t day;
    char subj[64], fact[256];
    SIM_CHECK_EQ(sim_world_npc_memory_at(w, id, m, &day, subj, sizeof subj, fact, sizeof fact), -1);
    SIM_CHECK_EQ(sim_world_npc_memory_at(w, id, -1, &day, subj, sizeof subj, fact, sizeof fact), -1);
    SIM_CHECK_EQ(sim_world_npc_memory_count(w, "no_such_npc"), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_event_feed_is_ordered() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    sim_world_set_drought(w, 3);
    sim_world_advance_days(w, 400);
    const int n = sim_world_event_count(w);
    SIM_CHECK(n > 0);
    int64_t prev = 0, day = 0;
    char rule[128], summary[512];
    for (int i = 0; i < n; ++i) {
        SIM_CHECK(sim_world_event_at(w, i, &day, rule, sizeof rule, summary, sizeof summary) >= 0);
        SIM_CHECK(day >= prev);
        prev = day;
    }
    SIM_CHECK_EQ(sim_world_event_at(w, n, &day, rule, sizeof rule, summary, sizeof summary), -1);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_npc_names_are_not_ids, test_memory_bounds, test_event_feed_is_ordered)
