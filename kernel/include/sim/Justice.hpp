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
struct DivineState;   // defined in sim/Divine.hpp (W5: divine wrath)

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
    Id victim;          // W2 review: who is owed compensation ("" = the community)
    Id place_city;      // W2 review: where it happened — the jurisdiction
    std::string stage;  // "" | "alarmed" | "pursued" | "detained" | "heard"
    // W2-A: 0 (default) keeps the original Wave-1 behaviour — tick_justice()
    // hears any witnessed crime the same tick it processes it. A nonzero day
    // (set by commit_crime()) makes tick_justice() wait until that day before
    // auto-hearing it — the real window scenario_crime.md gap 1 asks for,
    // between detention and the hearing. Calling hold_hearing()/
    // hold_crime_hearing() directly, at any time, still works regardless.
    DayNumber hearing_day = 0;
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
// W5 (additive, defaulted): when `divine` is given, a conviction (verdict !=
// dismissed) for an offence against the gods also adds divine wrath
// (sim/Divine.hpp note_divine_conviction). Null keeps the Wave-1 behaviour.
Hearing hold_hearing(const WorldContext& ctx, JusticeState& state, const Id& crime_id,
                     DivineState* divine = nullptr);

const Crime* find_crime(const JusticeState& state, const Id& crime_id);

}  // namespace sim
