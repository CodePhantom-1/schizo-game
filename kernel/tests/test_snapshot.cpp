// test_snapshot.cpp — the save file (sim/Snapshot.hpp contract): world state
// round-trips byte-for-byte, restored worlds keep advancing identically, a
// malformed save throws, and every module's state is actually covered.
#include "sim/Snapshot.hpp"
#include "sim/World.hpp"

#include "sim/Test.hpp"

#include <stdexcept>
#include <string>

using namespace sim;

namespace {

// Pushes state through every module so the save/restore exercises the whole
// struct, not just the modules the daily tick alone would touch.
void touch_everything(WorldState& w) {
    add_standing(w.faction, "the_empire", 35);
    add_standing(w.faction, "the_barbarians", -20);
    swear(w.faction, "player", "the_empire", w.day);

    add_favour(w.magic, "inanna", 15);
    w.magic.place = "temple:city_of_the_moon";
    perform_rite(w.context(), w.magic, "barutu_haruspicy", RiteInputs{});  // refused, but exercises the path

    w.population.knows["the_prophet"].push_back("the_warchief");
    witness(w.population, "the_prophet", "city_of_the_moon",
            "the Great Lighthouse burns sea-gem oil tonight, \"they say\"\nnew line", w.day);

    report_crime(w.justice, "the_warchief", "", "theft", w.day, "the_prophet");
    hold_hearing(w.context(), w.justice, w.justice.open_crimes.front().id);

    Asset& a = buy_asset(w.property, "field", "player", "city_of_jewels", 5, true);
    issue_loan(w.property, "player", "the_prophet", 1000, 20, w.day, 90);
    credit_purse(w.property, "player", 250);
    (void)a;

    accept(w.quests, "", w.day);  // exercises the machinery even with no canon defs yet
}

}  // namespace

static bool test_save_load_round_trips() {
    WorldState a;
    a.init("../db/canon", 42);
    touch_everything(a);
    a.advance_days(400);

    const std::string s = save_world(a);
    SIM_CHECK(s.substr(0, 9) == std::string("SIMSAVE 2"));

    WorldState b;
    load_world(b, "../db/canon", s);
    const std::string s2 = save_world(b);
    SIM_CHECK(s == s2);

    // The restored world matches the source world field-for-field where it's
    // cheap to compare directly (map equality is defined and deterministic).
    SIM_CHECK_EQ(a.day, b.day);
    SIM_CHECK(a.economy.market_by_city == b.economy.market_by_city);
    SIM_CHECK(a.population.npcs.size() == b.population.npcs.size());
    SIM_CHECK(a.faction.standing_by_faction == b.faction.standing_by_faction);
    SIM_CHECK(a.property.purse_by_owner == b.property.purse_by_owner);
    return true;
}

static bool test_restored_world_advances_identically() {
    WorldState a;
    a.init("../db/canon", 42);
    touch_everything(a);
    a.advance_days(400);

    WorldState b;
    load_world(b, "../db/canon", save_world(a));

    a.advance_days(300);
    b.advance_days(300);
    SIM_CHECK(save_world(a) == save_world(b));
    return true;
}

static bool test_malformed_save_throws() {
    WorldState w;
    bool threw = false;
    try {
        load_world(w, "../db/canon", "not a save at all");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    SIM_CHECK(threw);

    threw = false;
    try {
        load_world(w, "../db/canon", "SIMSAVE 2\nday\tnot_a_number\n");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    SIM_CHECK(threw);

    threw = false;
    try {
        load_world(w, "../db/canon", "SIMSAVE 99\n");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    SIM_CHECK(threw);
    return true;
}

// load_world must be atomic: a truncated/corrupt save throws without
// disturbing the caller's live world.
static bool test_load_world_is_atomic_on_failure() {
    WorldState live;
    live.init("../db/canon", 42);
    touch_everything(live);
    live.advance_days(50);
    const std::string before = save_world(live);

    bool threw = false;
    try {
        // Well-formed header and a few fields, then truncated mid-stream —
        // reaches deep into the parse before failing.
        load_world(live, "../db/canon",
                    "SIMSAVE 2\nday\t5\nseed\t42\nrng\t1\ndrought_stage\t0\n");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    SIM_CHECK(threw);
    SIM_CHECK(save_world(live) == before);
    return true;
}

// Wave-2 review: the fields added in wave 2 must survive a save/load, not just
// change the bytes (quest rewards, journal, npc role, crime stage/hearing day,
// verdict tablet/compensation, outlawry).
static bool test_wave2_fields_round_trip() {
    WorldState w;
    w.init("../db/canon", 9);
    SIM_CHECK(!w.quests.defs.empty());
    w.quests.defs.front().reward_silver = 77;
    w.quests.defs.front().reward_faction = "the_empire";
    w.quests.defs.front().reward_standing = 4;
    w.quests.journal["q_x"].push_back(JournalEntry{3, "met", "a line\twith tab"});
    SIM_CHECK(!w.population.npcs.empty());
    w.population.npcs.front().role = "baker";
    Crime& c = report_crime(w.justice, "player", "theft", "theft", w.day, "");
    c.stage = "detained";
    c.hearing_day = 12;
    w.justice.verdicts.push_back(Hearing{"crime_x", 5, "compensation", "verdict_tablet_crime_x", 50});
    w.faction.outlawed_by_faction["the_empire"] = true;

    WorldState r;
    load_world(r, "../db/canon", save_world(w));
    SIM_CHECK_EQ(r.quests.defs.front().reward_silver, Silver{77});
    SIM_CHECK_EQ(r.quests.defs.front().reward_faction, std::string("the_empire"));
    SIM_CHECK_EQ(r.quests.defs.front().reward_standing, 4);
    SIM_CHECK_EQ(r.quests.journal["q_x"].front().text, std::string("a line\twith tab"));
    SIM_CHECK_EQ(r.population.npcs.front().role, std::string("baker"));
    SIM_CHECK_EQ(r.justice.open_crimes.back().stage, std::string("detained"));
    SIM_CHECK_EQ(r.justice.open_crimes.back().hearing_day, DayNumber{12});
    SIM_CHECK_EQ(r.justice.verdicts.back().tablet_id, std::string("verdict_tablet_crime_x"));
    SIM_CHECK_EQ(r.justice.verdicts.back().compensation_paid, Silver{50});
    SIM_CHECK(r.faction.outlawed_by_faction["the_empire"]);
    SIM_CHECK(save_world(r) == save_world(w));
    return true;
}

// A field-coverage guard: mutating each module's state changes the save
// bytes. A field the writer forgot would leave the save identical.
static bool test_every_module_changes_the_save_bytes() {
    WorldState base;
    base.init("../db/canon", 5);
    const std::string s0 = save_world(base);

    {
        WorldState w = base;
        w.day += 1;
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        w.rng.set_state(w.rng.state() + 1);
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        w.facts.drought_stage = 2;
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        deliver(w.economy, "city_of_the_moon", "grain", 10);
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        witness(w.population, "the_prophet", "self", "a private thought", w.day);
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        add_standing(w.faction, "the_empire", 5);
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        add_favour(w.magic, "inanna", 5);
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        report_crime(w.justice, "the_warchief", "", "theft", w.day, "the_prophet");
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        buy_asset(w.property, "field", "player", "city_of_jewels", 5, true);
        SIM_CHECK(save_world(w) != s0);
    }
    {
        WorldState w = base;
        accept(w.quests, "some_quest", w.day);
        SIM_CHECK(save_world(w) != s0);
    }
    return true;
}


// Bug-review 2026-09-25: an NPC row's memory count came from the file and
// went straight into vector::reserve — a corrupt/hostile save (e.g. through
// sim_world_load_from_buffer) asked for petabytes. Under ASan that aborts the
// process instead of the documented std::runtime_error.
static bool test_huge_section_count_throws_cleanly() {
    WorldState w;
    w.init("../db/canon", 42);
    std::string data = save_world(w);
    // Rewrite the first NPC row's memory count (field 8 of 9) to 1e15.
    const std::size_t npcs = data.find("POP_NPCS\t");
    SIM_CHECK(npcs != std::string::npos);
    const std::size_t row = data.find('\n', npcs) + 1;
    const std::size_t eol = data.find('\n', row);
    std::size_t tab = row;
    for (int i = 0; i < 7; ++i) tab = data.find('\t', tab) + 1;
    const std::size_t end = data.find('\t', tab);
    SIM_CHECK(end < eol);
    data.replace(tab, end - tab, "1000000000000000");
    bool threw = false;
    try {
        WorldState out;
        load_world(out, "../db/canon", data);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    SIM_CHECK(threw);
    return true;
}

SIM_MAIN(test_wave2_fields_round_trip, test_save_load_round_trips,
         test_restored_world_advances_identically,
         test_malformed_save_throws,
         test_load_world_is_atomic_on_failure,
         test_every_module_changes_the_save_bytes,
         test_huge_section_count_throws_cleanly)
