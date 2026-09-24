#pragma once
// Types.hpp — common vocabulary for every module (contract, coordinator-owned).
#include <cstdint>
#include <string>

namespace sim {

// The world's clock counts days. Everything else derives.
using DayNumber = std::int64_t;

// Silver is measured in grains — integral on purpose: the simulation must be
// byte-deterministic across runs and saves (INVENTED abstraction; the canon
// says silver by weight, mechanics.md row 3).
using Silver = std::int64_t;

// Canon ids are the `id` column of db/canon/*.csv (e.g. "inanna", "city_of_the_moon").
using Id = std::string;

struct Date {
    int year = 1;
    int month = 1;  // 1..CalendarConfig::months_per_year
    int day = 1;    // 1..CalendarConfig::days_per_month
    bool operator==(const Date&) const = default;
};

// The coordinator-owned world facts: scalar act content every module may read
// and none may write. The drought that breaks the world lives here (notes L15,
// L104) — its stage curve is Act content (INVENTED abstraction, OPEN data).
struct WorldFacts {
    int drought_stage = 0;  // 0 = none; rises through the acts
    int war_stage = 0;      // 0 = peace; rises as the eastern raids and the southern rebellion grow
};

}  // namespace sim
