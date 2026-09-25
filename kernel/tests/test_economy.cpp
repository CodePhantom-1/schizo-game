// test_economy.cpp — the price engine: silver by weight, grain as the base good.
// Tests the contract in include/sim/Economy.hpp: seeding from canon cities,
// the grain-anchored fallback, supply shocks, and the tick's responses to
// supply, demand, season and the drought — plus the invariants (integral
// silver, never negative, never zero for food staples) and byte determinism.
#include "sim/Context.hpp"

#include "sim/Test.hpp"

#include <cstdint>
#include <string>

using namespace sim;

namespace {

const char* kCity = "city_of_jewels";  // a CANON row of db/canon/cities.csv

// A small deterministic world to tick against, mirroring the coordinator's
// construction (test_contracts.cpp). ctx holds const refs to the members.
struct Rig {
    Db db;
    Rng rng{7};
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
    NeedsState needs;
    std::map<Id, Inventory> inventories;
    WorldContext ctx;

    explicit Rig(DayNumber day = 1)
        : db(Db::load("../db/canon")),
          ctx{db,          rng,     day,      cal,      facts,    economy, population,
              faction,     magic,   justice,  events,   property, quests, needs, inventories} {}
};

// A deterministic serialization of the state: same bytes => same state.
std::string fingerprint(const EconomyState& s) {
    std::string out;
    for (const auto& [city, book] : s.market_by_city) {
        out += city;
        out += '|';
        for (const auto& [item, price] : book.silver_by_item) {
            out += item;
            out += '=';
            out += std::to_string(price);
            out += ';';
        }
        out += '#';
    }
    for (const auto& [key, units] : s.stock_by_city_item) {
        out += key;
        out += '=';
        out += std::to_string(units);
        out += ';';
    }
    return out;
}

std::int64_t stock(const EconomyState& s, const Id& city, const Id& item) {
    const auto it = s.stock_by_city_item.find(city + "/" + item);
    return it == s.stock_by_city_item.end() ? -1 : it->second;
}

// --- seeding ---------------------------------------------------------------

static bool test_seed_market_knows_the_city() {
    Rig rig;
    SIM_CHECK(rig.db.has("cities", kCity));  // the id we seed is canon
    seed_market(rig.ctx, rig.economy, kCity);

    const auto cit = rig.economy.market_by_city.find(kCity);
    SIM_CHECK(cit != rig.economy.market_by_city.end());
    const auto grain = cit->second.silver_by_item.find("grain");
    SIM_CHECK(grain != cit->second.silver_by_item.end());
    SIM_CHECK_EQ(grain->second, Silver{2});  // the grain base price
    SIM_CHECK_EQ(stock(rig.economy, kCity, "grain"), std::int64_t{100});

    // items.csv is empty for now, so every book entry is grain or a canon item.
    for (const auto& [city, book] : rig.economy.market_by_city)
        for (const auto& [item, price] : book.silver_by_item) {
            SIM_CHECK(item == "grain" || rig.db.has("items", item));
            SIM_CHECK(price >= 1);  // never zero, never negative
        }
    return true;
}

static bool test_seed_market_is_idempotent_and_covers_canon_cities() {
    Rig rig;
    seed_market(rig.ctx, rig.economy, kCity);
    const std::string once = fingerprint(rig.economy);
    seed_market(rig.ctx, rig.economy, kCity);
    SIM_CHECK_EQ(fingerprint(rig.economy), once);

    // Every canon city can be seeded; two fresh runs seed byte-identically.
    Rig a;
    Rig b;
    for (const Row& row : a.db.rows("cities")) {
        seed_market(a.ctx, a.economy, row.at("id"));
        seed_market(b.ctx, b.economy, row.at("id"));
    }
    SIM_CHECK_EQ(a.economy.market_by_city.size(), a.db.rows("cities").size());
    SIM_CHECK_EQ(fingerprint(a.economy), fingerprint(b.economy));
    return true;
}

// --- price_of: the grain-anchored fallback ----------------------------------

static bool test_price_of_fallback_anchors_to_grain() {
    Rig rig;
    seed_market(rig.ctx, rig.economy, kCity);
    // An unknown item in a known city: grain base x band, band 1 while items
    // canon is empty — exactly the city's current grain price.
    SIM_CHECK_EQ(price_of(rig.economy, kCity, "no_such_item"),
                 price_of(rig.economy, kCity, "grain"));
    SIM_CHECK_EQ(price_of(rig.economy, kCity, "no_such_item"), Silver{2});

    // The fallback tracks the anchor: a drought moves unknown-item prices too.
    rig.facts.drought_stage = 2;
    tick_economy(rig.ctx, rig.economy, 1);
    SIM_CHECK_EQ(price_of(rig.economy, kCity, "no_such_item"),
                 price_of(rig.economy, kCity, "grain"));
    SIM_CHECK(price_of(rig.economy, kCity, "no_such_item") > 2);

    // Unknown city: the bare grain base. Still integral and positive.
    SIM_CHECK_EQ(price_of(rig.economy, "nowhere", "grain"), Silver{2});
    SIM_CHECK_EQ(price_of(rig.economy, "nowhere", "no_such_item"), Silver{2});
    SIM_CHECK(price_of(rig.economy, "nowhere", "no_such_item") >= 1);
    return true;
}

// --- supply shocks ----------------------------------------------------------

static bool test_prices_respond_to_supply() {
    // Baseline: seeded, one market day (day 1 is no harvest, drought 0).
    Rig base;
    seed_market(base.ctx, base.economy, kCity);
    tick_economy(base.ctx, base.economy, 1);
    SIM_CHECK_EQ(price_of(base.economy, kCity, "grain"), Silver{2});
    SIM_CHECK_EQ(stock(base.economy, kCity, "grain"), std::int64_t{99});

    // Goods arriving (ship, caravan): full shelves are cheap.
    Rig cheap;
    seed_market(cheap.ctx, cheap.economy, kCity);
    deliver(cheap.economy, kCity, "grain", 300);
    SIM_CHECK_EQ(stock(cheap.economy, kCity, "grain"), std::int64_t{400});
    tick_economy(cheap.ctx, cheap.economy, 1);
    SIM_CHECK_EQ(price_of(cheap.economy, kCity, "grain"), Silver{1});

    // Goods leaving (theft, raid): empty shelves are dear.
    Rig dear;
    seed_market(dear.ctx, dear.economy, kCity);
    consume(dear.economy, kCity, "grain", 90);
    tick_economy(dear.ctx, dear.economy, 1);
    SIM_CHECK_EQ(price_of(dear.economy, kCity, "grain"), Silver{3});

    SIM_CHECK(price_of(cheap.economy, kCity, "grain") <
              price_of(base.economy, kCity, "grain"));
    SIM_CHECK(price_of(base.economy, kCity, "grain") <
              price_of(dear.economy, kCity, "grain"));
    return true;
}

static bool test_consume_floors_at_zero_and_junk_units_are_noops() {
    Rig rig;
    seed_market(rig.ctx, rig.economy, kCity);
    consume(rig.economy, kCity, "grain", 1000);
    SIM_CHECK_EQ(stock(rig.economy, kCity, "grain"), std::int64_t{0});  // never negative

    consume(rig.economy, kCity, "grain", 0);
    consume(rig.economy, kCity, "grain", -5);
    deliver(rig.economy, kCity, "grain", 0);
    deliver(rig.economy, kCity, "grain", -5);
    SIM_CHECK_EQ(stock(rig.economy, kCity, "grain"), std::int64_t{0});

    deliver(rig.economy, kCity, "grain", 50);
    consume(rig.economy, kCity, "grain", 50);  // round trip restores the stock
    SIM_CHECK_EQ(stock(rig.economy, kCity, "grain"), std::int64_t{0});
    return true;
}

// --- the drought ------------------------------------------------------------

static bool test_prices_respond_to_the_drought() {
    // Thirty market days from day 1 (no harvest window), stages 0..6: the
    // scarcity premium raises the anchor stage by stage.
    std::int64_t previous = 0;
    for (const int stage : {0, 2, 4, 6}) {
        Rig rig;
        rig.facts.drought_stage = stage;
        seed_market(rig.ctx, rig.economy, kCity);
        tick_economy(rig.ctx, rig.economy, 30);
        const Silver price = price_of(rig.economy, kCity, "grain");
        SIM_CHECK(price >= 1);
        SIM_CHECK(price > previous);  // monotone in the drought stage
        previous = price;
    }
    SIM_CHECK_EQ(previous, Silver{9});  // stage 6, stock drained to 70

    // The drought also kills the harvest: at stage 4 the fields fail. Ticked
    // on day 181, inside the harvest window, the wet world pays the fields in
    // while the dry world only draws down.
    Rig wet(181);
    wet.facts.drought_stage = 0;
    seed_market(wet.ctx, wet.economy, kCity);
    Rig dry(181);
    dry.facts.drought_stage = 4;
    seed_market(dry.ctx, dry.economy, kCity);
    tick_economy(wet.ctx, wet.economy, 1);
    tick_economy(dry.ctx, dry.economy, 1);
    SIM_CHECK_EQ(stock(wet.economy, kCity, "grain"), std::int64_t{111});  // -1 draw, +12 inflow
    SIM_CHECK_EQ(stock(dry.economy, kCity, "grain"), std::int64_t{99});   // inflow failed
    return true;
}

// --- the season -------------------------------------------------------------

static bool test_prices_respond_to_the_season() {
    // Default calendar: 360-day year, harvest window opens at its mid-point.
    // Same stock (30 units) ticked on either side of the boundary.
    const int days[] = {179, 180, 209, 210};
    std::int64_t stocks[4];
    Silver prices[4];
    for (int i = 0; i < 4; ++i) {
        Rig rig(days[i]);
        seed_market(rig.ctx, rig.economy, kCity);
        consume(rig.economy, kCity, "grain", 70);
        tick_economy(rig.ctx, rig.economy, 1);
        stocks[i] = stock(rig.economy, kCity, "grain");
        prices[i] = price_of(rig.economy, kCity, "grain");
        SIM_CHECK(prices[i] >= 1);
    }
    // Harvest days (180..209) pay the fields in; the days just outside do not.
    SIM_CHECK_EQ(stocks[0], std::int64_t{29});  // day 179: draw only
    SIM_CHECK_EQ(stocks[1], std::int64_t{41});  // day 180: draw + inflow
    SIM_CHECK_EQ(stocks[2], std::int64_t{41});  // day 209: still in the window
    SIM_CHECK_EQ(stocks[3], std::int64_t{29});  // day 210: window closed
    SIM_CHECK(prices[1] <= prices[0]);          // plenty is cheaper
    SIM_CHECK(prices[2] <= prices[3]);
    return true;
}

// --- demand -----------------------------------------------------------------

static bool test_prices_respond_to_demand() {
    Rig hungry;
    seed_market(hungry.ctx, hungry.economy, kCity);
    tick_economy(hungry.ctx, hungry.economy, 1);
    const Silver no_demand = price_of(hungry.economy, kCity, "grain");

    // Ten named residents whose home is the city bid the price up.
    Rig fed;
    for (int i = 0; i < 10; ++i) {
        Npc npc;
        npc.id = "npc_" + std::to_string(i);
        npc.home_city = kCity;
        fed.population.npcs.push_back(npc);
    }
    seed_market(fed.ctx, fed.economy, kCity);
    tick_economy(fed.ctx, fed.economy, 1);
    const Silver with_demand = price_of(fed.economy, kCity, "grain");
    SIM_CHECK(with_demand > no_demand);

    // The demand pressure is capped: more residents change nothing further.
    Rig crowded;
    for (int i = 0; i < 25; ++i) {
        Npc npc;
        npc.id = "npc_" + std::to_string(i);
        npc.home_city = kCity;
        crowded.population.npcs.push_back(npc);
    }
    seed_market(crowded.ctx, crowded.economy, kCity);
    tick_economy(crowded.ctx, crowded.economy, 1);
    SIM_CHECK_EQ(price_of(crowded.economy, kCity, "grain"), with_demand);

    // Residents of OTHER cities do not bid on this market.
    Rig elsewhere;
    Npc stranger;
    stranger.id = "stranger";
    stranger.home_city = "city_of_the_dead";
    elsewhere.population.npcs.push_back(stranger);
    seed_market(elsewhere.ctx, elsewhere.economy, kCity);
    tick_economy(elsewhere.ctx, elsewhere.economy, 1);
    SIM_CHECK_EQ(price_of(elsewhere.economy, kCity, "grain"), no_demand);
    return true;
}

// --- determinism ------------------------------------------------------------

static bool test_tick_is_byte_deterministic() {
    // Identical (db, seed, facts, days) => identical state bytes.
    Rig a;
    Rig b;
    for (const Row& row : a.db.rows("cities")) {
        seed_market(a.ctx, a.economy, row.at("id"));
        seed_market(b.ctx, b.economy, row.at("id"));
    }
    a.facts.drought_stage = 3;
    b.facts.drought_stage = 3;
    SIM_CHECK_EQ(fingerprint(a.economy), fingerprint(b.economy));
    tick_economy(a.ctx, a.economy, 400);
    tick_economy(b.ctx, b.economy, 400);
    SIM_CHECK_EQ(fingerprint(a.economy), fingerprint(b.economy));

    // Zero or negative days advance nothing.
    const std::string before = fingerprint(a.economy);
    tick_economy(a.ctx, a.economy, 0);
    tick_economy(a.ctx, a.economy, -7);
    SIM_CHECK_EQ(fingerprint(a.economy), before);
    return true;
}

// --- invariants under a long soak -------------------------------------------

static bool test_soak_never_negative_never_zero() {
    Rig rig;
    for (const Row& row : rig.db.rows("cities"))
        seed_market(rig.ctx, rig.economy, row.at("id"));

    Silver price_at_drought_onset = 0;
    for (DayNumber day = 1; day <= 400; ++day) {
        rig.ctx.day = day;
        rig.facts.drought_stage = day > 200 ? 8 : 0;  // the drought breaks mid-soak
        if (day == 100) deliver(rig.economy, kCity, "grain", 50);   // a ship lands
        if (day == 150) consume(rig.economy, kCity, "grain", 200);  // raiders hit
        tick_economy(rig.ctx, rig.economy, 1);
        for (const auto& [city, book] : rig.economy.market_by_city)
            for (const auto& [item, price] : book.silver_by_item) {
                SIM_CHECK(price >= 1);  // never negative, never zero — staples included
                (void)city;
            }
        for (const auto& [key, units] : rig.economy.stock_by_city_item) {
            SIM_CHECK(units >= 0);  // stocks never go negative
            (void)key;
        }
        if (day == 200) price_at_drought_onset = price_of(rig.economy, kCity, "grain");
    }
    // Two drought centuries without a harvest: the price curve has climbed.
    SIM_CHECK(price_of(rig.economy, kCity, "grain") > price_at_drought_onset);
    return true;
}

// --- graceful absence ---------------------------------------------------------

static bool test_absence_is_graceful() {
    // A fresh state knows no cities; the tick is a no-op, prices fall back.
    Rig rig;
    const EconomyState fresh;
    SIM_CHECK_EQ(fresh.market_by_city.size(), std::size_t{0});
    tick_economy(rig.ctx, rig.economy, 10);
    SIM_CHECK_EQ(fingerprint(rig.economy), fingerprint(fresh));

    SIM_CHECK_EQ(price_of(rig.economy, "nowhere", "grain"), Silver{2});
    SIM_CHECK(price_of(rig.economy, "nowhere", "grain") >= 1);

    // Shocks to an unseeded city land in the stock book only — no market is
    // invented, and prices stay on the fallback.
    deliver(rig.economy, "nowhere", "grain", 5);
    SIM_CHECK_EQ(stock(rig.economy, "nowhere", "grain"), std::int64_t{5});
    SIM_CHECK(rig.economy.market_by_city.find("nowhere") == rig.economy.market_by_city.end());
    SIM_CHECK_EQ(price_of(rig.economy, "nowhere", "grain"), Silver{2});
    consume(rig.economy, "nowhere", "grain", 99);
    SIM_CHECK_EQ(stock(rig.economy, "nowhere", "grain"), std::int64_t{0});
    return true;
}

}  // namespace

SIM_MAIN(test_seed_market_knows_the_city,
         test_seed_market_is_idempotent_and_covers_canon_cities,
         test_price_of_fallback_anchors_to_grain,
         test_prices_respond_to_supply,
         test_consume_floors_at_zero_and_junk_units_are_noops,
         test_prices_respond_to_the_drought,
         test_prices_respond_to_the_season,
         test_prices_respond_to_demand,
         test_tick_is_byte_deterministic,
         test_soak_never_negative_never_zero,
         test_absence_is_graceful)
