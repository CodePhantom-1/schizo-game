// test_population.cpp — named NPCs, memory, and the rumour graph.
// Exercises the documented behavior of sim/Population.hpp: canon seeding
// (idempotent, OPEN-safe), day-ordered memory, and one-hop-per-tick rumour
// spread along `knows`.
#include "sim/Context.hpp"  // the seam: tick/seed take a WorldContext

#include "sim/Test.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using namespace sim;

namespace {

// A minimal world seam. Population reads ctx.db and ctx.day and nothing else,
// but the seam's fixed tick order requires the whole bundle to exist.
struct World {
    Db db{};
    Rng rng{1};
    Calendar cal{};
    WorldFacts facts{};
    EconomyState economy{};
    PopulationState population{};
    FactionState faction{};
    MagicState magic{};
    JusticeState justice{};
    EventsState events{};
    PropertyState property{};
    QuestState quests{};
    NeedsState needs{};
    std::map<Id, Inventory> inventories{};
    WorldContext ctx;

    explicit World(std::uint64_t seed = 1, DayNumber day = 1)
        : rng(seed),
          ctx{db,   rng,      day,      cal,      facts,   economy, population, faction,
              magic, justice, events, property, quests, needs, inventories} {}
};

// A canonical byte image of the state (npcs in vector order, knows in map
// order). The contract's determinism invariant is "same seed -> same state
// bytes", so tests compare these.
std::string state_bytes(const PopulationState& s) {
    std::string out;
    for (const Npc& n : s.npcs) {
        out += "npc " + n.id + " name=" + n.name + " city=" + n.home_city +
               " house=" + n.household + " faction=" + n.faction_id +
               " deity=" + n.patron_deity + " role=" + n.role +
               " loyalty=" + std::to_string(n.loyalty);
        for (const MemoryEntry& m : n.memory)
            out += " mem[" + std::to_string(m.day) + "](" + m.subject + ": " + m.fact + ")";
        out += "\n";
    }
    for (const auto& [id, edges] : s.knows) {
        out += "knows " + id + " ->";
        for (const Id& e : edges) out += " " + e;
        out += "\n";
    }
    return out;
}

const MemoryEntry* holds(const Npc& npc, const Id& subject, const std::string& fact) {
    for (const MemoryEntry& m : npc.memory)
        if (m.subject == subject && m.fact == fact) return &m;
    return nullptr;
}

// A knows-graph edge list over the three seeded named people.
void wire_triangle(PopulationState& state) {
    const char* names[] = {"law_giver", "the_prophet", "the_warchief"};
    for (const char* a : names)
        for (const char* b : names)
            if (std::string{a} != std::string{b}) state.knows[a].push_back(b);
}

const char* kTithe = "the tithe doubles";

// A throwaway street-residents fixture: people.csv rows with schedule-matching
// roles (the shape the coordinator says another agent is authoring against —
// id,name,role,city,faction,tag,source_ref) plus a matching schedules.csv, in
// the system temp dir, never committed as canon (test_justice.cpp's pattern).
// Wave 1 shipped 3 named leaders; the street-data wave adds light residents
// on top. Rather than hardcode a row count that content authoring keeps
// moving, every real-canon test computes its expectation from the canon
// itself, the same way seed_people()/canon_lint judge "shippable" (non-OPEN).
std::size_t shippable_people_count(const Db& db) {
    std::size_t n = 0;
    for (const Row& r : db.rows("people"))
        if (r.get("tag") != "OPEN" && !r.get("id").empty()) ++n;
    return n;
}

Db load_fixture_db() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "schizo_game_test_population_fixture";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "people.csv", std::ios::trunc);
        out << "id,name,role,city,faction,tag,source_ref\n"
            << "res_baker_1,Nanna the baker,baker,city_of_the_moon,,INVENTED,test fixture\n"
            << "res_baker_2,Ur-Nanshe the baker,baker,city_of_the_moon,,INVENTED,test fixture\n"
            << "res_watch_1,Lugal the watchman,watchman,city_of_the_moon,,INVENTED,test fixture\n"
            << "res_far_baker,Puabi the baker,baker,city_of_jewels,,INVENTED,test fixture\n"
            << "res_open,Unwritten Resident,baker,city_of_the_moon,,OPEN,test fixture\n"
            << "res_no_role_match,A carter,cart driver,city_of_the_moon,,INVENTED,test fixture\n"
            << "named_leader,The Named Leader,founder of somewhere;implied,city_of_the_moon,,"
               "CANON,test fixture\n";
    }
    {
        std::ofstream out(dir / "schedules.csv", std::ios::trunc);
        out << "id,role,hour,task,season,tag,source_ref\n"
            << "baker_row,baker,3,bakes bread,,INVENTED,test fixture\n"
            << "watch_row,watchman,21,walks the wall,,INVENTED,test fixture\n";
    }
    return Db::load(dir.string());
}

}  // namespace

static bool test_seed_reads_the_named_people() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);

    // Every non-OPEN people.csv row seeds — the named Wave 1 leaders plus
    // whatever the street-data wave has added since.
    SIM_CHECK(find_npc(w.population, "law_giver") != nullptr);
    SIM_CHECK(find_npc(w.population, "the_prophet") != nullptr);
    SIM_CHECK(find_npc(w.population, "the_warchief") != nullptr);
    SIM_CHECK_EQ(w.population.npcs.size(), shippable_people_count(w.db));

    for (const char* id : {"law_giver", "the_prophet", "the_warchief"}) {
        const Npc* npc = find_npc(w.population, id);
        const auto row = w.db.find("people", id);
        SIM_CHECK(npc != nullptr);
        SIM_CHECK(row.has_value());
        SIM_CHECK_EQ(npc->name, row->at("name"));
        SIM_CHECK_EQ(npc->home_city, row->at("city"));
        SIM_CHECK_EQ(npc->faction_id, row->at("faction"));
        SIM_CHECK(npc->memory.empty());
        SIM_CHECK_EQ(npc->loyalty, 50);
        // A named leader's narrative title never names a schedule role, so
        // the link-wiring pass never touches it — true at Wave 1 and stays
        // true however many street residents the canon gains.
        SIM_CHECK(w.population.knows.find(Id(id)) == w.population.knows.end());
    }
    return true;
}

static bool test_seed_is_idempotent() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);
    const std::string first = state_bytes(w.population);
    seed_people(w.ctx, w.population);
    SIM_CHECK_EQ(w.population.npcs.size(), shippable_people_count(w.db));
    SIM_CHECK_EQ(state_bytes(w.population), first);

    // Re-seeding must not clobber live state, either.
    Npc* giver = find_npc(w.population, "law_giver");
    SIM_CHECK(giver != nullptr);
    giver->loyalty = 7;
    witness(w.population, "law_giver", "the_prophet", kTithe, DayNumber{12});
    seed_people(w.ctx, w.population);
    SIM_CHECK_EQ(w.population.npcs.size(), shippable_people_count(w.db));
    SIM_CHECK_EQ(giver->loyalty, 7);
    SIM_CHECK_EQ(giver->memory.size(), std::size_t{1});
    return true;
}

static bool test_seed_without_canon_is_a_noop() {
    World empty;  // a Db with no tables at all
    seed_people(empty.ctx, empty.population);
    SIM_CHECK(empty.population.npcs.empty());
    SIM_CHECK(empty.population.knows.empty());

    World missing;
    missing.db = Db::load("/tmp/sim_population_test_no_such_dir");  // directory absent
    seed_people(missing.ctx, missing.population);
    SIM_CHECK(missing.population.npcs.empty());
    return true;
}

static bool test_seed_skips_open_rows_and_short_rows() {
    // OPEN-tagged canon rows mark content that does not exist yet: seeding
    // must neither resolve nor invent them. A row with a missing trailing
    // column must still seed, with empty fallbacks.
    const std::string dir = "/tmp/sim_population_test_canon";
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir + "/people.csv");
        out << "id,name,role,city,faction,tag,source_ref\n";
        out << "canon_person,Canon Person,a canon row,city_of_the_moon,the_empire,CANON,wb 1\n";
        out << "open_person,Unwritten Person,not yet canon,city_of_the_moon,the_empire,OPEN,notes 9\n";
        out << "short_person,Short Person,trailing column missing,city_of_jewels,the_empire,CANON\n";
    }
    World w;
    w.db = Db::load(dir);
    seed_people(w.ctx, w.population);

    SIM_CHECK_EQ(w.population.npcs.size(), std::size_t{2});
    SIM_CHECK(find_npc(w.population, "canon_person") != nullptr);
    SIM_CHECK(find_npc(w.population, "open_person") == nullptr);  // never seeded
    const Npc* short_one = find_npc(w.population, "short_person");
    SIM_CHECK(short_one != nullptr);
    SIM_CHECK_EQ(short_one->name, std::string("Short Person"));
    SIM_CHECK_EQ(short_one->home_city, std::string("city_of_jewels"));
    SIM_CHECK_EQ(short_one->faction_id, std::string("the_empire"));

    std::filesystem::remove_all(dir);
    return true;
}

static bool test_witness_records_memory_ordered_by_day() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);

    witness(w.population, "law_giver", "the_prophet", kTithe, DayNumber{7});
    witness(w.population, "law_giver", "the_warchief", "raided the eastern villages", DayNumber{3});
    witness(w.population, "law_giver", "the_empire", "raised the temple tax", DayNumber{5});

    const Npc* giver = find_npc(w.population, "law_giver");
    SIM_CHECK(giver != nullptr);
    SIM_CHECK_EQ(giver->memory.size(), std::size_t{3});
    // Ordered by day, whatever the witnessing order was.
    SIM_CHECK_EQ(giver->memory[0].day, DayNumber{3});
    SIM_CHECK_EQ(giver->memory[1].day, DayNumber{5});
    SIM_CHECK_EQ(giver->memory[2].day, DayNumber{7});
    SIM_CHECK_EQ(giver->memory[2].subject, std::string("the_prophet"));
    SIM_CHECK_EQ(giver->memory[2].fact, std::string(kTithe));

    // An unknown witness is a graceful no-op: nothing crashes, nothing changes.
    witness(w.population, "no_such_npc", "the_prophet", "a ghost fact", DayNumber{8});
    SIM_CHECK_EQ(w.population.npcs.size(), shippable_people_count(w.db));
    for (const Npc& n : w.population.npcs)
        if (n.id != "law_giver") SIM_CHECK(n.memory.empty());
    return true;
}

static bool test_find_npc_finds_and_misses() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);

    Npc* mutable_npc = find_npc(w.population, "the_prophet");
    SIM_CHECK(mutable_npc != nullptr);
    SIM_CHECK_EQ(mutable_npc->id, std::string("the_prophet"));
    mutable_npc->loyalty = 33;  // the mutable overload really is mutable

    const PopulationState& frozen = w.population;
    const Npc* const_npc = find_npc(frozen, "the_prophet");
    SIM_CHECK(const_npc != nullptr);
    SIM_CHECK_EQ(const_npc->loyalty, 33);

    SIM_CHECK(find_npc(frozen, "no_such_npc") == nullptr);
    SIM_CHECK(find_npc(w.population, "no_such_npc") == nullptr);
    return true;
}

static bool test_rumour_moves_one_hop_per_tick() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);
    w.population.knows["law_giver"] = {"the_prophet"};
    w.population.knows["the_prophet"] = {"the_warchief"};

    witness(w.population, "law_giver", "the_empire", kTithe, DayNumber{1});

    // A fact heard today is not retold the same day.
    Npc* prophet = find_npc(w.population, "the_prophet");
    Npc* chief = find_npc(w.population, "the_warchief");
    SIM_CHECK(prophet != nullptr);
    SIM_CHECK(chief != nullptr);

    w.ctx.day = 1;
    tick_population(w.ctx, w.population);  // default: one day
    SIM_CHECK(prophet->memory.empty());

    // Day 2: the teller retells; the fact hops once — to the prophet, not on.
    w.ctx.day = 2;
    tick_population(w.ctx, w.population);
    const MemoryEntry* at_prophet = holds(*prophet, "the_empire", kTithe);
    SIM_CHECK(at_prophet != nullptr);
    SIM_CHECK_EQ(at_prophet->day, DayNumber{2});
    SIM_CHECK(chief->memory.empty());  // one hop per tick

    // Day 3: the prophet retells what he heard yesterday; the warchief learns it.
    w.ctx.day = 3;
    tick_population(w.ctx, w.population);
    const MemoryEntry* at_chief = holds(*chief, "the_empire", kTithe);
    SIM_CHECK(at_chief != nullptr);
    SIM_CHECK_EQ(at_chief->day, DayNumber{3});
    return true;
}

static bool test_rumour_does_not_duplicate_or_bounce() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);
    wire_triangle(w.population);  // everyone knows everyone: it must settle, not echo

    witness(w.population, "law_giver", "the_empire", kTithe, DayNumber{1});
    for (DayNumber day = 2; day <= 6; ++day) {
        w.ctx.day = day;
        tick_population(w.ctx, w.population);
    }
    // Everyone in the wired triangle holds the fact exactly once. Other
    // npcs (street residents, once the canon has them, with their own
    // separate knows-components from wire_resident_links) are untouched —
    // this scenario only wires the three named leaders together.
    std::size_t total = 0;
    for (const char* id : {"law_giver", "the_prophet", "the_warchief"}) {
        const Npc* n = find_npc(w.population, id);
        SIM_CHECK(n != nullptr);
        SIM_CHECK_EQ(n->memory.size(), std::size_t{1});
        SIM_CHECK(holds(*n, "the_empire", kTithe) != nullptr);
        total += n->memory.size();
    }
    SIM_CHECK_EQ(total, std::size_t{3});

    // Once the component is saturated, further ticks change no bytes.
    const std::string settled = state_bytes(w.population);
    for (DayNumber day = 7; day <= 9; ++day) {
        w.ctx.day = day;
        tick_population(w.ctx, w.population);
    }
    SIM_CHECK_EQ(state_bytes(w.population), settled);
    return true;
}

static bool test_facts_stay_put_without_knows_edges() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);  // knows stays empty: no canon social links yet

    witness(w.population, "law_giver", "the_empire", kTithe, DayNumber{1});
    for (DayNumber day = 2; day <= 4; ++day) {
        w.ctx.day = day;
        tick_population(w.ctx, w.population);
    }
    const Npc* giver = find_npc(w.population, "law_giver");
    SIM_CHECK(giver != nullptr);
    SIM_CHECK_EQ(giver->memory.size(), std::size_t{1});
    for (const Npc& n : w.population.npcs)
        if (n.id != "law_giver") SIM_CHECK(n.memory.empty());
    return true;
}

static bool test_multi_day_tick_matches_daily_ticks() {
    World stepwise;  // three single-day ticks
    World batch;     // one days = 3 tick
    for (World* w : {&stepwise, &batch}) {
        w->db = Db::load("../db/canon");
        seed_people(w->ctx, w->population);
        w->population.knows["law_giver"] = {"the_prophet"};
        w->population.knows["the_prophet"] = {"the_warchief"};
        witness(w->population, "law_giver", "the_empire", kTithe, DayNumber{1});
    }
    for (DayNumber day = 2; day <= 4; ++day) {
        stepwise.ctx.day = day;
        tick_population(stepwise.ctx, stepwise.population);
    }
    batch.ctx.day = 2;
    tick_population(batch.ctx, batch.population, 3);
    SIM_CHECK_EQ(state_bytes(stepwise.population), state_bytes(batch.population));

    // The cascade really crossed two hops inside the one call.
    const Npc* chief = find_npc(batch.population, "the_warchief");
    SIM_CHECK(chief != nullptr);
    SIM_CHECK(holds(*chief, "the_empire", kTithe) != nullptr);
    return true;
}

static bool test_dangling_knows_edges_are_graceful() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);
    w.population.knows["law_giver"] = {"the_prophet", "ghost_listener"};  // listener not seeded
    w.population.knows["ghost_teller"] = {"law_giver"};                   // teller not seeded

    witness(w.population, "law_giver", "the_empire", kTithe, DayNumber{1});
    w.ctx.day = 2;
    tick_population(w.ctx, w.population);

    const Npc* prophet = find_npc(w.population, "the_prophet");
    SIM_CHECK(prophet != nullptr);
    SIM_CHECK(holds(*prophet, "the_empire", kTithe) != nullptr);
    // No NPC is invented for the ghosts, and nothing crashes.
    SIM_CHECK_EQ(w.population.npcs.size(), shippable_people_count(w.db));
    return true;
}

static bool test_same_operations_same_state_bytes_any_seed() {
    // The contract's determinism invariant. The spread draws no randomness at
    // all, so the state bytes must not depend on the world seed either.
    auto run = [](std::uint64_t seed) {
        World w(seed, 1);
        w.db = Db::load("../db/canon");
        seed_people(w.ctx, w.population);
        w.population.knows["law_giver"] = {"the_prophet", "the_warchief"};
        w.population.knows["the_prophet"] = {"the_warchief"};
        witness(w.population, "law_giver", "the_empire", kTithe, DayNumber{1});
        witness(w.population, "the_prophet", "the_empire", kTithe, DayNumber{2});
        for (DayNumber day = 2; day <= 5; ++day) {
            w.ctx.day = day;
            tick_population(w.ctx, w.population);
        }
        return state_bytes(w.population);
    };
    SIM_CHECK_EQ(run(1), run(1));                 // same seed -> same state bytes
    SIM_CHECK_EQ(run(1), run(0xDEADBEEFull));     // ... and the seed does not matter at all
    return true;
}

// --- light residents: role seeding, knows-wiring, npc_task_at (D-011) ------

static bool test_seed_sets_role_only_for_schedule_matching_rows() {
    World w;
    w.db = load_fixture_db();
    seed_people(w.ctx, w.population);

    SIM_CHECK_EQ(find_npc(w.population, "res_baker_1")->role, std::string("baker"));
    SIM_CHECK_EQ(find_npc(w.population, "res_watch_1")->role, std::string("watchman"));
    // "cart driver" names no schedules.csv role in this fixture: stays empty.
    SIM_CHECK(find_npc(w.population, "res_no_role_match")->role.empty());
    // A named leader's narrative title never matches a schedule role either.
    SIM_CHECK(find_npc(w.population, "named_leader")->role.empty());
    // OPEN rows never seed at all.
    SIM_CHECK(find_npc(w.population, "res_open") == nullptr);
    return true;
}

static bool test_seed_wires_same_role_same_city_and_neighbour_links() {
    World w;
    w.db = load_fixture_db();
    seed_people(w.ctx, w.population);

    // Same role, same city: the two City-of-the-Moon bakers know each other.
    const auto& b1 = w.population.knows["res_baker_1"];
    SIM_CHECK(std::find(b1.begin(), b1.end(), Id("res_baker_2")) != b1.end());

    // A baker in a different city is never linked by the same-role rule.
    const auto& far = w.population.knows["res_far_baker"];
    SIM_CHECK(std::find(far.begin(), far.end(), Id("res_baker_1")) == far.end());

    // Neighbours: role-bearing residents (the ones the street link-wiring
    // pass operates on) of the same city are chained by sorted id —
    // res_baker_1 and res_watch_1 are consecutive in that order despite
    // differing roles, since the chain sorts by id, not role.
    std::vector<Id> moon_residents = {"res_baker_1", "res_baker_2", "res_watch_1"};
    std::sort(moon_residents.begin(), moon_residents.end());
    for (std::size_t i = 1; i < moon_residents.size(); ++i) {
        const auto& edges = w.population.knows[moon_residents[i - 1]];
        SIM_CHECK(std::find(edges.begin(), edges.end(), moon_residents[i]) != edges.end());
    }

    // A narrative leader and a resident whose role names no schedule get no
    // links at all: the link-wiring pass only ever touches role-bearing
    // (schedule-matching) residents.
    SIM_CHECK(w.population.knows.find("named_leader") == w.population.knows.end());
    SIM_CHECK(w.population.knows.find("res_no_role_match") == w.population.knows.end());

    // No self-loop, no dangling edge to res_open (it was never seeded).
    for (const auto& [id, edges] : w.population.knows) {
        SIM_CHECK(std::find(edges.begin(), edges.end(), id) == edges.end());
        SIM_CHECK(std::find(edges.begin(), edges.end(), Id("res_open")) == edges.end());
    }
    return true;
}

static bool test_seed_link_wiring_is_idempotent() {
    World w;
    w.db = load_fixture_db();
    seed_people(w.ctx, w.population);
    const std::string first = state_bytes(w.population);
    seed_people(w.ctx, w.population);  // re-seed: no duplicate edges, no changed bytes
    SIM_CHECK_EQ(state_bytes(w.population), first);
    return true;
}

static bool test_named_leaders_never_get_knows_edges_from_real_canon() {
    // Guards the "keep named NPC behaviour intact" requirement against the
    // real canon: whatever street residents the canon has gained, the three
    // Wave 1 leaders' narrative titles never name a schedule role, so the
    // link-wiring pass must never touch them.
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);
    for (const char* id : {"law_giver", "the_prophet", "the_warchief"})
        SIM_CHECK(w.population.knows.find(Id(id)) == w.population.knows.end());
    return true;
}

static bool test_npc_task_at_uses_the_npcs_own_role() {
    World w;
    w.db = load_fixture_db();
    seed_people(w.ctx, w.population);
    const Calendar cal{};

    const auto t = npc_task_at(w.db, cal, w.population, "res_baker_1", DayNumber{1}, 10);
    SIM_CHECK(t.has_value());
    SIM_CHECK_EQ(t->schedule_id, Id("baker_row"));

    // No role matched: nullopt, not a crash.
    SIM_CHECK(!npc_task_at(w.db, cal, w.population, "res_no_role_match", DayNumber{1}, 10)
                   .has_value());
    // Unknown npc: nullopt.
    SIM_CHECK(!npc_task_at(w.db, cal, w.population, "no_such_npc", DayNumber{1}, 10).has_value());
    return true;
}

static bool test_npc_task_at_against_real_canon_is_deterministic() {
    World w;
    w.db = Db::load("../db/canon");
    seed_people(w.ctx, w.population);
    const Calendar cal{};
    // No canon person currently carries a schedule-matching role, so every
    // named leader resolves to nullopt today — proven stable across calls.
    for (const char* id : {"law_giver", "the_prophet", "the_warchief"}) {
        const auto a = npc_task_at(w.db, cal, w.population, id, DayNumber{50}, 12);
        const auto b = npc_task_at(w.db, cal, w.population, id, DayNumber{50}, 12);
        SIM_CHECK_EQ(a.has_value(), b.has_value());
    }
    return true;
}

SIM_MAIN(test_seed_reads_the_named_people,
         test_seed_is_idempotent,
         test_seed_without_canon_is_a_noop,
         test_seed_skips_open_rows_and_short_rows,
         test_witness_records_memory_ordered_by_day,
         test_find_npc_finds_and_misses,
         test_rumour_moves_one_hop_per_tick,
         test_rumour_does_not_duplicate_or_bounce,
         test_facts_stay_put_without_knows_edges,
         test_multi_day_tick_matches_daily_ticks,
         test_dangling_knows_edges_are_graceful,
         test_same_operations_same_state_bytes_any_seed,
         test_seed_sets_role_only_for_schedule_matching_rows,
         test_seed_wires_same_role_same_city_and_neighbour_links,
         test_seed_link_wiring_is_idempotent,
         test_named_leaders_never_get_knows_edges_from_real_canon,
         test_npc_task_at_uses_the_npcs_own_role,
         test_npc_task_at_against_real_canon_is_deterministic)
