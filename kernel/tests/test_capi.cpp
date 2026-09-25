// test_capi.cpp — the C boundary the engine will live behind: everything the
// SimRuntime plugin needs must work through extern "C" alone.
#include "sim/CApi.h"

#include "sim/Test.hpp"

#include <cstring>
#include <string>

static bool test_lifecycle_and_clock() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    if (sim_world_day(w) != 1) { sim_world_destroy(w); return false; }
    sim_world_advance_days(w, 30);
    if (sim_world_day(w) != 31) { sim_world_destroy(w); return false; }
    char season[16];
    const int len = sim_world_season(w, season, sizeof(season));
    if (len < 0) { sim_world_destroy(w); return false; }
    if (std::strcmp(season, "rains") != 0 && std::strcmp(season, "sowing") != 0 &&
        std::strcmp(season, "harvest") != 0 && std::strcmp(season, "vintage") != 0) {
        std::fprintf(stderr, "FAIL unknown season id '%s'\n", season);
        sim_world_destroy(w);
        return false;
    }
    sim_world_destroy(w);
    return true;
}

static bool test_prices_and_the_drought_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    sim_world_advance_days(w, 60);
    const int64_t before = sim_world_price(w, "city_of_the_moon", "grain");
    sim_world_set_drought(w, 3);
    sim_world_advance_days(w, 60);
    const int64_t after = sim_world_price(w, "city_of_the_moon", "grain");
    const bool ok = after > before;
    sim_world_destroy(w);
    return ok;
}

static bool test_standing_and_favour_clamps() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    sim_world_add_favour(w, "inanna", 200);
    const int fav = sim_world_favour(w, "inanna");
    sim_world_add_standing(w, "the_empire", -500);
    const int st = sim_world_standing(w, "the_empire");
    const bool ok = fav == 100 && st == 0;
    sim_world_destroy(w);
    return ok;
}

static bool test_two_worlds_same_seed_agree() {
    SimWorld* a = sim_world_create("../db/canon", 9);
    SimWorld* b = sim_world_create("../db/canon", 9);
    if (!a || !b) { sim_world_destroy(a); sim_world_destroy(b); return false; }
    sim_world_advance_days(a, 120);
    sim_world_advance_days(b, 120);
    const bool ok = sim_world_price(a, "city_of_jewels", "grain") ==
                    sim_world_price(b, "city_of_jewels", "grain");
    sim_world_destroy(a);
    sim_world_destroy(b);
    return ok;
}

SIM_MAIN(test_lifecycle_and_clock,
         test_prices_and_the_drought_through_c,
         test_standing_and_favour_clamps,
         test_two_worlds_same_seed_agree)
