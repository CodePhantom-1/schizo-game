// test_contracts.cpp — the seam compiles: every module header instantiates and
// a WorldContext can be built from real states. (No tick calls here: Wave 1
// agents implement the module bodies; Wave 2 wires the tick.)
#include "sim/Context.hpp"

#include "sim/Test.hpp"

using namespace sim;

static bool test_states_instantiate() {
    EconomyState economy;
    PopulationState population;
    FactionState faction;
    MagicState magic;
    JusticeState justice;
    EventsState events;
    PropertyState property;
    QuestState quests;

    Db db{};
    Rng rng{1};
    Calendar cal{};
    WorldFacts facts{};
    WorldContext ctx{db,  rng,      1,  cal,  facts, economy, population, faction,
                     magic, justice, events, property, quests};
    SIM_CHECK_EQ(ctx.day, DayNumber{1});
    SIM_CHECK_EQ(ctx.facts.drought_stage, 0);
    return true;
}

static bool test_defaults_are_sane() {
    FactionState faction;
    SIM_CHECK(!faction.oath_breaker_curse);
    MagicState magic;
    SIM_CHECK_EQ(magic.purity, 100);
    JusticeState justice;
    SIM_CHECK(justice.open_crimes.empty());
    return true;
}

SIM_MAIN(test_states_instantiate, test_defaults_are_sane)
