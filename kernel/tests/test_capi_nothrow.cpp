// test_capi_nothrow.cpp — no C++ exception may cross the C boundary into the
// engine (bug review 2026-09-25). This binary replaces the global operator
// new with one that can be told to fail, then drives every allocating C API
// entry point while allocation fails: each must return its documented error
// value instead of letting std::bad_alloc escape (which would std::terminate
// the engine). Its own binary so the replacement touches no other suite.
#include "sim/CApi.h"
#include "sim/CApiVerbs.h"

#include "sim/Test.hpp"

#include <cstdlib>
#include <new>
#include <string>

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

// Follow-up review: the K-1 (rites) and K-2 (festivals, people) entry points
// were merged after the guard pass. A representative subset, each building an
// Id or a std::string key from kLong while allocation fails: each must return
// -1 (or no-op, for the void setters) instead of throwing.
static bool test_k1_k2_entry_points_do_not_throw() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    char buf[256];
    char place_before[256];
    SIM_CHECK(sim_world_rite_place(w, place_before, sizeof(place_before)) >= 0);

    g_fail_alloc = true;
    const int knows = sim_world_knows_rite(w, kLong);
    const int learn_teacher = sim_world_learn_rite_from_teacher(w, kLong, kLong);
    const int learn_text = sim_world_learn_rite_from_text(w, kLong, kLong);
    const int taught_by = sim_world_rites_taught_by(w, kLong, buf, sizeof(buf));
    const int taught_in = sim_world_rites_taught_in(w, kLong, buf, sizeof(buf));
    sim_world_set_rite_place(w, kLong);
    const int perform = sim_world_perform_rite(w, kLong, kLong, buf, sizeof(buf));
    const int64_t ward_until = sim_world_ward_until(w, kLong);
    const int warded = sim_world_warded(w, kLong);
    const int npc_task = sim_world_npc_task_at(w, kLong, 6, buf, sizeof(buf));
    const int npc_place = sim_world_npc_place_at(w, kLong, 6, buf, sizeof(buf));
    const int npc_schedule = sim_world_npc_schedule_at(w, kLong, 6, buf, sizeof(buf));
    // Day 1 is New Waters (festivals.csv): the market lookup reads its row.
    const int market = sim_world_market_open(w);
    // No allocation on these paths today; called so a future one cannot throw.
    sim_world_set_purity(w, 50);
    const int purity = sim_world_purity(w);
    const int omens = sim_world_omen_count(w);
    const int omen = sim_world_omen(w, 0, buf, sizeof(buf));
    const int festival = sim_world_is_festival(w);
    sim_world_destroy(nullptr);
    g_fail_alloc = false;

    SIM_CHECK_EQ(knows, -1);
    SIM_CHECK_EQ(learn_teacher, -1);
    SIM_CHECK_EQ(learn_text, -1);
    SIM_CHECK_EQ(taught_by, -1);
    SIM_CHECK_EQ(taught_in, -1);
    SIM_CHECK_EQ(perform, -1);
    SIM_CHECK_EQ(ward_until, int64_t{-1});
    SIM_CHECK_EQ(warded, -1);
    SIM_CHECK_EQ(npc_task, -1);
    SIM_CHECK_EQ(npc_place, -1);
    SIM_CHECK_EQ(npc_schedule, -1);
    SIM_CHECK_EQ(market, -1);
    SIM_CHECK_EQ(purity, 50);
    SIM_CHECK_EQ(omens, 0);
    SIM_CHECK_EQ(omen, -1);  // no omen at index 0
    SIM_CHECK_EQ(festival, 1);

    // The failed setter left the rite place as it was, and the world still works.
    char place_after[256];
    SIM_CHECK(sim_world_rite_place(w, place_after, sizeof(place_after)) >= 0);
    SIM_CHECK_EQ(std::string(place_after), std::string(place_before));
    SIM_CHECK_EQ(sim_world_knows_rite(w, kLong), 0);
    sim_world_destroy(w);
    return true;
}

// A7 + A14 (quests, journal, dialogue, people, events, calendar): every new
// entry point under failing allocation returns its error value; the calendar
// reads that never allocate keep answering.
static bool test_a7_entry_points_do_not_throw() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    char buf[256], buf2[256];
    int64_t day = 0;
    int y = 0, m = 0, d = 0;

    g_fail_alloc = true;
    const int results[] = {
        sim_world_quest_count(w, "available"),
        sim_world_quest_at(w, "available", 0, buf, sizeof(buf)),
        sim_world_quest_title(w, kLong, buf, sizeof(buf)),
        sim_world_quest_giver(w, kLong, buf, sizeof(buf)),
        sim_world_quest_kind(w, kLong, buf, sizeof(buf)),
        sim_world_quest_act(w, kLong, buf, sizeof(buf)),
        sim_world_quest_stage(w, kLong, buf, sizeof(buf)),
        static_cast<int>(sim_world_quest_deadline(w, kLong)),
        sim_world_quest_accept(w, kLong),
        sim_world_quest_advance(w, kLong, kLong, kLong),
        sim_world_quest_complete(w, kLong),
        sim_world_quest_abandon(w, kLong),
        sim_world_journal_count(w, kLong),
        sim_world_journal_at(w, kLong, 0, &day, buf, sizeof(buf), buf2, sizeof(buf2)),
        sim_world_dialogue_count(w, kLong),
        sim_world_dialogue_at(w, kLong, 0, buf, sizeof(buf), buf2, sizeof(buf2)),
        sim_world_npc_name(w, kLong, buf, sizeof(buf)),
        sim_world_npc_memory_count(w, kLong),
        sim_world_npc_memory_at(w, kLong, 0, &day, buf, sizeof(buf), buf2, sizeof(buf2)),
        sim_world_event_at(w, 0, &day, buf, sizeof(buf), buf2, sizeof(buf2)),
        sim_world_set_need(w, kLong, "hunger", 50),
    };
    const int date = sim_world_date(w, &y, &m, &d);
    const int month = sim_world_month_name(w, buf, sizeof(buf));
    const int phase = sim_world_moon_phase(w, buf, sizeof(buf));
    const int lit = sim_world_moon_illumination(w);
    const int omen = sim_world_day_omen(w, buf, sizeof(buf));
    const int observance = sim_world_day_observance(w, buf, sizeof(buf));
    g_fail_alloc = false;

    for (int r : results) SIM_CHECK_EQ(r, -1);
    SIM_CHECK_EQ(date, 0);
    SIM_CHECK(month > 0 && phase > 0 && omen > 0 && observance > 0);
    SIM_CHECK_EQ(lit, 0);
    // The world still works after the storm.
    SIM_CHECK(sim_world_quest_count(w, "available") > 30);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_no_exception_escapes_the_c_boundary, test_k1_k2_entry_points_do_not_throw,
         test_a7_entry_points_do_not_throw)
