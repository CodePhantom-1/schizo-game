// test_festivals.cpp — K-2 end to end: the real canon's festivals land on the
// world calendar, bend the street's day, satisfy Magic's festival-day time
// power, survive a save/load, and reach the engine through the C API.
// Rite fixtures below are test scaffolding in the system temp dir, never canon.
#include "sim/CApi.h"
#include "sim/Magic.hpp"
#include "sim/Population.hpp"
#include "sim/Schedule.hpp"
#include "sim/Snapshot.hpp"
#include "sim/World.hpp"

#include "sim/Test.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

using namespace sim;

namespace {

const char* kCanon = "../db/canon";

WorldState real_world() {
    WorldState w;
    w.init(kCanon, 7);
    return w;
}

std::string npc_str(int (*fn)(const SimWorld*, const char*, int, char*, int), const SimWorld* w,
                    const char* npc, int hour) {
    char buf[512];
    const int n = fn(w, npc, hour, buf, sizeof(buf));
    return n < 0 ? std::string("<err>") : std::string(buf);
}

}  // namespace

// Every non-OPEN festivals.csv row is on the world calendar at its day_of_year,
// every year; the calendar's festivals are exactly the table's.
static bool test_real_canon_festivals_on_the_calendar() {
    const WorldState w = real_world();
    std::size_t rows = 0;
    for (const Row& r : w.db.rows("festivals")) {
        if (r.get("tag") == "OPEN") continue;
        ++rows;
        const DayNumber doy = std::stoll(r.get("day_of_year"));
        SIM_CHECK(w.cal.is_festival(doy));
        SIM_CHECK(w.cal.is_festival(doy + w.cal.days_per_year()));
        SIM_CHECK_EQ(w.cal.festival_id(doy), r.get("id"));
        // Each festival's place is a places.csv id or the "home" token.
        const std::string place = r.get("place");
        SIM_CHECK(place == "home" || w.db.has("places", place));
    }
    SIM_CHECK(rows > 0);
    SIM_CHECK_EQ(w.cal.festivals().size(), rows);
    // An ordinary day is not a festival.
    SIM_CHECK(!w.cal.is_festival(2));
    SIM_CHECK(w.cal.festival_id(2).empty());
    return true;
}

// Every place a real canon schedule row or person names resolves to a
// places.csv id (or is a home/work token).
static bool test_real_canon_places_resolve() {
    const WorldState w = real_world();
    for (const char* table : {"schedules", "person_schedules"})
        for (const Row& r : w.db.rows(table)) {
            const std::string p = r.get("place");
            SIM_CHECK(p.empty() || p == "home" || p == "work" || w.db.has("places", p));
        }
    for (const Row& r : w.db.rows("people"))
        for (const char* col : {"home_place", "work_place"}) {
            const std::string p = r.get(col);
            SIM_CHECK(p.empty() || w.db.has("places", p));
        }
    for (const Row& r : w.db.rows("person_schedules")) SIM_CHECK(w.db.has("people", r.get("person_id")));
    return true;
}

// The street on a festival: a baker goes to the gathering; the gatekeeper
// (exempt) keeps the gate; the market is closed or open per the row.
static bool test_real_canon_street_on_a_festival() {
    const WorldState w = real_world();
    for (const FestivalDef& f : w.cal.festivals()) {
        const auto bend = festival_bend(w.db, w.cal, f.day_of_year);
        SIM_CHECK(bend.has_value());
        SIM_CHECK(bend->attend_from < bend->attend_until);
        SIM_CHECK_EQ(market_open(w.db, w.cal, f.day_of_year), bend->market_open);
        // A non-exempt resident with an ordinary day is at the gathering (or
        // at their own festival row) at attend_from.
        const auto t = npc_task_at(w.db, w.cal, w.population, "ur_shara_baker", f.day_of_year,
                                   bend->attend_from);
        SIM_CHECK(t.has_value());
        SIM_CHECK_EQ(t->festival, f.id);
        // The exempt gatekeeper's hour is never the gathering.
        const auto g = npc_task_at(w.db, w.cal, w.population, "ur_utu_gatekeeper_dawn",
                                   f.day_of_year, bend->attend_from);
        SIM_CHECK(g.has_value());
        SIM_CHECK(g->origin != "festival");
    }
    // A plain day: the market trades.
    SIM_CHECK(market_open(w.db, w.cal, 2));
    return true;
}

// Per-person variation in the real canon: the two gatekeepers share a role
// but not a day.
static bool test_real_canon_gatekeepers_split_the_day() {
    const WorldState w = real_world();
    const auto dawn_6 = npc_task_at(w.db, w.cal, w.population, "ur_utu_gatekeeper_dawn", 2, 6);
    const auto dusk_6 = npc_task_at(w.db, w.cal, w.population, "sin_iddinam_gatekeeper_dusk", 2, 6);
    const auto dawn_18 = npc_task_at(w.db, w.cal, w.population, "ur_utu_gatekeeper_dawn", 2, 18);
    const auto dusk_18 = npc_task_at(w.db, w.cal, w.population, "sin_iddinam_gatekeeper_dusk", 2, 18);
    SIM_CHECK(dawn_6 && dusk_6 && dawn_18 && dusk_18);
    SIM_CHECK_EQ(dawn_6->schedule_id, Id("gates_open_at_dawn"));
    SIM_CHECK_EQ(dawn_6->place, std::string("moon_gate_place"));
    SIM_CHECK_EQ(dusk_6->origin, std::string("person"));
    SIM_CHECK_EQ(dusk_18->schedule_id, Id("gates_close_at_dusk"));
    SIM_CHECK_EQ(dawn_18->origin, std::string("person"));
    // The leaders have no schedule at all.
    SIM_CHECK(!npc_task_at(w.db, w.cal, w.population, "law_giver", 2, 12).has_value());
    return true;
}

// Magic.hpp: the time power (0.10) is satisfied by a festival day when the
// rite demands one — the festival days now come from festivals.csv through
// WorldState::init (Magic itself is untouched).
static bool test_magic_festival_time_power_from_the_calendar() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "schizo_game_test_festivals_magic";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "rites.csv");
        out << "id,name,tradition,deity,materials,place,time_window,effect_family,effect_tag,tag,"
               "source_ref\n"
            << "festival_rite_fixture,Fixture rite,fixture,inanna,honey,,on its festival day,"
               "blessing (fixture),INVENTED: EFFECT,CANON,tests/test_festivals.cpp\n";
        std::ofstream f(dir / "festivals.csv");
        f << "id,name,day_of_year,deity,place,attend_from,attend_until,exempt_roles,market,"
             "gathering,description,tag,source_ref\n"
          << "fixture_torch,Fixture Torch,5,inanna,home,4,9,,closed,gathers,test,INVENTED,test\n";
    }
    auto score_on = [&](DayNumber day) {
        WorldState w;
        w.init(dir.string(), 1);
        w.day = day;
        w.magic.place = "riverbank";
        RiteInputs in;
        in.materials_held["honey"] = 1;
        in.performer_knows_rite = true;
        return perform_rite(w.context(), w.magic, "festival_rite_fixture", in).score;
    };
    const double off = score_on(4);
    const double on = score_on(5);
    const double next_year = score_on(365);
    SIM_CHECK(std::fabs((on - off) - 0.10) < 1e-9);
    SIM_CHECK(std::fabs(next_year - on) < 1e-9);
    std::filesystem::remove_all(dir);
    return true;
}

// Festivals and per-person plans are pure over canon + day: a save/load
// round trip (no new state) answers identically.
static bool test_festival_answers_survive_save_load() {
    WorldState a;
    a.init(kCanon, 11);
    a.advance_days(179);  // day 180; tomorrow is the First-Cutting Procession
    WorldState b;
    load_world(b, kCanon, save_world(a));
    for (int step = 0; step < 3; ++step) {
        SIM_CHECK_EQ(a.day, b.day);
        SIM_CHECK_EQ(a.cal.festival_id(a.day), b.cal.festival_id(b.day));
        for (const Npc& n : a.population.npcs)
            for (int h = 0; h < 24; ++h) {
                const auto x = npc_task_at(a.db, a.cal, a.population, n.id, a.day, h);
                const auto y = npc_task_at(b.db, b.cal, b.population, n.id, b.day, h);
                SIM_CHECK_EQ(x.has_value(), y.has_value());
                if (x) {
                    SIM_CHECK_EQ(x->schedule_id, y->schedule_id);
                    SIM_CHECK_EQ(x->place, y->place);
                }
            }
        a.advance_days(1);
        b.advance_days(1);
    }
    SIM_CHECK_EQ(save_world(a), save_world(b));
    return true;
}

static bool test_c_api_festivals_and_people() {
    SimWorld* w = sim_world_create(kCanon, 3);
    SIM_CHECK(w != nullptr);
    char buf[256];

    // Day 1 is New Waters (festivals.csv).
    SIM_CHECK_EQ(sim_world_is_festival(w), 1);
    SIM_CHECK_EQ(sim_world_festival(w, buf, sizeof(buf)), 10);
    SIM_CHECK_EQ(std::string(buf), std::string("new_waters"));
    SIM_CHECK_EQ(sim_world_market_open(w), 0);
    SIM_CHECK_EQ(sim_world_is_festival_day(w, 181), 1);
    SIM_CHECK(sim_world_festival_on(w, 181, buf, sizeof(buf)) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("first_cutting_procession"));
    SIM_CHECK_EQ(sim_world_festival_on(w, 2, buf, sizeof(buf)), 0);
    SIM_CHECK_EQ(std::string(buf), std::string(""));

    // The baker at New Waters' gathering hour, then on an ordinary day.
    SIM_CHECK_EQ(npc_str(sim_world_npc_schedule_at, w, "ur_shara_baker", 9), std::string("new_waters"));
    SIM_CHECK_EQ(npc_str(sim_world_npc_place_at, w, "ur_shara_baker", 9),
                 std::string("temple_front_place"));
    sim_world_advance_days(w, 1);
    SIM_CHECK_EQ(sim_world_is_festival(w), 0);
    SIM_CHECK_EQ(sim_world_market_open(w), 1);
    SIM_CHECK_EQ(npc_str(sim_world_npc_schedule_at, w, "ur_shara_baker", 3),
                 std::string("baker_fires_the_oven"));
    SIM_CHECK_EQ(npc_str(sim_world_npc_place_at, w, "ur_shara_baker", 3), std::string("bakery_place"));
    SIM_CHECK(npc_str(sim_world_npc_task_at, w, "ur_shara_baker", 3).find("oven") != std::string::npos);

    // Enumerating the people.
    const int n = sim_world_npc_count(w);
    SIM_CHECK(n > 0);
    bool saw_baker = false;
    for (int i = 0; i < n; ++i) {
        SIM_CHECK(sim_world_npc_id(w, i, buf, sizeof(buf)) > 0);
        if (std::string(buf) == "ur_shara_baker") saw_baker = true;
    }
    SIM_CHECK(saw_baker);
    SIM_CHECK_EQ(sim_world_npc_id(w, n, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_npc_id(w, -1, buf, sizeof(buf)), -1);

    // Errors: unknown npc, npc with no schedule, null arguments.
    SIM_CHECK_EQ(sim_world_npc_task_at(w, "nobody", 9, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_npc_task_at(w, "law_giver", 9, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_npc_task_at(w, nullptr, 9, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_is_festival(nullptr), -1);
    SIM_CHECK_EQ(sim_world_is_festival_day(w, 0), -1);
    SIM_CHECK_EQ(sim_world_market_open(nullptr), -1);
    SIM_CHECK_EQ(sim_world_festival(nullptr, buf, sizeof(buf)), -1);

    // Through a save buffer: the loaded world answers the same.
    std::string save(static_cast<std::size_t>(1) << 20, '\0');
    const int wrote = sim_world_save_to_buffer(w, save.data(), static_cast<int>(save.size()));
    SIM_CHECK(wrote > 0 && wrote < static_cast<int>(save.size()));
    SimWorld* v = sim_world_load_from_buffer(kCanon, save.c_str());
    SIM_CHECK(v != nullptr);
    SIM_CHECK_EQ(npc_str(sim_world_npc_place_at, v, "ur_shara_baker", 3), std::string("bakery_place"));
    SIM_CHECK_EQ(sim_world_is_festival(v), 0);
    sim_world_destroy(v);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_real_canon_festivals_on_the_calendar,
         test_real_canon_places_resolve,
         test_real_canon_street_on_a_festival,
         test_real_canon_gatekeepers_split_the_day,
         test_magic_festival_time_power_from_the_calendar,
         test_festival_answers_survive_save_load,
         test_c_api_festivals_and_people)
