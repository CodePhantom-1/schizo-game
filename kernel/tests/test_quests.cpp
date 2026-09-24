// test_quests.cpp — the quest state machine: accept, complete, fail-on-deadline.
// The real db/canon/quests.csv is EMPTY at Wave 1 (header only), so def loading
// is additionally exercised against a synthetic canon directory written to the
// system temp dir at runtime; the repo's db/ is never touched.
#include "sim/Context.hpp"

#include "sim/Test.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace sim;

namespace {

// A minimal world: every module state at its default, one rng, no calendar
// extras — the same shape test_contracts.cpp instantiates the seam with.
struct World {
    Db db{};
    Rng rng{1};
    Calendar cal{};
    WorldFacts facts{};
    EconomyState economy;
    PopulationState population;
    FactionState faction;
    MagicState magic;
    JusticeState justice;
    EventsState events;
    PropertyState property;
    QuestState quests;

    WorldContext ctx(DayNumber day = 1) {
        return WorldContext{db,   rng,      day,      cal,     facts,    economy, population,
                            faction, magic, justice, events, property, quests};
    }
};

// Byte-for-byte equality of the observable quest state: the determinism check
// (same seed -> same state bytes) compares these fields, nothing else.
bool state_bytes_equal(const QuestState& a, const QuestState& b) {
    if (a.defs.size() != b.defs.size() || a.active.size() != b.active.size() ||
        a.completed != b.completed || a.failed_list != b.failed_list)
        return false;
    for (std::size_t i = 0; i < a.defs.size(); ++i) {
        if (a.defs[i].id != b.defs[i].id || a.defs[i].kind != b.defs[i].kind ||
            a.defs[i].deadline_days != b.defs[i].deadline_days)
            return false;
    }
    for (std::size_t i = 0; i < a.active.size(); ++i) {
        if (a.active[i].def_id != b.active[i].def_id ||
            a.active[i].accepted != b.active[i].accepted ||
            a.active[i].deadline != b.active[i].deadline ||
            a.active[i].failed != b.active[i].failed ||
            a.active[i].stage != b.active[i].stage)
            return false;
    }
    return true;
}

// Loads the synthetic canon into a world. The ids are test fixtures, not canon.
World world_with_test_defs() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "sim_quests_test_canon";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        // The Wave 1 quests.csv header plus a deadline_days column: the def
        // loader must take its deadline from data, never from the header's
        // absence.
        std::ofstream out(dir / "quests.csv");
        out << "id,name,giver,act,kind,tag,source_ref,deadline_days\n"
            << "qtest_arc,Test arc quest,tester,act1,history_arc,CANON,test,10\n"
            << "qtest_side,Test side quest,tester,act1,sourced,CANON,test,3\n"
            << "qtest_nodeadline,Test systemic quest,tester,act1,systemic,CANON,test,\n"
            << "qtest_broken,Bad deadline cell,tester,act1,systemic,CANON,test,soon\n"
            << "qtest_open,Unwritten quest,,act2,companion,OPEN,notes (unwritten),5\n"
            << "qtest_open_paren,Unwritten quest two,,act2,companion,OPEN (roster unwritten),notes,7\n";
    }
    World w;
    w.db = Db::load(dir.string());
    fs::remove_all(dir);
    load_defs(w.ctx(), w.quests);
    return w;
}

const QuestDef* find_def(const QuestState& state, const Id& def_id) {
    for (const QuestDef& def : state.defs)
        if (def.id == def_id) return &def;
    return nullptr;
}

}  // namespace

// --- Wave 1 reality: the canon table is empty ------------------------------

static bool test_empty_canon_loads_zero_defs_and_machine_still_runs() {
    World w;
    w.db = Db::load("../db/canon");  // ctest runs from kernel/
    SIM_CHECK_EQ(w.db.rows("quests").size(), std::size_t{0});

    load_defs(w.ctx(), w.quests);
    SIM_CHECK(w.quests.defs.empty());
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK(w.quests.completed.empty());
    SIM_CHECK(w.quests.failed_list.empty());

    // Ticking an empty world moves nothing.
    tick_quests(w.ctx(), w.quests, 30);
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK(w.quests.failed_list.empty());

    // The machinery is data-driven: with no canon defs at all it still runs,
    // and an id the canon does not represent gets no invented deadline (0).
    Quest& q = accept(w.quests, Id{"qtest_absent"}, DayNumber{3});
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK_EQ(q.accepted, DayNumber{3});
    SIM_CHECK_EQ(q.deadline, DayNumber{0});
    SIM_CHECK_EQ(q.stage, std::string("accepted"));

    tick_quests(w.ctx(1000), w.quests, 1);
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});  // no deadline: never times out

    complete(w.quests, Id{"qtest_absent"}, DayNumber{1001});
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.completed.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.completed.back(), std::string("qtest_absent"));
    return true;
}

// --- Def loading against a synthetic canon (the real table is empty) -------

static bool test_load_defs_parses_rows_skips_open_and_reloads_clean() {
    World w = world_with_test_defs();

    // Db loads every row; the loader resolves only the written ones.
    SIM_CHECK_EQ(w.db.rows("quests").size(), std::size_t{6});
    SIM_CHECK_EQ(w.quests.defs.size(), std::size_t{4});
    SIM_CHECK(find_def(w.quests, "qtest_open") == nullptr);
    SIM_CHECK(find_def(w.quests, "qtest_open_paren") == nullptr);  // "OPEN (...)" is OPEN too

    const QuestDef* arc = find_def(w.quests, "qtest_arc");
    const QuestDef* side = find_def(w.quests, "qtest_side");
    const QuestDef* nodeadline = find_def(w.quests, "qtest_nodeadline");
    const QuestDef* broken = find_def(w.quests, "qtest_broken");
    SIM_CHECK(arc != nullptr && side != nullptr && nodeadline != nullptr && broken != nullptr);

    SIM_CHECK_EQ(arc->kind, std::string("history_arc"));
    SIM_CHECK_EQ(arc->deadline_days, 10);
    SIM_CHECK_EQ(side->kind, std::string("sourced"));
    SIM_CHECK_EQ(side->deadline_days, 3);
    // An empty cell is "no deadline in canon" — not an invented default.
    SIM_CHECK_EQ(nodeadline->deadline_days, 0);
    // A non-numeric cell is a deadline the machine cannot honor: none.
    SIM_CHECK_EQ(broken->deadline_days, 0);

    // Idempotence: reload yields the identical defs.
    const QuestState snapshot = w.quests;
    load_defs(w.ctx(), w.quests);
    SIM_CHECK(state_bytes_equal(w.quests, snapshot));
    return true;
}

// --- accept -----------------------------------------------------------------

static bool test_accept_uses_def_deadline_and_returns_live_reference() {
    World w = world_with_test_defs();

    Quest& q = accept(w.quests, Id{"qtest_arc"}, DayNumber{5});
    SIM_CHECK(&q == &w.quests.active.back());  // the reference aliases the stored quest
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK_EQ(q.def_id, std::string("qtest_arc"));
    SIM_CHECK_EQ(q.accepted, DayNumber{5});
    SIM_CHECK_EQ(q.deadline, DayNumber{15});  // accepted day + def's deadline_days
    SIM_CHECK(!q.failed);
    SIM_CHECK_EQ(q.stage, std::string("accepted"));

    // The deadline is read per-accept from the def the state carries: a def
    // pushed after load (a later wave's content, or a test) drives it too.
    QuestDef extra;
    extra.id = "qtest_later_wave";
    extra.kind = "companion";
    extra.deadline_days = 2;
    w.quests.defs.push_back(extra);
    Quest& late = accept(w.quests, Id{"qtest_later_wave"}, DayNumber{100});
    SIM_CHECK_EQ(late.deadline, DayNumber{102});
    return true;
}

// --- tick_quests: fail-on-deadline ------------------------------------------

static bool test_deadline_day_is_still_live_the_day_after_fails() {
    World w = world_with_test_defs();

    accept(w.quests, Id{"qtest_side"}, DayNumber{1});  // deadline 1 + 3 = day 4
    SIM_CHECK_EQ(w.quests.active.front().deadline, DayNumber{4});

    tick_quests(w.ctx(4), w.quests, 1);  // the deadline day itself: still live
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK(w.quests.failed_list.empty());

    tick_quests(w.ctx(5), w.quests, 1);  // past it: the world doesn't wait
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.failed_list.front(), std::string("qtest_side"));

    tick_quests(w.ctx(6), w.quests, 1);  // already moved out: no double fail
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{1});
    return true;
}

static bool test_multiday_tick_fails_once_in_expiry_order() {
    World w = world_with_test_defs();

    accept(w.quests, Id{"qtest_side"}, DayNumber{1});  // deadline 4  -> fails day 5
    accept(w.quests, Id{"qtest_arc"}, DayNumber{2});   // deadline 12 -> fails day 13

    tick_quests(w.ctx(4), w.quests, 10);  // evaluates days 4..13
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{2});
    SIM_CHECK_EQ(w.quests.failed_list[0], std::string("qtest_side"));  // expiry order
    SIM_CHECK_EQ(w.quests.failed_list[1], std::string("qtest_arc"));

    tick_quests(w.ctx(14), w.quests, 5);  // nothing left to fail
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{2});
    return true;
}

static bool test_zero_or_absent_deadline_never_times_out() {
    World w = world_with_test_defs();

    accept(w.quests, Id{"qtest_nodeadline"}, DayNumber{1});  // def row: empty deadline cell
    accept(w.quests, Id{"qtest_broken"}, DayNumber{1});      // def row: non-numeric cell
    accept(w.quests, Id{"qtest_absent"}, DayNumber{1});      // no def row at all
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{3});

    tick_quests(w.ctx(2), w.quests, 36000);  // a hundred years
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{3});
    SIM_CHECK(w.quests.failed_list.empty());
    return true;
}

static bool test_tick_with_nonpositive_days_advances_nothing() {
    World w = world_with_test_defs();

    accept(w.quests, Id{"qtest_side"}, DayNumber{1});  // deadline 4, long past day 10
    tick_quests(w.ctx(10), w.quests, 0);
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    tick_quests(w.ctx(10), w.quests, -3);
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK(w.quests.failed_list.empty());
    return true;
}

// --- complete ---------------------------------------------------------------

static bool test_complete_records_completion_order_and_ignores_unknown() {
    World w = world_with_test_defs();

    accept(w.quests, Id{"qtest_side"}, DayNumber{1});
    accept(w.quests, Id{"qtest_arc"}, DayNumber{2});

    complete(w.quests, Id{"qtest_arc"}, DayNumber{6});  // before its deadline (12)
    SIM_CHECK_EQ(w.quests.completed.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.completed.front(), std::string("qtest_arc"));
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.active.front().def_id, std::string("qtest_side"));

    complete(w.quests, Id{"qtest_side"}, DayNumber{7});
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.completed.size(), std::size_t{2});  // completion order
    SIM_CHECK_EQ(w.quests.completed[1], std::string("qtest_side"));

    // Unknown id, and an id with nothing active: both move nothing.
    complete(w.quests, Id{"qtest_nope"}, DayNumber{8});
    complete(w.quests, Id{"qtest_side"}, DayNumber{8});
    SIM_CHECK_EQ(w.quests.completed.size(), std::size_t{2});
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK(w.quests.failed_list.empty());
    return true;
}

static bool test_completing_on_the_deadline_day_beats_the_tick() {
    World w = world_with_test_defs();

    accept(w.quests, Id{"qtest_side"}, DayNumber{1});  // deadline day 4
    complete(w.quests, Id{"qtest_side"}, DayNumber{4});  // handed in on the last day
    SIM_CHECK_EQ(w.quests.completed.size(), std::size_t{1});

    tick_quests(w.ctx(5), w.quests, 1);  // nothing active: the tick fails nothing
    SIM_CHECK(w.quests.failed_list.empty());
    return true;
}

// --- determinism ------------------------------------------------------------

static bool test_determinism_same_seed_same_state_bytes() {
    // Two fresh worlds, same seed, the same scenario: identical state bytes.
    const QuestState run_a = [&] {
        World w = world_with_test_defs();
        accept(w.quests, Id{"qtest_arc"}, DayNumber{1});
        accept(w.quests, Id{"qtest_side"}, DayNumber{2});
        accept(w.quests, Id{"qtest_absent"}, DayNumber{3});
        complete(w.quests, Id{"qtest_side"}, DayNumber{4});  // deadline day 5: on time
        tick_quests(w.ctx(5), w.quests, 8);  // qtest_arc times out on day 12
        return w.quests;
    }();
    const QuestState run_b = [&] {
        World w = world_with_test_defs();
        accept(w.quests, Id{"qtest_arc"}, DayNumber{1});
        accept(w.quests, Id{"qtest_side"}, DayNumber{2});
        accept(w.quests, Id{"qtest_absent"}, DayNumber{3});
        complete(w.quests, Id{"qtest_side"}, DayNumber{4});
        tick_quests(w.ctx(5), w.quests, 8);
        return w.quests;
    }();

    SIM_CHECK(state_bytes_equal(run_a, run_b));

    // And the scenario itself did what it should: one completed, one timed out,
    // the deadline-less quest still active.
    SIM_CHECK_EQ(run_a.completed.size(), std::size_t{1});
    SIM_CHECK_EQ(run_a.completed.front(), std::string("qtest_side"));
    SIM_CHECK_EQ(run_a.failed_list.size(), std::size_t{1});
    SIM_CHECK_EQ(run_a.failed_list.front(), std::string("qtest_arc"));
    SIM_CHECK_EQ(run_a.active.size(), std::size_t{1});
    SIM_CHECK_EQ(run_a.active.front().def_id, std::string("qtest_absent"));
    return true;
}

SIM_MAIN(test_empty_canon_loads_zero_defs_and_machine_still_runs,
         test_load_defs_parses_rows_skips_open_and_reloads_clean,
         test_accept_uses_def_deadline_and_returns_live_reference,
         test_deadline_day_is_still_live_the_day_after_fails,
         test_multiday_tick_fails_once_in_expiry_order,
         test_zero_or_absent_deadline_never_times_out,
         test_tick_with_nonpositive_days_advances_nothing,
         test_complete_records_completion_order_and_ignores_unknown,
         test_completing_on_the_deadline_day_beats_the_tick,
         test_determinism_same_seed_same_state_bytes)
