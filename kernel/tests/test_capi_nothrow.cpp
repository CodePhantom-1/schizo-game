// test_capi_nothrow.cpp — no C++ exception may cross the C boundary into the
// engine (bug review 2026-09-25). This binary replaces the global operator
// new with one that can be told to fail, then drives every allocating C API
// entry point while allocation fails: each must return its documented error
// value instead of letting std::bad_alloc escape (which would std::terminate
// the engine). Its own binary so the replacement touches no other suite.
#include "sim/CApi.h"

#include "sim/Test.hpp"

#include <cstdlib>
#include <new>

namespace {
bool g_fail_alloc = false;
}

// Under -fsanitize=address, libstdc++'s own allocations can reach ASan's
// operator new while this binary's delete frees them with free(): a mismatch
// of the test's replacement, not of the kernel. Read only by ASan builds.
extern "C" const char* __asan_default_options() { return "alloc_dealloc_mismatch=0"; }

void* operator new(std::size_t n) {
    if (g_fail_alloc) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

// Longer than any small-string buffer, so building the std::string key allocates.
static const char* kLong = "an_actor_id_long_enough_to_defeat_small_string_optimisation";

static bool test_no_exception_escapes_the_c_boundary() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    char buf[256];

    g_fail_alloc = true;
    const int64_t price = sim_world_price(w, kLong, kLong);
    const int standing = sim_world_standing(w, kLong);
    sim_world_add_standing(w, kLong, 5);
    const int favour = sim_world_favour(w, kLong);
    sim_world_add_favour(w, kLong, 5);
    const int hunger = sim_world_hunger(w, kLong);
    const int thirst = sim_world_thirst(w, kLong);
    const int fatigue = sim_world_fatigue(w, kLong);
    sim_world_advance_needs(w, kLong, 3, 0);
    const int eat = sim_world_eat(w, kLong, kLong);
    const int drink = sim_world_drink(w, kLong, kLong);
    const int effects = sim_world_need_effects(w, kLong, buf, sizeof(buf));
    const int count = sim_world_item_count(w, kLong, kLong);
    const int give = sim_world_give_item(w, kLong, kLong, 1);
    const int craft = sim_world_craft(w, kLong, kLong, "quern;oven", 1);
    const int task = sim_world_task_at(w, kLong, 6, buf, sizeof(buf));
    const int saved = sim_world_save_to_buffer(w, buf, sizeof(buf));
    SimWorld* loaded = sim_world_load_from_buffer("../db/canon", "SIMSAVE 2\n");
    SimWorld* created = sim_world_create("../db/canon", 1);
    sim_world_advance_days(w, 1);
    g_fail_alloc = false;

    SIM_CHECK_EQ(price, int64_t{-1});
    SIM_CHECK_EQ(standing, -1);
    SIM_CHECK_EQ(favour, -1);
    SIM_CHECK_EQ(hunger, -1);
    SIM_CHECK_EQ(thirst, -1);
    SIM_CHECK_EQ(fatigue, -1);
    SIM_CHECK_EQ(eat, -1);
    SIM_CHECK_EQ(drink, -1);
    SIM_CHECK_EQ(effects, -1);
    SIM_CHECK_EQ(count, -1);
    SIM_CHECK_EQ(give, -1);
    SIM_CHECK_EQ(craft, -1);
    SIM_CHECK_EQ(task, -1);
    SIM_CHECK_EQ(saved, -1);
    SIM_CHECK(loaded == nullptr);
    SIM_CHECK(created == nullptr);

    // The world is still usable once memory is back.
    sim_world_advance_days(w, 1);
    SIM_CHECK(sim_world_day(w) >= 2);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_no_exception_escapes_the_c_boundary)
