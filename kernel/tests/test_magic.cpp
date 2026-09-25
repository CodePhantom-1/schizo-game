// test_magic.cpp — the five powers of a rite (sim/Magic.hpp contract).
//
// Magic.hpp fixes the score formula and the single rng draw; these tests pin
// the documented behavior against the REAL db/canon (loaded through sim::Db,
// the only canon door) plus a small fixture canon dir for rows the real table
// cannot carry yet (a deity="any" rite with an empty place requirement, a
// festival-demanding time_window, an OPEN-tagged row). Fixture rows are test
// scaffolding only — they are not canon and ship nowhere.
//
// Verified rolls (D-022: Rng{seed}.fork(day).fork(stable_hash(rite)).below(10000),
// see Magic.hpp), in basis points:
//   seed  1, day 5, sacrifice_fish_sea_gems -> 3493  (< 8000: the full-good rite succeeds)
//   seed  1, day 5, any_god_blessing        -> 4729  (< 8000: succeeds)
//   seed 14, day 5, sacrifice_fish_sea_gems -> 9589  (>= 5000 and >= 3000: the weakened rite fails)
//   seed  7, day 5, sacrifice_fish_sea_gems -> 5807  (any outcome; used for consistency only)
//   seed 11, day 9, sacrifice_fish_sea_gems -> 5913  (determinism pair)
#include "sim/Context.hpp"

#include "sim/Test.hpp"

#include <climits>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>

using namespace sim;

namespace {

bool close(double a, double b) { return std::fabs(a - b) <= 1e-9; }

// MagicState serialized by hand (map order is defined) for byte-determinism checks.
std::string fingerprint(const MagicState& s) {
    std::string out = "purity=" + std::to_string(s.purity) + ";place=" + s.place + ";favour=";
    for (const auto& [deity, value] : s.favour_by_deity) {
        out += deity + ":" + std::to_string(value) + ",";
    }
    return out;
}

// The full WorldContext, every state fresh, ctx referencing the members above it.
struct World {
    Db db;
    Rng rng;
    Calendar cal;
    WorldFacts facts;
    EconomyState economy;
    PopulationState population;
    FactionState faction;
    MagicState magic;
    JusticeState justice;
    EventsState events;
    PropertyState property;
    QuestState quests;
    NeedsState needs;
    std::map<Id, Inventory> inventories;
    WorldContext ctx;

    explicit World(std::uint64_t seed, DayNumber day, const std::string& canon_dir,
                   const CalendarConfig& cfg = {})
        : db(Db::load(canon_dir)), rng(seed), cal(cfg),
          ctx{db, rng, day, cal, facts, economy, population, faction,
              magic, justice, events, property, quests, needs, inventories} {}
};

RiteInputs prepared(std::initializer_list<std::pair<const std::string, std::int64_t>> items) {
    RiteInputs inputs;
    for (const auto& [id, count] : items) inputs.materials_held.emplace(id, count);
    inputs.performer_knows_rite = true;
    return inputs;
}

// A performer with everything except the one knob a test turns.
RiteInputs known_with_nothing() { return prepared({}); }

const char* kRitesHeader =
    "id,name,tradition,deity,materials,place,time_window,effect_family,effect_tag,tag,source_ref\n";

// Fixture canon dir (cleaned up on scope exit). Content = test scaffolding, not canon.
struct TempCanon {
    std::filesystem::path dir;

    explicit TempCanon(const std::string& name) : dir(std::filesystem::temp_directory_path() / ("sim_test_magic_" + name)) {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        std::ofstream out(dir / "rites.csv");
        out << kRitesHeader
            << "any_god_blessing,Fixture rite (test scaffolding),fixture,any,honey,,,"
               "blessing (fixture),INVENTED: EFFECT,CANON,tests/test_magic.cpp\n"
            << "festival_rite_fixture,Fixture rite (test scaffolding),fixture,inanna,honey,,"
               "its festival day,blessing (fixture),INVENTED: EFFECT,CANON,tests/test_magic.cpp\n"
            << "open_rite_fixture,Fixture rite (test scaffolding),fixture,inanna,honey,,,"
               "protection (fixture),INVENTED: EFFECT,OPEN,tests/test_magic.cpp\n";
    }

    ~TempCanon() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    std::string str() const { return dir.string(); }
};

}  // namespace

// ---------------------------------------------------------------------------
// favour: absent entries stand at the neutral birth value of 50 (Magic.hpp:37);
// add_favour clamps to 0..100 (Magic.hpp:59-60).
static bool test_favour_defaults_and_clamps() {
    MagicState state;
    SIM_CHECK_EQ(favour(state, "inanna"), 50);   // never touched -> neutral birth

    add_favour(state, "inanna", 2);
    SIM_CHECK_EQ(favour(state, "inanna"), 52);

    add_favour(state, "enlil", 200);
    SIM_CHECK_EQ(favour(state, "enlil"), 100);   // clamped high

    add_favour(state, "enki", -200);
    SIM_CHECK_EQ(favour(state, "enki"), 0);      // clamped low

    add_favour(state, "utu", 30);
    add_favour(state, "utu", -30);
    SIM_CHECK_EQ(favour(state, "utu"), 50);
    return true;
}

// Magic.hpp:21 — a rite missing from the db cannot be performed.
static bool test_unknown_rite_is_refused() {
    World w{1, 5, "../db/canon"};
    const auto inputs = known_with_nothing();

    const RiteResult r = perform_rite(w.ctx, w.magic, "no_such_rite", inputs);
    SIM_CHECK(!r.performed);
    SIM_CHECK(!r.succeeded);
    SIM_CHECK_EQ(r.refusal_reason, std::string("unknown_rite"));
    SIM_CHECK_EQ(r.rite_id, std::string("no_such_rite"));
    SIM_CHECK(r.deity.empty());
    SIM_CHECK(r.effect_family.empty());          // "empty when refused" (Magic.hpp:53)
    SIM_CHECK(r.score == 0.0);
    SIM_CHECK(w.magic.favour_by_deity.empty());  // no state change on refusal

    const RiteResult again = perform_rite(w.ctx, w.magic, "no_such_rite", inputs);
    SIM_CHECK_EQ(again.refusal_reason, std::string("unknown_rite"));  // and it stays deterministic
    return true;
}

// OPEN rows are canon that does not exist yet; they cannot ship (db/schema/rites.md).
static bool test_open_rite_row_is_refused() {
    TempCanon canon{"open"};
    World w{1, 5, canon.str()};
    w.magic.place = "temple:city_of_the_moon";

    const RiteResult r = perform_rite(w.ctx, w.magic, "open_rite_fixture", prepared({{"honey", 1}}));
    SIM_CHECK(!r.performed);
    SIM_CHECK_EQ(r.refusal_reason, std::string("unknown_rite"));
    SIM_CHECK(w.magic.favour_by_deity.empty());
    return true;
}

// rpg-systems §10.1 power 2: "you must know the rite" — a gate, not a score weight.
static bool test_rite_needs_knowledge() {
    World w{1, 5, "../db/canon"};
    RiteInputs inputs;  // knows nothing

    const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems", inputs);
    SIM_CHECK(!r.performed);
    SIM_CHECK(!r.succeeded);
    SIM_CHECK_EQ(r.refusal_reason, std::string("rite_not_known"));
    SIM_CHECK(r.effect_family.empty());
    SIM_CHECK(r.score == 0.0);
    SIM_CHECK(w.magic.favour_by_deity.empty());  // nothing angered by a non-attempt
    return true;
}

// The fixed formula (Magic.hpp) with every power satisfied and neutral
// favour: 50*4000/100 + 2000 + 2000 + 1000 + 1000 = 8000 bp; the roll is
// exactly Rng{seed}.fork(day).fork(stable_hash(rite)).below(10000) and
// success is roll < score_bp (D-022).
static bool test_full_formula_and_draw() {
    World w{7, 5, "../db/canon"};
    w.magic.place = "temple:city_of_the_moon";
    const auto inputs = prepared({{"fish", 2}, {"sea_gems", 1}});

    const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems", inputs);
    SIM_CHECK(r.performed);
    SIM_CHECK_EQ(r.score_bp, 8000);
    SIM_CHECK(close(r.score, 0.80));
    SIM_CHECK(r.refusal_reason.empty());
    SIM_CHECK_EQ(r.deity, std::string("the_two_waters"));
    SIM_CHECK_EQ(r.effect_family, std::string("offering (favour)"));

    // fork does not advance the parent (Rng.hpp), so a fresh Rng{7} replays it.
    const std::uint64_t roll =
        Rng{7}.fork(5).fork(stable_hash("sacrifice_fish_sea_gems")).below(10000);
    SIM_CHECK_EQ(roll, std::uint64_t{5807});
    SIM_CHECK_EQ(r.succeeded, roll < static_cast<std::uint64_t>(r.score_bp));
    SIM_CHECK(r.succeeded);
    return true;
}

// Magic.hpp:17-18: "A performed rite raises favour with its deity by +2."
// Score 8000 bp with roll 3493 -> success; favour 50 -> 52.
static bool test_performed_rite_raises_favour_by_two() {
    World w{1, 5, "../db/canon"};
    w.magic.place = "temple:city_of_the_moon";

    const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems",
                                      prepared({{"fish", 1}, {"sea_gems", 1}}));
    SIM_CHECK(r.performed);
    SIM_CHECK(r.succeeded);
    SIM_CHECK_EQ(favour(w.magic, "the_two_waters"), 52);
    return true;
}

// Magic.hpp:17-18: a performed rite raises +2 AND a failed rite angers -5
// (the header says "performed", and its RiteResult distinguishes performed
// from succeeded; the fixed formula provides only the one draw). Weakened to
// score 5000 bp (neutral favour, no materials, wrong place) with seed 14's
// day-5 roll 9589: failure nets 50 -> 47.
static bool test_failed_rite_angers_the_deity() {
    World w{14, 5, "../db/canon"};
    w.magic.place = "riverbank";  // rite wants "the first great temple (origin myth)"

    const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems",
                                      known_with_nothing());
    SIM_CHECK(r.performed);
    SIM_CHECK(!r.succeeded);
    SIM_CHECK(close(r.score, 0.50));
    SIM_CHECK_EQ(favour(w.magic, "the_two_waters"), 47);  // +2 performed, -5 angered

    // Clamping holds on the anger path too: a hated deity (0) cannot go below 0.
    World w2{14, 5, "../db/canon"};  // score 3000 bp, roll 9589
    w2.magic.place = "riverbank";
    add_favour(w2.magic, "the_two_waters", -50);  // 50 -> 0
    const RiteResult r2 = perform_rite(w2.ctx, w2.magic, "sacrifice_fish_sea_gems",
                                       known_with_nothing());
    SIM_CHECK(r2.performed);
    SIM_CHECK(!r2.succeeded);
    SIM_CHECK_EQ(favour(w2.magic, "the_two_waters"), 0);
    return true;
}

// Magic.hpp:9: materials weigh 0.20 only when ALL are held. Missing one lowers
// the score but the rite is still attempted (a performed rite, score 0.60).
static bool test_missing_materials_lower_score_but_still_perform() {
    World w{1, 5, "../db/canon"};
    w.magic.place = "temple:city_of_the_moon";

    const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems",
                                      prepared({{"fish", 3}}));
    SIM_CHECK(r.performed);
    SIM_CHECK(close(r.score, 0.60));
    return true;
}

// The canon materials column ("fish; sea gems") is normalized to Id keys
// (trimmed, lowercased, spaces -> '_'); keys must be held with count > 0.
static bool test_material_keys_are_normalized_and_nonzero() {
    World zero_count{1, 5, "../db/canon"};
    zero_count.magic.place = "temple:city_of_the_moon";
    const RiteResult r0 = perform_rite(zero_count.ctx, zero_count.magic, "sacrifice_fish_sea_gems",
                                       prepared({{"fish", 0}, {"sea_gems", 5}}));
    SIM_CHECK(close(r0.score, 0.60));  // a zero count is not holding the material

    World raw_prose{1, 5, "../db/canon"};
    raw_prose.magic.place = "temple:city_of_the_moon";
    const RiteResult r1 = perform_rite(raw_prose.ctx, raw_prose.magic, "sacrifice_fish_sea_gems",
                                       prepared({{"fish", 1}, {"sea gems", 5}}));
    SIM_CHECK(close(r1.score, 0.60));  // the canon entry "sea gems" keys as "sea_gems"
    return true;
}

// Magic.hpp:10-12 — place matching against the prose requirements of rites.csv.
static bool test_place_rules() {
    // "the first great temple (origin myth)": the temple KIND matches, a
    // riverbank does not, an unknown place does not.
    {
        World w{1, 5, "../db/canon"};
        w.magic.place = "temple:city_of_the_moon";
        const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems",
                                          prepared({{"fish", 1}, {"sea_gems", 1}}));
        SIM_CHECK(close(r.score, 0.80));
    }
    {
        World w{1, 5, "../db/canon"};
        w.magic.place = "riverbank";
        const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems",
                                          prepared({{"fish", 1}, {"sea_gems", 1}}));
        SIM_CHECK(close(r.score, 0.70));
    }
    {
        World w{1, 5, "../db/canon"};
        const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems",
                                          prepared({{"fish", 1}, {"sea_gems", 1}}));
        SIM_CHECK(close(r.score, 0.70));  // no known place -> the 0.10 is lost
    }
    // "temples and shrines" (hymns_deity_names): "shrine" matches "shrines".
    {
        World w{1, 5, "../db/canon"};
        w.magic.place = "shrine:old_ones";
        const RiteResult r = perform_rite(w.ctx, w.magic, "hymns_deity_names",
                                          prepared({{"vocal_performance", 1},
                                                    {"the_deity's_true_name", 1}}));
        SIM_CHECK(close(r.score, 0.80));
    }
    return true;
}

// Magic.hpp:12 — an EMPTY place requirement means anywhere.
static bool test_empty_place_requirement_means_anywhere() {
    TempCanon canon{"anywhere"};
    World w{1, 5, canon.str()};
    w.magic.place = "riverbank";  // nowhere near a temple; the fixture rite has no place demand

    const RiteResult r = perform_rite(w.ctx, w.magic, "any_god_blessing", prepared({{"honey", 1}}));
    SIM_CHECK(r.performed);
    SIM_CHECK(close(r.score, 0.80));
    return true;
}

// deity == "any" (3 of the 4 canon rites) names no god: RiteInputs has no field
// for which deity the performer addresses, and favour_by_deity is documented as
// "deities.csv id" (Magic.hpp:37) — so no favour entry appears at all.
static bool test_deity_any_applies_no_favour() {
    TempCanon canon{"any_deity"};
    World w{1, 5, canon.str()};  // roll 4729 < 8000 bp -> succeeds

    const RiteResult r = perform_rite(w.ctx, w.magic, "any_god_blessing", prepared({{"honey", 1}}));
    SIM_CHECK(r.performed);
    SIM_CHECK(r.succeeded);
    SIM_CHECK_EQ(r.deity, std::string("any"));
    SIM_CHECK(w.magic.favour_by_deity.empty());
    return true;
}

// Magic.hpp:13-14 — a festival is demanded only when the rite's time_window
// says so; the default calendar has no festival days (calendar.csv OPEN row).
static bool test_festival_time_window() {
    TempCanon canon{"festival"};
    const CalendarConfig no_festivals;
    World w{1, 12, canon.str(), no_festivals};
    w.magic.place = "riverbank";  // empty requirement -> anywhere

    const RiteResult off = perform_rite(w.ctx, w.magic, "festival_rite_fixture",
                                        prepared({{"honey", 1}}));
    SIM_CHECK(off.performed);
    SIM_CHECK(close(off.score, 0.70));  // time power missing on a non-festival day

    CalendarConfig with_festival;
    with_festival.festival_days = {12};
    World wf{1, 12, canon.str(), with_festival};
    wf.magic.place = "riverbank";

    const RiteResult on = perform_rite(wf.ctx, wf.magic, "festival_rite_fixture",
                                       prepared({{"honey", 1}}));
    SIM_CHECK(on.performed);
    SIM_CHECK(close(on.score, 0.80));  // the same day, now a festival day
    return true;
}

// Magic.hpp:10 — the rite's purity requirement defaults to 0 (this canon row's
// purity_required is blank), so even a purely impure performer keeps the 0.20.
static bool test_purity_requirement_defaults_to_zero() {
    World w{1, 5, "../db/canon"};
    w.magic.purity = 0;
    w.magic.place = "temple:city_of_the_moon";
    add_favour(w.magic, "the_two_waters", -50);  // favour 0: the 0.40 goes away

    const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems",
                                      known_with_nothing());
    SIM_CHECK(r.performed);
    SIM_CHECK(close(r.score, 0.40));  // 0.20 purity + 0.10 place + 0.10 time
    return true;
}

// Determinism (contract: "same seed -> same state bytes"): identical worlds
// produce identical results and identical state bytes, and the single draw
// forks WITHOUT advancing the world's rng (Rng.hpp:32-34).
static bool test_determinism_same_seed_same_state_bytes() {
    const auto inputs = prepared({{"fish", 1}, {"sea_gems", 1}});

    World a{11, 9, "../db/canon"};
    a.magic.place = "temple:city_of_the_moon";
    const RiteResult ra = perform_rite(a.ctx, a.magic, "sacrifice_fish_sea_gems", inputs);

    World b{11, 9, "../db/canon"};
    b.magic.place = "temple:city_of_the_moon";
    const RiteResult rb = perform_rite(b.ctx, b.magic, "sacrifice_fish_sea_gems", inputs);

    SIM_CHECK_EQ(ra.performed, rb.performed);
    SIM_CHECK_EQ(ra.succeeded, rb.succeeded);
    SIM_CHECK(ra.score == rb.score);  // bit-identical, not just close
    SIM_CHECK_EQ(ra.deity, rb.deity);
    SIM_CHECK_EQ(ra.effect_family, rb.effect_family);
    SIM_CHECK_EQ(fingerprint(a.magic), fingerprint(b.magic));

    // The draw came from a fork: the world rng stands exactly where it started.
    Rng probe{11};
    SIM_CHECK_EQ(a.rng.next(), probe.next());
    return true;
}

// Follow-up review 2026-09-25: add_favour widens before clamping, so an
// extreme delta (the C API passes the engine's int through) saturates instead
// of overflowing int. 50 + INT_MAX used to wrap negative and clamp to 0.
static bool test_add_favour_extreme_delta_saturates() {
    MagicState state;
    add_favour(state, "inanna", INT_MAX);
    SIM_CHECK_EQ(favour(state, "inanna"), 100);
    add_favour(state, "inanna", INT_MAX);  // already 100: stays 100
    SIM_CHECK_EQ(favour(state, "inanna"), 100);
    add_favour(state, "enki", INT_MIN);
    SIM_CHECK_EQ(favour(state, "enki"), 0);
    add_favour(state, "enki", INT_MIN);  // already 0: stays 0
    SIM_CHECK_EQ(favour(state, "enki"), 0);
    return true;
}

// Follow-up review 2026-09-25: the purity term reads rites.csv's
// purity_required column (Magic.hpp:10 "purity >= rite's purity requirement,
// default 0"); a blank cell is the default 0. Fixture rows are scaffolding.
static bool test_purity_required_column_gates_the_purity_term() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sim_test_magic_purity_required";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "rites.csv");
        out << "id,name,tradition,deity,materials,place,time_window,purity_required,"
               "effect_family,effect_tag,tag,source_ref\n"
            << "pure_rite_fixture,Fixture rite (test scaffolding),fixture,inanna,,,,60,"
               "blessing (fixture),INVENTED: EFFECT,CANON,tests/test_magic.cpp\n"
            << "blank_rite_fixture,Fixture rite (test scaffolding),fixture,inanna,,,,,"
               "blessing (fixture),INVENTED: EFFECT,CANON,tests/test_magic.cpp\n";
    }
    // No materials/place/time demands: 0.40*0.50 + 0.20 + [purity 0.20] + 0.10 + 0.10.
    const auto score_at = [&](const char* rite, int purity) {
        World w{1, 5, dir.string()};
        w.magic.purity = purity;
        return perform_rite(w.ctx, w.magic, rite, known_with_nothing()).score;
    };
    SIM_CHECK(close(score_at("pure_rite_fixture", 59), 0.60));   // below 60: the term is lost
    SIM_CHECK(close(score_at("pure_rite_fixture", 60), 0.80));   // at the requirement: kept
    SIM_CHECK(close(score_at("pure_rite_fixture", 100), 0.80));
    SIM_CHECK(close(score_at("blank_rite_fixture", 0), 0.80));   // blank = default 0
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return true;
}

// D-022: each rite gets its own roll, Rng.fork(day).fork(stable_hash(rite_id)),
// so two rites on the same day no longer succeed or fail together.
// Seed 1, day 5 (probed): d022_rite_a rolls 4717, d022_rite_b rolls 9031, and
// both fixture rites score 8000 bp — so a succeeds and b fails.
static bool test_d022_each_rite_gets_its_own_roll() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sim_test_magic_d022_rolls";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "rites.csv");
        out << kRitesHeader
            << "d022_rite_a,Fixture rite (test scaffolding),fixture,any,,,,"
               "blessing (fixture),INVENTED: EFFECT,CANON,tests/test_magic.cpp\n"
            << "d022_rite_b,Fixture rite (test scaffolding),fixture,any,,,,"
               "blessing (fixture),INVENTED: EFFECT,CANON,tests/test_magic.cpp\n";
    }
    World w{1, 5, dir.string()};
    const RiteResult a = perform_rite(w.ctx, w.magic, "d022_rite_a", known_with_nothing());
    const RiteResult b = perform_rite(w.ctx, w.magic, "d022_rite_b", known_with_nothing());
    SIM_CHECK(a.performed && b.performed);
    SIM_CHECK(a.score == b.score);  // same powers, same score...
    SIM_CHECK(a.succeeded);         // ...but a's roll 4717 < 8000
    SIM_CHECK(!b.succeeded);        // and b's roll 9031 >= 8000

    // The rolls are exactly the documented draw.
    for (const RiteResult* r : {&a, &b}) {
        const std::uint64_t roll =
            Rng{1}.fork(5).fork(stable_hash(r->rite_id)).below(10000);
        SIM_CHECK_EQ(r->succeeded, roll < 8000u);
    }
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return true;
}

// D-022: the score is integer basis points (0..10000), weights 4000/2000/2000/
// 1000/1000 and favour*4000/100 in integer maths, so it is exact on every CPU.
static bool test_d022_score_is_exact_basis_points() {
    World full{1, 5, "../db/canon"};
    full.magic.place = "temple:city_of_the_moon";
    const RiteResult r = perform_rite(full.ctx, full.magic, "sacrifice_fish_sea_gems",
                                      prepared({{"fish", 1}, {"sea_gems", 1}}));
    SIM_CHECK_EQ(r.score_bp, 8000);  // 50*40 + 2000 + 2000 + 1000 + 1000
    SIM_CHECK(r.score == 0.8);       // bit-exact: derived as score_bp / 10000.0

    World weak{1, 5, "../db/canon"};
    weak.magic.place = "riverbank";
    add_favour(weak.magic, "the_two_waters", -17);  // 33: 33*4000/100 = 1320 exactly
    const RiteResult w2 = perform_rite(weak.ctx, weak.magic, "sacrifice_fish_sea_gems",
                                       known_with_nothing());
    SIM_CHECK_EQ(w2.score_bp, 1320 + 2000 + 1000);  // favour + purity + time
    return true;
}

// D-022: Rng::below(n) is an unbiased draw in [0, n) and consumes the stream
// deterministically.
static bool test_d022_bounded_draw() {
    Rng a{99};
    Rng b{99};
    for (int i = 0; i < 1000; ++i) {
        const std::uint64_t x = a.below(10000);
        SIM_CHECK(x < 10000u);
        SIM_CHECK_EQ(x, b.below(10000));
    }
    Rng one{3};
    SIM_CHECK_EQ(one.below(1), std::uint64_t{0});
    SIM_CHECK_EQ(one.below(0), std::uint64_t{0});
    // FNV-1a 64 reference values (the empty string is the offset basis).
    SIM_CHECK_EQ(stable_hash(""), std::uint64_t{14695981039346656037ull});
    SIM_CHECK_EQ(stable_hash("a"), std::uint64_t{0xaf63dc4c8601ec8cull});
    return true;
}

// K-1: rite knowledge is MagicState (known_rites). learn_rite/knows_rite
// write/read only it; perform_rite's contract is unchanged — it still reads
// the gate from RiteInputs::performer_knows_rite, which the caller fills
// from knows_rite() (sim/Rites.hpp).
static bool test_rite_knowledge_state() {
    MagicState s;
    SIM_CHECK(!knows_rite(s, "sacrifice_fish_sea_gems"));
    SIM_CHECK(learn_rite(s, "sacrifice_fish_sea_gems"));
    SIM_CHECK(knows_rite(s, "sacrifice_fish_sea_gems"));
    SIM_CHECK(!learn_rite(s, "sacrifice_fish_sea_gems"));  // already known: no change
    SIM_CHECK(!learn_rite(s, ""));
    SIM_CHECK_EQ(s.known_rites.size(), std::size_t{1});
    SIM_CHECK(s.favour_by_deity.empty());  // learning touches nothing else

    World w{1, 5, "../db/canon"};
    w.magic.place = "temple:city_of_the_moon";
    learn_rite(w.magic, "sacrifice_fish_sea_gems");
    RiteInputs unflagged;  // the caller forgot to read knows_rite(): still refused
    unflagged.materials_held = {{"fish", 1}, {"sea_gems", 1}};
    const RiteResult r = perform_rite(w.ctx, w.magic, "sacrifice_fish_sea_gems", unflagged);
    SIM_CHECK(!r.performed);
    SIM_CHECK_EQ(r.refusal_reason, std::string("rite_not_known"));
    return true;
}

SIM_MAIN(test_favour_defaults_and_clamps,
         test_unknown_rite_is_refused,
         test_open_rite_row_is_refused,
         test_rite_needs_knowledge,
         test_full_formula_and_draw,
         test_performed_rite_raises_favour_by_two,
         test_failed_rite_angers_the_deity,
         test_missing_materials_lower_score_but_still_perform,
         test_material_keys_are_normalized_and_nonzero,
         test_place_rules,
         test_empty_place_requirement_means_anywhere,
         test_deity_any_applies_no_favour,
         test_festival_time_window,
         test_purity_requirement_defaults_to_zero,
         test_determinism_same_seed_same_state_bytes,
         test_rite_knowledge_state,
         test_add_favour_extreme_delta_saturates,
         test_purity_required_column_gates_the_purity_term,
         test_d022_each_rite_gets_its_own_roll,
         test_d022_score_is_exact_basis_points,
         test_d022_bounded_draw)
