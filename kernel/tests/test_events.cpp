// test_events.cpp — the trigger engine over the WorldFacts.
// The real events.csv is EMPTY at Wave 1 (header only), so the rule-loading and
// firing paths are additionally exercised against a synthetic canon directory
// written to the system temp dir at runtime; the repo's db/ is never touched.
#include "sim/Context.hpp"

#include "sim/Test.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
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

    World() = default;
    // Calendar is non-movable (const member), so configured calendars are
    // built in place at construction.
    explicit World(const CalendarConfig& cfg) : cal(cfg) {}

    WorldContext ctx(DayNumber day = 1) {
        return WorldContext{db,   rng,      day,      cal,     facts,    economy, population,
                            faction, magic, justice, events, property, quests};
    }
};

EventRule make_rule(Id id, std::vector<std::string> triggers, bool repeatable = false) {
    EventRule rule;
    rule.id = std::move(id);
    rule.category = "test";
    rule.triggers = std::move(triggers);
    rule.repeatable = repeatable;
    return rule;
}

std::size_t fires_of(const EventsState& s, const Id& rule_id) {
    const auto it = s.fire_count_by_rule.find(rule_id);
    return it == s.fire_count_by_rule.end() ? 0 : static_cast<std::size_t>(it->second);
}

bool state_bytes_equal(const EventsState& a, const EventsState& b) {
    if (a.rules.size() != b.rules.size() || a.fired.size() != b.fired.size() ||
        a.fire_count_by_rule != b.fire_count_by_rule)
        return false;
    for (std::size_t i = 0; i < a.rules.size(); ++i) {
        if (a.rules[i].id != b.rules[i].id || a.rules[i].category != b.rules[i].category ||
            a.rules[i].triggers != b.rules[i].triggers ||
            a.rules[i].repeatable != b.rules[i].repeatable)
            return false;
    }
    for (std::size_t i = 0; i < a.fired.size(); ++i) {
        if (a.fired[i].rule_id != b.fired[i].rule_id || a.fired[i].day != b.fired[i].day ||
            a.fired[i].summary != b.fired[i].summary)
            return false;
    }
    return true;
}

}  // namespace

// --- Wave 1 reality: the canon table is empty ------------------------------

static bool test_empty_canon_loads_zero_rules_and_fires_nothing() {
    World w;
    w.db = Db::load("../db/canon");  // ctest runs from kernel/
    SIM_CHECK_EQ(w.db.rows("events").size(), std::size_t{0});

    load_rules(w.ctx(), w.events);
    SIM_CHECK_EQ(w.events.rules.size(), std::size_t{0});

    // Even a world on fire fires nothing: there are no rules to evaluate.
    w.facts.drought_stage = 5;
    w.facts.war_stage = 5;
    tick_events(w.ctx(), w.events, 30);
    SIM_CHECK(w.events.fired.empty());
    SIM_CHECK(w.events.fire_count_by_rule.empty());
    return true;
}

static bool test_load_rules_is_idempotent() {
    World w;
    w.db = Db::load("../db/canon");
    load_rules(w.ctx(), w.events);
    const EventsState snapshot = w.events;
    load_rules(w.ctx(), w.events);
    SIM_CHECK(state_bytes_equal(w.events, snapshot));
    return true;
}

// --- Rule loading against a synthetic canon (the real table is empty) ------

static bool test_load_rules_parses_rows_skips_open_and_reloads_clean() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "sim_events_test_canon";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream out(dir / "events.csv");
        out << "id,name,category,triggers,participants,consequences,tag,source_ref,repeat\n"
            << "ev_drought,Drought omens,omen,drought_gte:2;chance:4,,,CANON,wb,\n"
            << "ev_war,The raids begin,war,war_gte:1,,,CANON,wb,\n"
            << "ev_open,Unwritten event,omen,drought_gte:9,,,OPEN,notes (unwritten),\n"
            << "ev_festival,Festival games,festival,festival,,,CANON,wb,\n"
            << "ev_flood,The flood,disaster,drought_gte:3,,,CANON,wb,true\n";
    }
    World w;
    w.db = Db::load(dir.string());
    SIM_CHECK_EQ(w.db.rows("events").size(), std::size_t{5});  // Db loads every row
    load_rules(w.ctx(), w.events);
    fs::remove_all(dir);  // the checks below need no files

    // The OPEN row is an unwritten event: visible to the db, never a rule.
    SIM_CHECK_EQ(w.events.rules.size(), std::size_t{4});
    SIM_CHECK(!w.db.find("events", "ev_open").has_value() ||
              w.db.find("events", "ev_open")->at("tag") == "OPEN");

    const EventRule* drought = nullptr;
    const EventRule* flood = nullptr;
    for (const EventRule& r : w.events.rules) {
        if (r.id == "ev_drought") drought = &r;
        if (r.id == "ev_flood") flood = &r;
    }
    SIM_CHECK(drought != nullptr);
    SIM_CHECK(flood != nullptr);
    SIM_CHECK_EQ(drought->category, std::string("omen"));
    SIM_CHECK_EQ(drought->triggers.size(), std::size_t{2});
    SIM_CHECK_EQ(drought->triggers[0], std::string("drought_gte:2"));
    SIM_CHECK_EQ(drought->triggers[1], std::string("chance:4"));
    SIM_CHECK(!drought->repeatable);
    SIM_CHECK(flood->repeatable);  // its row says repeat

    // Idempotence with a non-empty table: reload yields the identical rules.
    const EventsState snapshot = w.events;
    load_rules(w.ctx(), w.events);
    SIM_CHECK(state_bytes_equal(w.events, snapshot));
    return true;
}

// --- Trigger evaluation ----------------------------------------------------

static bool test_drought_and_war_gates() {
    World w;
    w.events.rules.push_back(make_rule("drought_omens", {"drought_gte:2"}));
    w.events.rules.push_back(make_rule("war_drums", {"war_gte:1"}));

    w.facts.drought_stage = 1;  // below the gate
    tick_events(w.ctx(), w.events, 1);
    SIM_CHECK_EQ(fires_of(w.events, "drought_omens"), std::size_t{0});

    w.facts.drought_stage = 2;  // at the gate
    tick_events(w.ctx(), w.events, 1);
    SIM_CHECK_EQ(fires_of(w.events, "drought_omens"), std::size_t{1});
    SIM_CHECK(!w.events.fired.empty());
    SIM_CHECK_EQ(w.events.fired.back().rule_id, std::string("drought_omens"));
    SIM_CHECK_EQ(w.events.fired.back().day, DayNumber{1});

    tick_events(w.ctx(2), w.events, 1);  // a later day: fire-once holds
    SIM_CHECK_EQ(fires_of(w.events, "drought_omens"), std::size_t{1});

    SIM_CHECK_EQ(fires_of(w.events, "war_drums"), std::size_t{0});  // war_stage 0 < 1
    w.facts.war_stage = 1;
    tick_events(w.ctx(3), w.events, 1);
    SIM_CHECK_EQ(fires_of(w.events, "war_drums"), std::size_t{1});
    return true;
}

static bool test_conditions_and_together() {
    World w;
    w.events.rules.push_back(make_rule("double_calamity", {"drought_gte:2", "war_gte:3"}));

    w.facts.drought_stage = 5;
    w.facts.war_stage = 1;
    tick_events(w.ctx(), w.events, 1);
    SIM_CHECK_EQ(fires_of(w.events, "double_calamity"), std::size_t{0});

    w.facts.war_stage = 3;  // both gates now open
    tick_events(w.ctx(2), w.events, 1);
    SIM_CHECK_EQ(fires_of(w.events, "double_calamity"), std::size_t{1});
    return true;
}

static bool test_season_and_festival_triggers() {
    CalendarConfig cfg;
    // A full season set: with only one season configured, the wrap rule makes
    // every pre-start day that season's (test_time.cpp covers this).
    cfg.seasons = {{"sowing", 271}, {"harvest", 91}, {"vintage", 181}};
    cfg.festival_days = {10};

    World w{cfg};
    w.events.rules.push_back(make_rule("harvest_rite", {"season:harvest"}));
    w.events.rules.push_back(make_rule("festival_games", {"festival"}));

    tick_events(w.ctx(90), w.events, 2);  // day 90 sowing, day 91 harvest begins
    SIM_CHECK_EQ(fires_of(w.events, "harvest_rite"), std::size_t{1});
    SIM_CHECK_EQ(w.events.fired[0].rule_id, std::string("harvest_rite"));
    SIM_CHECK_EQ(w.events.fired[0].day, DayNumber{91});

    tick_events(w.ctx(9), w.events, 3);  // only day 10 is a festival day
    SIM_CHECK_EQ(fires_of(w.events, "festival_games"), std::size_t{1});
    SIM_CHECK_EQ(w.events.fired[1].day, DayNumber{10});

    // A calendar with no seasons never satisfies a season condition.
    World plain;
    plain.events.rules.push_back(make_rule("harvest_rite", {"season:harvest"}));
    tick_events(plain.ctx(), plain.events, 5);
    SIM_CHECK_EQ(fires_of(plain.events, "harvest_rite"), std::size_t{0});
    return true;
}

static bool test_malformed_and_unknown_conditions_never_fire() {
    World w;
    w.facts.drought_stage = 9;
    w.facts.war_stage = 9;
    w.events.rules.push_back(make_rule("bad_kind", {"meteor_strike:1"}));
    w.events.rules.push_back(make_rule("no_arg", {"drought_gte"}));
    w.events.rules.push_back(make_rule("empty_arg", {"chance:"}));
    w.events.rules.push_back(make_rule("text_arg", {"chance:abc"}));
    w.events.rules.push_back(make_rule("no_colon", {"chance"}));
    w.events.rules.push_back(make_rule("zero_chance", {"chance:0"}));
    w.events.rules.push_back(make_rule("negative_chance", {"chance:-3"}));
    tick_events(w.ctx(), w.events, 3);
    for (const char* id : {"bad_kind", "no_arg", "empty_arg", "text_arg", "no_colon",
                           "zero_chance", "negative_chance"})
        SIM_CHECK_EQ(fires_of(w.events, id), std::size_t{0});
    return true;
}

static bool test_empty_condition_list_is_a_vacuous_and() {
    // Mechanical AND semantics: no conditions means nothing blocks it, and the
    // fire-once invariant caps a rule that would otherwise fire every day.
    World w;
    w.events.rules.push_back(make_rule("unconditioned", {}));
    tick_events(w.ctx(), w.events, 4);
    SIM_CHECK_EQ(fires_of(w.events, "unconditioned"), std::size_t{1});
    SIM_CHECK_EQ(w.events.fired.size(), std::size_t{1});
    return true;
}

// --- Chance rolls ----------------------------------------------------------

static bool test_chance_one_in_one_fires_every_day() {
    World w;
    w.events.rules.push_back(make_rule("certain", {"chance:1"}, /*repeatable=*/true));
    tick_events(w.ctx(), w.events, 5);
    SIM_CHECK_EQ(fires_of(w.events, "certain"), std::size_t{5});
    for (DayNumber d = 1; d <= 5; ++d) SIM_CHECK(w.events.fired[d - 1].day == d);
    return true;
}

static bool test_chance_is_deterministic_and_per_rule() {
    // Two identical worlds (same seed, same rules) must produce the identical
    // events state — the DoD's "same seed -> same state bytes" (no serializer
    // exists at Wave 1, so the state is compared field by field).
    World a;
    World b;
    for (World* w : {&a, &b}) {
        w->rng = Rng{7};
        w->events.rules.push_back(make_rule("coin_a", {"chance:2"}, /*repeatable=*/true));
        w->events.rules.push_back(make_rule("coin_b", {"chance:2"}, /*repeatable=*/true));
        tick_events(w->ctx(), w->events, 200);
    }
    SIM_CHECK(state_bytes_equal(a.events, b.events));

    // coin_a and coin_b must not share one stream: the rule salt makes their
    // daily rolls independent, so their fire-day sets differ.
    std::set<DayNumber> days_a;
    std::set<DayNumber> days_b;
    for (const TriggeredEvent& e : a.events.fired)
        (e.rule_id == "coin_a" ? days_a : days_b).insert(e.day);
    SIM_CHECK_EQ(days_a.size() + days_b.size(), a.events.fired.size());
    SIM_CHECK(!(days_a == days_b));

    // 1-in-2 over 200 days: sane band guards against always/never bugs
    // (deterministic for the fixed seed, so this cannot flake run to run).
    SIM_CHECK(days_a.size() >= 50 && days_a.size() <= 150);
    return true;
}

static bool test_repeatable_rules_fire_again_but_only_once_per_day() {
    World w;
    w.events.rules.push_back(make_rule("yearly_omen", {"drought_gte:1"}, /*repeatable=*/true));
    w.facts.drought_stage = 2;
    tick_events(w.ctx(), w.events, 4);
    SIM_CHECK_EQ(fires_of(w.events, "yearly_omen"), std::size_t{4});

    World once;
    once.events.rules.push_back(make_rule("one_time_omen", {"drought_gte:1"}));
    once.facts.drought_stage = 2;
    tick_events(once.ctx(), once.events, 4);
    SIM_CHECK_EQ(fires_of(once.events, "one_time_omen"), std::size_t{1});
    return true;
}

// --- Invariants over the fired log ----------------------------------------

static bool test_fired_log_is_ordered_by_day_and_counts_agree() {
    CalendarConfig cfg;
    cfg.festival_days = {10, 20};
    World w{cfg};
    w.events.rules.push_back(make_rule("harvest_rite", {"festival"}, /*repeatable=*/true));
    w.events.rules.push_back(make_rule("one_time_omen", {"drought_gte:1"}));
    w.facts.drought_stage = 1;

    tick_events(w.ctx(9), w.events, 13);  // days 9..21: festival on 10 and 20
    SIM_CHECK_EQ(w.events.fired.size(), std::size_t{3});  // day 9 omen, days 10+20 rite
    SIM_CHECK(std::is_sorted(w.events.fired.begin(), w.events.fired.end(),
                             [](const TriggeredEvent& x, const TriggeredEvent& y) {
                                 return x.day < y.day;
                             }));
    std::size_t total = 0;
    for (const auto& [id, count] : w.events.fire_count_by_rule) total += static_cast<std::size_t>(count);
    SIM_CHECK_EQ(total, w.events.fired.size());
    SIM_CHECK_EQ(w.events.fired[0].day, DayNumber{9});
    SIM_CHECK_EQ(w.events.fired[1].day, DayNumber{10});
    SIM_CHECK_EQ(w.events.fired[2].day, DayNumber{20});
    return true;
}

static bool test_tick_touches_only_the_events_state() {
    World w;
    w.events.rules.push_back(make_rule("certain", {"chance:1"}, /*repeatable=*/true));
    const WorldFacts facts_before = w.facts;
    const std::size_t markets_before = w.economy.market_by_city.size();
    const std::map<Id, std::int64_t> stock_before = w.economy.stock_by_city_item;
    const std::size_t npcs_before = w.population.npcs.size();
    const std::map<Id, std::vector<Id>> knows_before = w.population.knows;
    const std::size_t quest_defs_before = w.quests.defs.size();
    const std::size_t quests_active_before = w.quests.active.size();
    const std::vector<Id> quests_completed_before = w.quests.completed;

    tick_events(w.ctx(), w.events, 3);

    SIM_CHECK_EQ(w.facts.drought_stage, facts_before.drought_stage);
    SIM_CHECK_EQ(w.facts.war_stage, facts_before.war_stage);
    SIM_CHECK_EQ(w.economy.market_by_city.size(), markets_before);
    SIM_CHECK(w.economy.stock_by_city_item == stock_before);
    SIM_CHECK_EQ(w.population.npcs.size(), npcs_before);
    SIM_CHECK(w.population.knows == knows_before);
    SIM_CHECK_EQ(w.quests.defs.size(), quest_defs_before);
    SIM_CHECK_EQ(w.quests.active.size(), quests_active_before);
    SIM_CHECK(w.quests.completed == quests_completed_before);
    SIM_CHECK_EQ(fires_of(w.events, "certain"), std::size_t{3});  // the events state did move
    return true;
}

SIM_MAIN(test_empty_canon_loads_zero_rules_and_fires_nothing,
         test_load_rules_is_idempotent,
         test_load_rules_parses_rows_skips_open_and_reloads_clean,
         test_drought_and_war_gates,
         test_conditions_and_together,
         test_season_and_festival_triggers,
         test_malformed_and_unknown_conditions_never_fire,
         test_empty_condition_list_is_a_vacuous_and,
         test_chance_one_in_one_fires_every_day,
         test_chance_is_deterministic_and_per_rule,
         test_repeatable_rules_fire_again_but_only_once_per_day,
         test_fired_log_is_ordered_by_day_and_counts_agree,
         test_tick_touches_only_the_events_state)
