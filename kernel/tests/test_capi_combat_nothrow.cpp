// test_capi_combat_nothrow.cpp — W4-B: the combat C API keeps the rule of
// test_capi_nothrow.cpp. No C++ exception may cross the C boundary. With
// allocation failing, every combat entry point returns its error value, and
// the world stays usable afterwards. It is its own binary because it replaces
// the global operator new.
#include "sim/CApi.h"

#include "sim/Test.hpp"

#include <cstdlib>
#include <new>

namespace {
bool g_fail_alloc = false;
}

extern "C" const char* __asan_default_options() { return "alloc_dealloc_mismatch=0"; }

void* operator new(std::size_t n) {
    if (g_fail_alloc) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static const char* kLong = "an_actor_id_long_enough_to_defeat_small_string_optimisation";

static bool test_no_exception_escapes_the_combat_api() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    char buf[256];

    g_fail_alloc = true;
    const int results[] = {
        sim_world_equip(w, kLong, kLong),
        sim_world_unequip(w, kLong, kLong),
        sim_world_equipped(w, kLong, buf, sizeof buf),
        sim_world_attack(w, kLong, "player", 1, buf, sizeof buf),
        sim_world_health(w, kLong),
        sim_world_stamina(w, kLong),
        sim_world_morale(w, kLong),
        sim_world_wound(w, kLong, 1),
        sim_world_bleeding(w, kLong),
        sim_world_is_dead(w, kLong),
        sim_world_combat_status(w, kLong, buf, sizeof buf),
        sim_world_wounds(w, kLong, buf, sizeof buf),
        sim_world_rest(w, kLong, 8),
        sim_world_combat_advance(w, kLong, 2),
        sim_world_catch_breath(w, kLong, 1),
        sim_world_treat_wounds(w, kLong, "bind", kLong),
        sim_world_stance(w, kLong, 1, 1, buf, sizeof buf),
        sim_world_apply_combat_style(w, kLong, "retributors_paladin"),
        sim_world_combat_style_of(w, kLong, buf, sizeof buf),
        sim_world_surrender(w, kLong, "player"),
        sim_world_take_prisoner(w, "player", kLong),
        sim_world_release_prisoner(w, kLong),
        static_cast<int>(sim_world_ransom(w, kLong, buf, sizeof buf)),
        sim_world_loot(w, "player", kLong),
        sim_world_agree_duel(w, kLong, "player"),
        sim_world_set_outlaw(w, kLong, 1),
        sim_world_combat_crime(w, kLong, "player", kLong, kLong, buf, sizeof buf),
        sim_world_repair(w, kLong, kLong, kLong),
        sim_world_recast(w, kLong, kLong, kLong, kLong),
        sim_world_skirmish(w, kLong, "player", 3, buf, sizeof buf),
    };
    g_fail_alloc = false;

    for (int r : results) SIM_CHECK_EQ(r, -1);

    // Still usable once memory is back.
    SIM_CHECK_EQ(sim_world_health(w, "player"), 100);
    sim_world_advance_days(w, 1);
    SIM_CHECK(sim_world_day(w) >= 2);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_no_exception_escapes_the_combat_api)
