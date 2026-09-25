// test_justice.cpp — the crime → witness → hearing → sealed-verdict pipeline.
// Canon is read only through sim::Db; the real db/canon/laws.csv is
// header-only (law content is OPEN — Phase 5), so the laws-row path is
// exercised against a throwaway fixture canon in the system temp directory,
// clearly marked as a test fixture and never committed as canon.
#include "sim/Context.hpp"

#include "sim/Test.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cctype>
#include <map>
#include <string>

using namespace sim;

namespace {

// A context shaped like the real seam (test_contracts.cpp builds one the same
// way). Only the fields the justice pipeline actually reads are live: db, day.
struct CtxParts {
    Db db;
    Calendar cal{};
    WorldFacts facts{};
    Rng rng{7};
    EconomyState economy;
    PopulationState population;
    FactionState faction;
    MagicState magic;
    JusticeState justice;
    EventsState events;
    PropertyState property;
    QuestState quests;

    WorldContext ctx(DayNumber day) {
        return WorldContext{db,  rng,   day,     cal,      facts,    economy, population,
                            faction, magic, justice, events, property, quests};
    }
};

std::string serialize_state(const JusticeState& s) {
    std::string out = "next_id=" + std::to_string(s.next_id);
    for (const Crime& c : s.open_crimes) {
        out += "|crime:" + c.id + "," + c.criminal + "," + c.law_row + "," + c.crime_kind + "," +
               std::to_string(c.day) + "," + c.witnessed_by + "," + (c.atoned ? "1" : "0");
    }
    for (const Hearing& h : s.verdicts) {
        out += "|verdict:" + h.crime_id + "," + std::to_string(h.day) + "," + h.verdict;
    }
    return out;
}

// Writes a minimal laws canon (test fixture, not game canon) and loads it.
Db load_fixture_db() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "schizo_game_test_justice_fixture";
    std::filesystem::remove_all(dir);  // stale fixtures from earlier runs
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "laws.csv", std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write justice test fixture");
        out << "id,crime,penalty_options,jurisdiction,tag,source_ref\n"
            << "theft,theft of goods,confiscation;compensation,city_gates,CANON,test fixture\n"
            << "sorcery,malefic magic,death,crown,CANON,test fixture\n"
            << "sacrilege,offense against the temple,stockades,crown,CANON,test fixture\n"
            << "temple_theft,theft from the temple,compensation,temple,OPEN,test fixture\n"
            << "oath_breaking,broken oath,compensation,temple,OPEN (awaiting authored law),test fixture\n";
    }
    return Db::load(dir.string());
}

}  // namespace

static bool test_report_assigns_sequential_ids_and_find_crime() {
    JusticeState state;
    // Copies, not references: a further report_crime may reallocate the
    // docket and invalidate references into it.
    const Crime a = report_crime(state, "npc_eshnunna_baker", "theft", "theft", 5, "guard_larsa");
    const Crime b = report_crime(state, "player", "burglary", "burglary", 6, "");
    SIM_CHECK_EQ(a.id, Id("crime_1"));
    SIM_CHECK_EQ(b.id, Id("crime_2"));
    SIM_CHECK_EQ(state.next_id, 3);

    const Crime* found = find_crime(state, "crime_1");
    SIM_CHECK(found != nullptr);
    SIM_CHECK_EQ(found->criminal, Id("npc_eshnunna_baker"));
    SIM_CHECK_EQ(found->law_row, Id("theft"));
    SIM_CHECK_EQ(found->crime_kind, Id("theft"));
    SIM_CHECK_EQ(found->day, DayNumber{5});
    SIM_CHECK_EQ(found->witnessed_by, Id("guard_larsa"));
    SIM_CHECK_EQ(found->atoned, false);

    // The report returns a reference into the state: mutations through it are
    // the recorded state (atonement is carried until the hearing).
    Crime& mutable_a = report_crime(state, "npc_zerzura_priest", "sorcery", "sorcery", 7, "guard_larsa");
    mutable_a.atoned = true;
    SIM_CHECK(find_crime(state, "crime_3") != nullptr);
    SIM_CHECK_EQ(find_crime(state, "crime_3")->atoned, true);

    SIM_CHECK(find_crime(state, "crime_404") == nullptr);
    SIM_CHECK_EQ(state.open_crimes.size(), std::size_t{3});
    return true;
}

static bool test_unwitnessed_crimes_stay_open_through_ticks() {
    CtxParts parts;
    parts.db = Db::load("../db/canon");
    const WorldContext ctx = parts.ctx(10);

    JusticeState state;
    (void)report_crime(state, "npc_ugbaba", "burglary", "burglary", 5, "");       // unwitnessed
    (void)report_crime(state, "npc_eshnunna_baker", "theft", "theft", 5, "guard_larsa");

    tick_justice(ctx, state, 1);

    // The witnessed crime is heard; the unwitnessed one stays on the docket.
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{1});
    SIM_CHECK_EQ(state.verdicts[0].crime_id, Id("crime_2"));
    SIM_CHECK(find_crime(state, "crime_1") != nullptr);
    SIM_CHECK(find_crime(state, "crime_2") == nullptr);

    tick_justice(ctx, state, 5);  // no evidence found yet: still no hearing
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{1});
    SIM_CHECK(find_crime(state, "crime_1") != nullptr);

    // Evidence is found later; the hearing can then be held explicitly.
    const Hearing h = hold_hearing(ctx, state, "crime_1");
    SIM_CHECK_EQ(h.crime_id, Id("crime_1"));
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{2});
    SIM_CHECK(find_crime(state, "crime_1") == nullptr);
    return true;
}

static bool test_hearing_reads_the_real_theft_law() {
    const Db db = Db::load("../db/canon");
    // Guard against a wrong working directory masquerading as "laws is empty".
    SIM_CHECK(db.rows("deities").size() >= std::size_t{16});  // canon only grows
    // The storm authored real law rows: the hearing reads the theft row now.
    SIM_CHECK(db.has("laws", "theft"));
    const Row theft = *db.find("laws", "theft");
    // Shipped law: OPEN rows cannot ship and must never decide a verdict.
    SIM_CHECK(theft.get("tag") == "A" || theft.get("tag") == "CANON");

    CtxParts parts;
    parts.db = db;
    const WorldContext ctx = parts.ctx(10);

    JusticeState state;
    (void)report_crime(state, "npc_eshnunna_baker", "theft", "theft", 5, "guard_larsa");
    const Hearing h = hold_hearing(ctx, state, "crime_1");
    // D-011 mapping: the first ';'-option of penalty_options decides. Compute
    // it from the canon like the kernel does, instead of hardcoding content.
    std::string first = theft.get("penalty_options");
    const auto sep = first.find(';');
    if (sep != std::string::npos) first = first.substr(0, sep);
    // trim + lowercase, mirroring the kernel's own normalization
    const auto issp = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!first.empty() && issp(static_cast<unsigned char>(first.front()))) first.erase(first.begin());
    while (!first.empty() && issp(static_cast<unsigned char>(first.back()))) first.pop_back();
    for (auto& c : first) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    SIM_CHECK(!first.empty());
    SIM_CHECK(first == "compensation" || first == "confiscation" || first == "debt_service" ||
              first == "exile" || first == "death" || first == "dismissed");
    SIM_CHECK_EQ(h.verdict, Id(first));
    // A verdict is an event with a day: the day being ticked, not the crime day.
    SIM_CHECK_EQ(h.day, DayNumber{10});
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{1});
    SIM_CHECK_EQ(state.verdicts[0].verdict, Id("compensation"));
    SIM_CHECK_EQ(state.verdicts[0].day, DayNumber{10});
    SIM_CHECK(find_crime(state, "crime_1") == nullptr);
    return true;
}

static bool test_hearing_reads_law_rows() {
    CtxParts parts;
    parts.db = load_fixture_db();  // test fixture, not canon
    const WorldContext ctx = parts.ctx(3);

    struct Case {
        const char* law_row;
        const char* expected_verdict;
    };
    const Case cases[] = {
        // A usable row decides: the first penalty option, verbatim.
        {"theft", "confiscation"},           // "confiscation;compensation"
        {"sorcery", "death"},                // single option
        // An OPEN row is missing canon — it must not decide (schema: "OPEN rows
        // mark missing canon; they cannot ship").
        {"temple_theft", "compensation"},    // tag OPEN
        {"oath_breaking", "compensation"},   // tag "OPEN (...)"
        // No usable row at all: the compensation fallback.
        {"no_such_law", "compensation"},     // unknown id
        {"", "compensation"},                // unspecified law row
        {"sacrilege", "compensation"},       // "stockades" names no documented verdict
    };

    JusticeState state;
    for (const Case& c : cases) {
        const Crime& crime = report_crime(state, "npc_eshnunna_baker", c.law_row, "theft", 1, "guard_larsa");
        const Hearing h = hold_hearing(ctx, state, crime.id);
        SIM_CHECK_EQ(h.verdict, Id(c.expected_verdict));
        SIM_CHECK_EQ(h.day, DayNumber{3});
    }
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{7});
    SIM_CHECK(state.open_crimes.empty());
    SIM_CHECK_EQ(state.next_id, 8);
    return true;
}

static bool test_hearing_for_unknown_or_already_heard_crime_is_dismissed() {
    CtxParts parts;
    parts.db = Db::load("../db/canon");
    const WorldContext ctx = parts.ctx(4);

    JusticeState state;
    (void)report_crime(state, "player", "assault", "assault", 4, "guard_larsa");

    // Unknown id: dismissed, and nothing is sealed.
    const Hearing unknown = hold_hearing(ctx, state, "crime_404");
    SIM_CHECK_EQ(unknown.crime_id, Id("crime_404"));
    SIM_CHECK_EQ(unknown.verdict, Id("dismissed"));
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{0});
    SIM_CHECK_EQ(state.open_crimes.size(), std::size_t{1});

    // Already heard: dismissed too — a crime is sealed exactly once.
    const Hearing first = hold_hearing(ctx, state, "crime_1");
    SIM_CHECK_EQ(first.verdict, Id("compensation"));
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{1});
    const Hearing again = hold_hearing(ctx, state, "crime_1");
    SIM_CHECK_EQ(again.verdict, Id("dismissed"));
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{1});
    SIM_CHECK(state.open_crimes.empty());
    return true;
}

static bool test_tick_is_a_noop_when_nothing_is_awaiting_hearing() {
    CtxParts parts;
    parts.db = Db::load("../db/canon");
    const WorldContext ctx = parts.ctx(6);

    JusticeState state;
    tick_justice(ctx, state, 0);  // zero days: nothing happens
    SIM_CHECK(state.open_crimes.empty());
    SIM_CHECK(state.verdicts.empty());
    tick_justice(ctx, state, 3);  // empty docket: still nothing
    SIM_CHECK(state.verdicts.empty());

    (void)report_crime(state, "npc_a", "theft", "theft", 5, "guard_a");
    (void)report_crime(state, "npc_b", "theft", "theft", 5, "guard_b");
    tick_justice(ctx, state, 1);
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{2});
    // Hearings are held once: further ticks seal nothing new.
    tick_justice(ctx, state, 1);
    tick_justice(ctx, state, 3);
    SIM_CHECK_EQ(state.verdicts.size(), std::size_t{2});
    SIM_CHECK(state.open_crimes.empty());
    return true;
}

static bool test_same_seed_produces_same_state_bytes() {
    // A scripted pipeline run against the real canon. Justice draws no
    // randomness, but the contract's bar is byte-determinism for a given seed;
    // this catches accidental static state or order dependence.
    auto run = [](std::uint64_t seed) {
        CtxParts parts;
        parts.rng = Rng{seed};
        parts.db = Db::load("../db/canon");
        const WorldContext ctx = parts.ctx(10);

        JusticeState state;
        Crime& first = report_crime(state, "npc_eshnunna_baker", "theft", "theft", 5, "guard_larsa");
        first.atoned = true;
        (void)report_crime(state, "npc_ugbaba", "burglary", "burglary", 5, "");  // unwitnessed
        (void)report_crime(state, "player", "assault", "assault", 6, "witness_aima");
        tick_justice(ctx, state, 1);
        (void)report_crime(state, "npc_zerzura_priest", "sorcery", "sorcery", 11, "guard_larsa");
        tick_justice(ctx, state, 2);
        (void)hold_hearing(ctx, state, "crime_2");  // evidence found later
        return state;
    };

    const JusticeState run_a = run(7);
    const JusticeState run_b = run(7);
    SIM_CHECK_EQ(serialize_state(run_a), serialize_state(run_b));

    // And the run itself did the expected thing (so the comparison is not
    // vacuous): crime_1 and crime_3 on the first tick, crime_4 on the second,
    // crime_2 only once evidence is found. Each verdict is its law row's
    // first option (D-011), read from the canon the same way the kernel reads
    // it; the crime -> law_row mapping is this run's own setup above.
    const Db canon_db = Db::load("../db/canon");
    const std::map<Id, Id> law_row_by_crime = {
        {Id("crime_1"), Id("theft")}, {Id("crime_2"), Id("burglary")},
        {Id("crime_3"), Id("assault")}, {Id("crime_4"), Id("sorcery")}};
    SIM_CHECK_EQ(run_a.verdicts.size(), std::size_t{4});
    SIM_CHECK_EQ(run_a.verdicts[0].crime_id, Id("crime_1"));
    SIM_CHECK_EQ(run_a.verdicts[1].crime_id, Id("crime_3"));
    SIM_CHECK_EQ(run_a.verdicts[2].crime_id, Id("crime_4"));
    SIM_CHECK_EQ(run_a.verdicts[3].crime_id, Id("crime_2"));
    for (const Hearing& h : run_a.verdicts) {
        const Row row = *canon_db.find("laws", law_row_by_crime.at(h.crime_id));
        std::string first = row.get("penalty_options");
        const auto sep = first.find(';');
        if (sep != std::string::npos) first = first.substr(0, sep);
        SIM_CHECK_EQ(h.verdict, Id(first));
        SIM_CHECK_EQ(h.day, DayNumber{10});
    }
    SIM_CHECK(run_a.open_crimes.empty());
    SIM_CHECK_EQ(run_a.next_id, 5);
    return true;
}

SIM_MAIN(test_report_assigns_sequential_ids_and_find_crime,
         test_unwitnessed_crimes_stay_open_through_ticks,
         test_hearing_reads_the_real_theft_law,
         test_hearing_reads_law_rows,
         test_hearing_for_unknown_or_already_heard_crime_is_dismissed,
         test_tick_is_a_noop_when_nothing_is_awaiting_hearing,
         test_same_seed_produces_same_state_bytes)
