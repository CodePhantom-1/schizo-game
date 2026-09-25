// test_needs.cpp — T3 (hunger, thirst, fatigue): decay, sleep, eat/drink,
// refusal, effect thresholds, clamping, determinism.
#include "sim/Needs.hpp"

#include "sim/Db.hpp"
#include "sim/Test.hpp"

using namespace sim;

static Db load_canon() { return Db::load("../db/canon"); }

static bool test_decay_over_hours() {
    NeedsState s;
    advance_needs(s, "player", 5, /*sleeping=*/false);
    const Needs& n = needs_of(s, "player");
    SIM_CHECK_EQ(n.hunger, 10);   // 5h * 2/h
    SIM_CHECK_EQ(n.thirst, 15);   // 5h * 3/h
    SIM_CHECK_EQ(n.fatigue, 20);  // 5h * 4/h
    return true;
}

static bool test_sleeping_restores_fatigue() {
    NeedsState s;
    advance_needs(s, "player", 10, /*sleeping=*/false);  // fatigue 40
    advance_needs(s, "player", 3, /*sleeping=*/true);     // -36, clamped at 0
    SIM_CHECK_EQ(needs_of(s, "player").fatigue, 4);
    // hunger/thirst keep climbing even while asleep
    SIM_CHECK(needs_of(s, "player").hunger > 20);
    return true;
}

static bool test_eat_reduces_hunger() {
    Db db = load_canon();
    NeedsState s;
    advance_needs(s, "player", 20, false);  // hunger 40
    const int before = needs_of(s, "player").hunger;
    std::string reason;
    SIM_CHECK(eat(db, s, "player", "dates", &reason));  // staple food, -20
    SIM_CHECK_EQ(needs_of(s, "player").hunger, before - 20);
    return true;
}

static bool test_bread_is_food() {
    Db db = load_canon();
    NeedsState s;
    advance_needs(s, "player", 20, false);
    const int before = needs_of(s, "player").hunger;
    SIM_CHECK(eat(db, s, "player", "bread"));  // T4's crafted bread, staple baked food, -30
    SIM_CHECK_EQ(needs_of(s, "player").hunger, before - 30);
    return true;
}

static bool test_drink_reduces_thirst() {
    Db db = load_canon();
    NeedsState s;
    advance_needs(s, "player", 20, false);  // thirst 60
    std::string reason;
    SIM_CHECK(drink(db, s, "player", "water", &reason));  // virtual, always drinkable, -40
    SIM_CHECK_EQ(needs_of(s, "player").thirst, 20);
    SIM_CHECK(drink(db, s, "player", "beer", &reason));  // staple drink, -20 thirst, -5 hunger
    SIM_CHECK_EQ(needs_of(s, "player").thirst, 0);
    return true;
}

static bool test_refuses_non_food_non_drink() {
    Db db = load_canon();
    NeedsState s;
    std::string reason;
    SIM_CHECK(!eat(db, s, "player", "gold", &reason));  // precious metal, not food
    SIM_CHECK_EQ(reason, std::string("not_food"));
    reason.clear();
    SIM_CHECK(!drink(db, s, "player", "gold", &reason));
    SIM_CHECK_EQ(reason, std::string("not_drink"));
    reason.clear();
    SIM_CHECK(!eat(db, s, "player", "no_such_item", &reason));
    SIM_CHECK_EQ(reason, std::string("unknown_item"));
    return true;
}

static bool test_effect_thresholds() {
    Needs n;
    SIM_CHECK(need_effects(n).empty());
    n.hunger = 40;
    SIM_CHECK_EQ(need_effects(n), std::vector<std::string>{"hungry"});
    n.hunger = 75;
    SIM_CHECK_EQ(need_effects(n), (std::vector<std::string>{"hungry", "starving"}));
    n = Needs{};
    n.thirst = 40;
    SIM_CHECK_EQ(need_effects(n), std::vector<std::string>{"parched"});
    n = Needs{};
    n.fatigue = 70;
    SIM_CHECK_EQ(need_effects(n), std::vector<std::string>{"exhausted"});
    // all four at once, sorted alphabetically
    n.hunger = 75;
    n.thirst = 40;
    SIM_CHECK_EQ(need_effects(n),
                 (std::vector<std::string>{"exhausted", "hungry", "parched", "starving"}));
    return true;
}

static bool test_clamping_0_100() {
    NeedsState s;
    advance_needs(s, "player", 1000, false);
    SIM_CHECK_EQ(needs_of(s, "player").hunger, 100);
    SIM_CHECK_EQ(needs_of(s, "player").thirst, 100);
    SIM_CHECK_EQ(needs_of(s, "player").fatigue, 100);
    advance_needs(s, "player", 1000, true);
    SIM_CHECK_EQ(needs_of(s, "player").fatigue, 0);
    return true;
}

static bool test_determinism() {
    NeedsState a, b;
    for (int i = 0; i < 30; ++i) {
        advance_needs(a, "player", 3, i % 4 == 0);
        advance_needs(b, "player", 3, i % 4 == 0);
    }
    SIM_CHECK_EQ(needs_of(a, "player").hunger, needs_of(b, "player").hunger);
    SIM_CHECK_EQ(needs_of(a, "player").thirst, needs_of(b, "player").thirst);
    SIM_CHECK_EQ(needs_of(a, "player").fatigue, needs_of(b, "player").fatigue);
    return true;
}

static bool test_per_actor_independent() {
    NeedsState s;
    advance_needs(s, "player", 5, false);
    SIM_CHECK_EQ(needs_of(s, "some_npc").hunger, 0);
    return true;
}

SIM_MAIN(test_decay_over_hours, test_bread_is_food, test_sleeping_restores_fatigue, test_eat_reduces_hunger,
          test_drink_reduces_thirst, test_refuses_non_food_non_drink, test_effect_thresholds,
          test_clamping_0_100, test_determinism, test_per_actor_independent)
