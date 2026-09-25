// Actions.cpp — implements sim/Actions.hpp (see its header for the contract
// and the scenario_crime.md / scenario_quest.md gaps each verb closes).
//
// Writes to several modules' state directly (Population, Justice, Property,
// Faction, Quests) — the one file besides World.cpp allowed to do that.
// Every module's own tick_*/accept/complete/etc. keeps writing only its own
// state; this file only sequences those calls and a few public-field writes
// (Crime::stage, Hearing::tablet_id/compensation_paid) the individual module
// contracts explicitly leave to it (see the W2-A comments on those fields).
#include "sim/Actions.hpp"

#include <algorithm>
#include <map>

namespace sim {
namespace {

std::string trim(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string ascii_lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::vector<std::string> split_semi(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        const std::size_t semi = s.find(';', start);
        const std::size_t end = semi == std::string::npos ? s.size() : semi;
        out.push_back(ascii_lower(trim(s.substr(start, end - start))));
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    return out;
}

// city-life §3.3.6: when the sentenced verdict can't be carried out (here:
// compensation the criminal's purse can't cover), the next penalty option
// the law row itself lists takes over; "debt_service" is the INVENTED
// fallback (city-life §3.3.6 names it as a real system of the age) when the
// row names nothing further or no row exists at all.
std::string escalate_verdict(const Db& db, const Id& law_row, const std::string& from) {
    if (!law_row.empty()) {
        if (const auto row = db.find("laws", law_row)) {
            const std::vector<std::string> options = split_semi(row->get("penalty_options"));
            const auto it = std::find(options.begin(), options.end(), from);
            if (it != options.end() && std::next(it) != options.end()) return *std::next(it);
        }
    }
    return "debt_service";
}

// INVENTED standing-loss ladder (no canon number exists for "how much a
// verdict costs your standing"; scaled by severity, retunable).
int standing_penalty(const std::string& verdict) {
    static const std::map<std::string, int> kSeverity = {
        {"dismissed", 0},    {"compensation", -5}, {"confiscation", -10},
        {"debt_service", -8}, {"exile", -25},        {"death", -40},
    };
    const auto it = kSeverity.find(verdict);
    return it == kSeverity.end() ? 0 : it->second;
}

}  // namespace

Id commit_crime(WorldState& w, const Id& criminal, const Id& law_row,
                const Id& place_city, const std::vector<Id>& witnesses) {
    Id first_witness;
    for (const Id& npc_id : witnesses) {
        witness(w.population, npc_id, criminal,
                "witnessed a " + law_row + " at " + place_city, w.day);
        if (first_witness.empty()) first_witness = npc_id;
    }

    Crime& crime = report_crime(w.justice, criminal, law_row, law_row, w.day, first_witness);
    if (!first_witness.empty()) {
        // city-life §3.1/§3.3.2-3: alarm -> the watch's pursuit -> detention.
        // Wave 1 collapses the chase itself into a stage transition, same
        // scope as Justice.cpp's own same-tick hearing; the field exists so a
        // later, per-tick guard simulation has somewhere to land each step.
        crime.stage = "alarmed";
        crime.stage = "pursued";
        crime.stage = "detained";
    }
    return crime.id;
}

Id city_faction(const Id& place_city) {
    static const std::map<Id, Id> kJurisdiction = {
        {"city_of_jewels", "the_empire"},
        {"city_of_the_dead", "dead_cult"},
        {"gravestone", "dead_cult"},
        {"city_of_the_sky", "sky_cult"},
        {"city_of_kings", "the_empire"},
        {"city_of_the_sun", "retributors_of_utu"},
        {"city_of_the_moon", "southern_city_states_alliance"},
        {"city_of_the_abyss", "the_empire"},
        {"city_of_the_warrior_spirit", "eastern_barbarian_alliance"},
    };
    const auto it = kJurisdiction.find(place_city);
    return it == kJurisdiction.end() ? Id("the_empire") : it->second;
}

Hearing hold_crime_hearing(WorldState& w, const Id& crime_id, const Id& victim,
                           const Id& place_city) {
    // Captured before hold_hearing() removes the crime from the docket.
    Id law_row, criminal;
    if (const Crime* c = find_crime(w.justice, crime_id)) {
        law_row = c->law_row;
        criminal = c->criminal;
    }

    const WorldContext ctx = w.context();
    (void)hold_hearing(ctx, w.justice, crime_id);  // seals into w.justice.verdicts
    Hearing& sealed = w.justice.verdicts.back();
    sealed.tablet_id = "verdict_tablet_" + crime_id;

    if (sealed.verdict == "compensation" && !criminal.empty()) {
        if (take_from_purse(w.property, criminal, kCompensationSilver)) {
            if (!victim.empty()) credit_purse(w.property, victim, kCompensationSilver);
            sealed.compensation_paid = kCompensationSilver;
        } else {
            sealed.verdict = escalate_verdict(w.db, law_row, "compensation");
        }
    }

    const Id faction = city_faction(place_city);
    add_standing(w.faction, faction, standing_penalty(sealed.verdict));
    if (sealed.verdict == "exile" || sealed.verdict == "death") outlaw(w.faction, faction);

    return sealed;
}

void complete_quest(WorldState& w, const Id& def_id, DayNumber day) {
    const QuestDef* def = nullptr;
    for (const QuestDef& d : w.quests.defs)
        if (d.id == def_id) { def = &d; break; }

    complete(w.quests, def_id, day);  // Quests::complete — unchanged, active -> completed

    if (def == nullptr) return;  // no canon def loaded: no reward data to pay
    if (def->reward_silver > 0) credit_purse(w.property, "player", def->reward_silver);
    if (!def->reward_faction.empty() && def->reward_standing != 0)
        add_standing(w.faction, def->reward_faction, def->reward_standing);
}

void fail_quest(WorldState& w, const Id& def_id, DayNumber day) {
    (void)day;
    fail(w.quests, def_id);  // Quests::fail — no reward on the failure path
}

}  // namespace sim
