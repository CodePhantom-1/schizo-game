// Economy.cpp — the price engine: silver by weight, grain as the base good.
// (mechanics row 3: the WorldState daily tick prices the world; parent
// game-design §6: no coins ever — silver by weight, barley/grain is the base
// unit, "grain prices are the game's heartbeat"; living-world §3: prices
// respond to supply, demand, season and the Act — here the drought.)
//
// The model is pure integer arithmetic over EconomyState plus the WorldContext
// reads, so the tick is byte-deterministic: identical (db, seed, facts, days)
// => identical state. Nothing here is stochastic, so the module draws no
// randomness from ctx.rng (a later wave may add market noise through the
// forked stream if the design wants it); there is no wall clock, no global or
// static mutable state, and no thread.
//
// INVENTED machinery (no canon exists for it yet — items.csv is empty, season
// names and price bands are OPEN data): the base-good id "grain", the base
// price, the reference stock, the daily market draw, the mid-year harvest
// window, and the per-stage drought multipliers. No canon data is invented or
// resolved here; OPEN canon rows are treated as absent.
#include "sim/Economy.hpp"

#include "sim/Context.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace sim {
namespace {

// --- mechanical constants -------------------------------------------------

constexpr const char* kGrain = "grain";  // the base good (INVENTED id: no canon items row exists yet)
constexpr Silver kGrainBasePrice = 2;    // silver grains per grain unit at reference stock
constexpr std::int64_t kReferenceStock = 100;  // units that count as a "normal" market
constexpr std::int64_t kDailyDraw = 1;   // units a city eats off the market each market day
constexpr int kHarvestInflow = 12;       // units/day of harvest inflow at drought 0 (balances kDailyDraw over the year)
constexpr int kDroughtPctPerStage = 50;  // the grain anchor rises +50% per drought stage
constexpr int kMaxDroughtStage = 20;     // anchor cap (x11) — also an overflow guard
constexpr int kDroughtSupplyPctPerStage = 25;  // harvest inflow -25%/stage; at stage 4+ the fields fail
constexpr int kDemandPctPerNpc = 5;      // +5% per named resident whose home is the city
constexpr int kDemandCapNpcs = 10;       // demand pressure cap (real demand arrives with light residents)
constexpr Silver kMinPrice = 1;          // prices never go negative; food staples never reach zero

// --- stock bookkeeping ----------------------------------------------------

// stock_by_city_item is keyed "city_id/item_id" (the header's documented key).
std::string stock_key(const Id& city, const Id& item) { return city + "/" + item; }

std::int64_t market_stock(const EconomyState& state, const Id& city, const Id& item) {
    const auto it = state.stock_by_city_item.find(stock_key(city, item));
    return it == state.stock_by_city_item.end() ? 0 : it->second;
}

std::int64_t& market_stock_ref(EconomyState& state, const Id& city, const Id& item) {
    return state.stock_by_city_item[stock_key(city, item)];
}

// --- the price model ------------------------------------------------------

// Demand pressure: named residents of the city bid the anchor up. Light
// residents (the real demand) are a later wave; absent them, pressure is 0.
int demand_pct(const WorldContext& ctx, const Id& city) {
    int residents = 0;
    for (const Npc& npc : ctx.population.npcs)
        if (npc.home_city == city) ++residents;
    return std::min(residents, kDemandCapNpcs) * kDemandPctPerNpc;
}

// The drought-and-demand-adjusted grain anchor every other price scales from.
Silver grain_anchor(const WorldContext& ctx, const Id& city) {
    const int stage = std::clamp(ctx.facts.drought_stage, 0, kMaxDroughtStage);
    Silver anchor = kGrainBasePrice;
    anchor = anchor * (100 + kDroughtPctPerStage * stage) / 100;  // scarcity premium
    anchor = anchor * (100 + demand_pct(ctx, city)) / 100;        // mouths to feed
    return anchor;
}

// Supply curve: twice the reference stock halves the price; empty shelves
// double it. Stock far above the reference is clamped for the computation
// only — the floor is reached long before the clamp.
Silver price_from_anchor(Silver anchor, std::int64_t stock) {
    const std::int64_t s = std::min<std::int64_t>(stock, kReferenceStock * 1000);
    const Silver price = anchor * (2 * kReferenceStock) / (kReferenceStock + s);
    return std::max<Silver>(price, kMinPrice);
}

// A canon item's price band: the leading integer of items.price_band; anything
// else (absent table, empty field, OPEN row, non-numeric text) anchors at 1.
std::int64_t band_of(const WorldContext& ctx, const Id& item) {
    const auto row = ctx.db.find("items", item);
    if (!row || row->get("tag") == "OPEN") return 1;  // OPEN rows: content does not exist yet
    std::int64_t band = 0;
    for (const char c : row->get("price_band")) {
        if (c < '0' || c > '9') break;
        band = band * 10 + (c - '0');
    }
    return band > 0 ? band : 1;
}

// Season: the harvest window. The canon gives no season names (calendar.csv is
// OPEN), so the machinery keys on the calendar's shape, not its labels: a
// half-year mid-point opens a dpy/12-day window of inflow on any calendar.
bool in_harvest(const WorldContext& ctx, DayNumber day) {
    const int dpy = ctx.cal.days_per_year();
    if (dpy <= 0) return false;
    const int mid = std::max(1, dpy / 2);
    const int window = std::max(1, dpy / 12);
    const int doy = ctx.cal.day_of_year(day);
    return doy >= mid && doy < mid + window;
}

int harvest_inflow_per_day(const WorldContext& ctx) {
    const int stage = std::clamp(ctx.facts.drought_stage, 0, 4);
    return kHarvestInflow * (100 - kDroughtSupplyPctPerStage * stage) / 100;
}

// Reprices every entry of one city's book from the current stocks and facts.
void reprice(const WorldContext& ctx, EconomyState& state, const Id& city) {
    const auto cit = state.market_by_city.find(city);
    if (cit == state.market_by_city.end()) return;
    const Silver anchor = grain_anchor(ctx, city);
    for (auto& [item, price] : cit->second.silver_by_item) {
        if (item == kGrain)
            price = price_from_anchor(anchor, market_stock(state, city, item));
        else
            price = price_from_anchor(anchor * band_of(ctx, item),
                                      market_stock(state, city, item));
    }
}

}  // namespace

// --- the contract -----------------------------------------------------------

void tick_economy(const WorldContext& ctx, EconomyState& state, int days) {
    for (int i = 0; i < days; ++i) {
        const DayNumber day = ctx.day + i;  // the i-th of the requested market days
        const bool harvesting = in_harvest(ctx, day);
        const int inflow = harvest_inflow_per_day(ctx);
        for (const auto& [city, book] : state.market_by_city) {
            if (book.silver_by_item.find(kGrain) != book.silver_by_item.end()) {
                std::int64_t& stock = market_stock_ref(state, city, kGrain);
                stock -= kDailyDraw;              // the city eats off the market
                if (harvesting) stock += inflow;  // the fields pay in
                if (stock < 0) stock = 0;         // shelves empty; stocks never go negative
            }
            reprice(ctx, state, city);  // no insertion — the loop's iterators stay valid
        }
    }
}

Silver price_of(const EconomyState& state, const Id& city, const Id& item) {
    const auto cit = state.market_by_city.find(city);
    if (cit != state.market_by_city.end()) {
        const auto it = cit->second.silver_by_item.find(item);
        if (it != cit->second.silver_by_item.end()) return it->second;
        // Grain-anchored fallback for unknown items: grain base x band. The
        // band is items-canon data and price_of has no Db, so an unknown item
        // anchors at band 1 — the city's current grain price.
        const auto grain = cit->second.silver_by_item.find(kGrain);
        if (grain != cit->second.silver_by_item.end())
            return std::max<Silver>(grain->second, kMinPrice);
    }
    // Unknown city too: the bare grain base.
    return std::max<Silver>(kGrainBasePrice, kMinPrice);
}

void deliver(EconomyState& state, const Id& city, const Id& item, std::int64_t units) {
    if (units <= 0) return;
    market_stock_ref(state, city, item) += units;  // a ship or caravan lands
}

void consume(EconomyState& state, const Id& city, const Id& item, std::int64_t units) {
    if (units <= 0) return;
    std::int64_t& stock = market_stock_ref(state, city, item);  // theft or raid
    stock -= units;
    if (stock < 0) stock = 0;
}

void seed_market(const WorldContext& ctx, EconomyState& state, const Id& city) {
    // Cities come from db/canon/cities.csv (the caller passes canon ids); a
    // market is seeded for whichever id the caller names — a missing or OPEN
    // canon row is no reason to crash. Idempotent: re-seeding changes nothing.
    PriceBook& book = state.market_by_city[city];

    // Grain is always on the market: the base good every book anchors to.
    if (book.silver_by_item.find(kGrain) == book.silver_by_item.end())
        book.silver_by_item[kGrain] = kGrainBasePrice;
    const std::string grain_key = stock_key(city, kGrain);
    if (state.stock_by_city_item.find(grain_key) == state.stock_by_city_item.end())
        state.stock_by_city_item[grain_key] = kReferenceStock;

    // Canon items, once the items table grows rows: each sits on the shelf at
    // grain base x band. Nothing is produced yet (producers are a later wave),
    // so their stock starts empty. OPEN rows are treated as absent.
    for (const Row& row : ctx.db.rows("items")) {
        if (row.get("tag") == "OPEN") continue;
        // Stations (quern, oven, vat) are fixtures, not goods; water is drawn
        // at the well (Needs/Crafting) — neither sits on a market shelf.
        if (row.get("category") == "station" || row.at("id") == "water") continue;
        const Id item = row.at("id");
        if (book.silver_by_item.find(item) != book.silver_by_item.end()) continue;
        book.silver_by_item[item] =
            std::max<Silver>(kGrainBasePrice * band_of(ctx, item), kMinPrice);
        const std::string key = stock_key(city, item);
        if (state.stock_by_city_item.find(key) == state.stock_by_city_item.end())
            state.stock_by_city_item[key] = 0;
    }
}

}  // namespace sim
