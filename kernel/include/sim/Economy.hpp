#pragma once
// Economy.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// The price engine: silver-by-weight, grain as the base good (mechanics row 3;
// game-design §6 of the parent). Prices respond to supply, demand, season and
// the drought (WorldFacts::drought_stage).
//
// Invariants:
//  - prices are integral Silver (grains); never negative; never zero for food staples
//  - the tick is deterministic: identical (db, seed, facts, days) => identical state
//  - reads db tables: items (empty for now), regions (city context later)
//  - writes ONLY EconomyState
//  - may READ other states via WorldContext (const), never write them
#include "sim/Types.hpp"

#include <map>


namespace sim {

struct WorldContext;  // defined in sim/Context.hpp (the seam — never included by module headers)

// Per-city book: what one silver item costs in one city's market.
struct PriceBook {
    std::map<Id, Silver> silver_by_item;
    bool operator==(const PriceBook&) const = default;  // determinism comparisons
};

struct EconomyState {
    // market_by_city[city_id].silver_by_item[item_id]
    std::map<Id, PriceBook> market_by_city;
    // stock_by_city_item[(city_id + "/" + item_id)] — units on the market
    std::map<Id, std::int64_t> stock_by_city_item;
};

// Advances the economy by `days` market days (call with days=1 on the daily tick).
void tick_economy(const WorldContext& ctx, EconomyState& state, int days = 1);

// Current price, or the grain-anchored fallback for unknown items (grain base x band).
Silver price_of(const EconomyState& state, const Id& city, const Id& item);

// Supply shock: goods arriving (ship, caravan) or leaving (theft, raid).
void deliver(EconomyState& state, const Id& city, const Id& item, std::int64_t units);
void consume(EconomyState& state, const Id& city, const Id& item, std::int64_t units);

// Cities a fresh EconomyState knows about (seeded from db/canon/cities.csv).
void seed_market(const WorldContext& ctx, EconomyState& state, const Id& city);

}  // namespace sim
