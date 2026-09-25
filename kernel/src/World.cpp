// World.cpp — Wave 2 integration: the deterministic daily tick (coordinator).
// Tick order is fixed in Context.hpp; every module reads the other states as of
// the same morning and writes only its own.
#include "sim/World.hpp"

#include "sim/Actions.hpp"

#include <cstdlib>

namespace sim {

void WorldState::init(const std::string& canon_dir, std::uint64_t world_seed) {
    db = Db::load(canon_dir);
    seed = world_seed;
    rng = Rng(world_seed);
    // The calendar's shape is ratified machinery; its seasons are canon data
    // (seasons.csv, D-015). Months stay unnamed and festival days stay OPEN.
    CalendarConfig cfg;
    for (const Row& r : db.rows("seasons")) {
        if (r.get("tag") == "OPEN") continue;
        SeasonDef s;
        s.id = r.at("id");
        s.start_day_of_year = std::strtol(r.get("start_day_of_year", "1").c_str(), nullptr, 10);
        if (s.start_day_of_year >= 1) cfg.seasons.push_back(s);
    }
    cal = Calendar{cfg};
    facts = WorldFacts{};
    day = 1;

    economy = EconomyState{};
    population = PopulationState{};
    faction = FactionState{};
    magic = MagicState{};
    justice = JusticeState{};
    events = EventsState{};
    property = PropertyState{};
    quests = QuestState{};

    needs = NeedsState{};
    inventories.clear();
    inventories["player"] = Inventory{};  // the prisoner start (D-009): empty-handed
    rite_effects = RiteEffectsState{};    // K-1: no ward laid, no omen read

    WorldContext ctx = context();
    for (const Row& city : db.rows("cities"))
        if (city.get("alias_of").empty())  // an alias is the same city: one market
            seed_market(ctx, economy, city.at("id"));
    seed_people(ctx, population);
    load_rules(ctx, events);  // events.csv is canon-empty at Wave 1: zero rules
    load_defs(ctx, quests);   // quests.csv likewise
}

void WorldState::advance_days(int days) {
    for (int i = 0; i < days; ++i) {
        const WorldContext ctx = context();
        tick_economy(ctx, economy, 1);
        tick_population(ctx, population, 1);
        tick_faction(ctx, faction, 1);
        // Magic has no tick: rites are performed by the engine layer through
        // perform_rite; favour only moves when someone performs.
        tick_justice(ctx, justice, 1);
        tick_events(ctx, events, 1);
        tick_property(ctx, property, 1);
        tick_quests(ctx, quests, 1);
        // World-orchestrated hearings (commit_crime) need several modules'
        // state, so they run here, after the module ticks, on the same day.
        hold_due_hearings(*this);
        ++day;
    }
}

WorldContext WorldState::context() {
    return WorldContext{db,  rng,      day,      cal,      facts,
                        economy, population, faction, magic,
                        justice, events,     property, quests,
                        needs, inventories};
}

}  // namespace sim
