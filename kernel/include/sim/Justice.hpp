#pragma once
// Justice.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// The justice pipeline (city-life §3): crime → witness → alarm → pursuit →
// detention → hearing → sealed verdict. Detention, never prison terms.
//
// Invariants:
//  - verdicts come from db/canon/laws.csv when a row exists for the crime;
//    until law content is authored (Phase 5 storm), the pipeline falls back to
//    "compensation" — the age's most common outcome (city-life §3.3)
//  - a verdict is an event with a day; the sealed-tablet ITEM is engine content
//  - deterministic; writes ONLY JusticeState
#include "sim/Types.hpp"

#include <map>
#include <vector>


namespace sim {

struct WorldContext;  // defined in sim/Context.hpp (the seam — never included by module headers)

struct Crime {
    Id id;              // "crime_<n>" in sequence
    Id criminal;        // npc id or "player"
    Id law_row;         // laws.csv id (e.g. "theft"); "" = unspecified
    Id crime_kind;      // coarse kind (free-form; laws.csv ships rows for theft, burglary,
                        // assault, murder, sorcery, sacrilege, oath_breaking, tomb_robbery,
                        // fraud, harbouring_fugitive)
    DayNumber day = 0;
    Id witnessed_by;    // npc id, or "" when unwitnessed (evidence remains)
    bool atoned = false;
    // W2-A (kernel/src/Actions.cpp): the pre-hearing stages city-life §3.3
    // names (alarm -> pursuit -> detention). "" until a crime orchestrated
    // through commit_crime() sets it; report_crime()/hold_hearing() never
    // read or write it themselves.
    std::string stage;  // "" | "alarmed" | "pursued" | "detained" | "heard"
};

struct Hearing {
    Id crime_id;
    DayNumber day = 0;
    std::string verdict;  // "compensation" | "confiscation" | "debt_service" | "exile" | "death" | "dismissed"
    // W2-A additions (kernel/src/Actions.cpp fills these; hold_hearing()
    // itself never touches them — it writes ONLY the fields above):
    Id tablet_id;               // "verdict_tablet_<crime_id>" once sealed
    Silver compensation_paid = 0;  // silver actually moved from the purse, 0 if none/unpaid
};

struct JusticeState {
    std::vector<Crime> open_crimes;
    std::vector<Hearing> verdicts;
    int next_id = 1;
};

void tick_justice(const WorldContext& ctx, JusticeState& state, int days = 1);

// A perceived act enters the pipeline. Unwitnessed crimes stay open (evidence
// can be found later).
Crime& report_crime(JusticeState& state, const Id& criminal, const Id& law_row,
                    const Id& crime_kind, DayNumber day, const Id& witnessed_by);

// Holds the hearing for a crime and moves it to verdicts. Uses laws.csv when
// the row exists, else the compensation fallback.
Hearing hold_hearing(const WorldContext& ctx, JusticeState& state, const Id& crime_id);

const Crime* find_crime(const JusticeState& state, const Id& crime_id);

}  // namespace sim
