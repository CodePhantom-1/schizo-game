// Justice.cpp — the crime → witness → hearing → sealed-verdict pipeline
// (implements the frozen API in include/sim/Justice.hpp).
//
// Determinism notes:
//  - no wall clock, no static mutable state, no threads, and no randomness:
//    the pipeline needs no draws (verdict selection reads canon in row order;
//    open crimes are processed in report order), so state bytes depend only on
//    the sequence of calls. ctx.rng is left untouched on purpose.
//  - writes ONLY JusticeState; the world is read through the const
//    WorldContext (canon via sim::Db only).
//
// Scope note: alarm → pursuit → detention are the guards'/pursuit layer's
// stages (mechanics.md row 12); the frozen API and JusticeState carry no
// detention surface, so this file implements the witnessed crime's path from
// the docket to the sealed verdict. Detention, never prison terms.
#include "sim/Justice.hpp"

#include "sim/Context.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace sim {
namespace {

char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string ascii_lower(std::string s) {
    for (char& c : s) c = ascii_lower(c);
    return s;
}

std::string trim(const std::string& s) {
    const auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    };
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && is_space(s[b])) ++b;
    while (e > b && is_space(s[e - 1])) --e;
    return s.substr(b, e - b);
}

// The verdict domain every Hearing.verdict is drawn from (Justice.hpp).
bool is_known_verdict(const std::string& v) {
    return v == "compensation" || v == "confiscation" || v == "debt_service" ||
           v == "exile" || v == "death" || v == "dismissed";
}

// OPEN rows mark missing canon (db/schema/laws.md: "OPEN rows mark missing
// canon; they cannot ship") — a row whose content does not exist yet must not
// decide a verdict. Canon tags begin with "OPEN" (plain or "OPEN (...)").
bool row_is_open(const Row& row) { return ascii_lower(trim(row.get("tag"))).rfind("open", 0) == 0; }

// The verdict the law row prescribes: the first option of penalty_options
// (canon lists are ';'-separated), when it names a documented verdict.
// Returns "" when the row yields no usable verdict.
std::string prescribed_verdict(const Row& row) {
    std::string options = row.get("penalty_options");
    const std::size_t semi = options.find(';');
    if (semi != std::string::npos) options = options.substr(0, semi);
    const std::string v = ascii_lower(trim(options));
    return is_known_verdict(v) ? v : std::string{};
}

std::vector<Crime>::iterator find_open(JusticeState& state, const Id& crime_id) {
    return std::find_if(state.open_crimes.begin(), state.open_crimes.end(),
                        [&](const Crime& c) { return c.id == crime_id; });
}

}  // namespace

Crime& report_crime(JusticeState& state, const Id& criminal, const Id& law_row,
                    const Id& crime_kind, DayNumber day, const Id& witnessed_by) {
    Crime crime;
    crime.id = "crime_" + std::to_string(state.next_id);
    crime.criminal = criminal;
    crime.law_row = law_row;
    crime.crime_kind = crime_kind;
    crime.day = day;
    crime.witnessed_by = witnessed_by;  // "" = unwitnessed: evidence remains
    state.open_crimes.push_back(std::move(crime));
    ++state.next_id;
    return state.open_crimes.back();
}

Hearing hold_hearing(const WorldContext& ctx, JusticeState& state, const Id& crime_id) {
    const auto it = find_open(state, crime_id);
    if (it == state.open_crimes.end()) {
        // Nothing on the docket (unknown id, or already heard and sealed): the
        // hearing is dismissed and nothing new is sealed.
        return Hearing{crime_id, ctx.day, "dismissed"};
    }

    // Verdicts come from db/canon/laws.csv when a usable row exists for the
    // crime; until law content is authored (Phase 5 storm), the age's most
    // common outcome — compensation — decides (city-life §3.3).
    std::string verdict = "compensation";
    if (!it->law_row.empty()) {
        const std::optional<Row> row = ctx.db.find("laws", it->law_row);
        if (row && !row_is_open(*row)) {
            const std::string prescribed = prescribed_verdict(*row);
            if (!prescribed.empty()) verdict = prescribed;
        }
    }

    const Hearing hearing{it->id, ctx.day, verdict};
    state.verdicts.push_back(hearing);
    state.open_crimes.erase(it);
    return hearing;
}

const Crime* find_crime(const JusticeState& state, const Id& crime_id) {
    const auto it = std::find_if(state.open_crimes.begin(), state.open_crimes.end(),
                                 [&](const Crime& c) { return c.id == crime_id; });
    return it == state.open_crimes.end() ? nullptr : &*it;
}

void tick_justice(const WorldContext& ctx, JusticeState& state, int days) {
    if (days <= 0) return;
    for (int i = 0; i < days; ++i) {
        // A witnessed crime completes alarm → pursuit → detention → hearing on
        // the morning it is processed, in report order. Unwitnessed crimes stay
        // open: the evidence can be found later (Justice.hpp). Ids are
        // collected first — a hearing removes its crime from the docket.
        // W2-A: a crime with a scheduled hearing_day (commit_crime()) waits
        // until that day; hearing_day == 0 keeps the original same-tick
        // behaviour for every crime filed via plain report_crime().
        std::vector<Id> to_hear;
        for (const Crime& c : state.open_crimes)
            // hearing_day > 0 marks a world-orchestrated crime (commit_crime):
            // WorldState::advance_days hears it via hold_due_hearings so the
            // verdict's consequences apply — this module must not seal it bare.
            if (!c.witnessed_by.empty() && c.hearing_day == 0)
                to_hear.push_back(c.id);
        for (const Id& id : to_hear) (void)hold_hearing(ctx, state, id);
    }
}

}  // namespace sim
