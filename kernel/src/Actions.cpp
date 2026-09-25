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
#include <optional>

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

bool contains(const std::string& haystack_lower, const char* needle) {
    return haystack_lower.find(needle) != std::string::npos;
}

// K-1 gap 5: interprets a SUCCESSFUL RiteResult by effect family
// (rpg-systems §10.3), applying it to the world. A performed-but-failed
// attempt already took its favour penalty inside sim::perform_rite
// (Magic.cpp); nothing further happens to a failure here — "the effect is
// the game" (Magic.hpp:22) only on success.
void apply_rite_effect(WorldState& w, const Id& performer, const RiteResult& result) {
    const std::string family = ascii_lower(result.effect_family);

    if (contains(family, "protection") || contains(family, "blessing") ||
        contains(family, "curse") || contains(family, "binding")) {
        const bool is_curse = contains(family, "curse") || contains(family, "binding");
        const std::string kind = is_curse ? "curse" : contains(family, "blessing")
                                                           ? "blessing"
                                                           : "protection";
        add_ward(w.magic, result.rite_id, result.deity, performer, kind, w.day, kWardDurationDays);
        // rpg-systems §10.3: "sorcery is a crime" — a curse costs the
        // performer's own purity even when no one catches them (INVENTED).
        if (is_curse) w.magic.purity = std::max(0, w.magic.purity - kCursePurityCost);
        return;
    }

    if (contains(family, "divination") || contains(family, "omen")) {
        // rpg-systems §10.3: "omens with probabilities, not certainties" —
        // the reading is a coarse INVENTED banding of the rite's own score,
        // never a flat true/false.
        const std::string reading = result.score >= 0.7   ? "favourable"
                                    : result.score >= 0.4  ? "uncertain"
                                                            : "ill-favoured";
        add_omen(w.magic, result.rite_id, result.deity, w.day, reading, result.score);
        return;
    }

    if (contains(family, "healing") || contains(family, "purification")) {
        if (contains(family, "healing")) {
            Needs& n = needs_of(w.needs, performer);
            n.hunger = std::max(0, n.hunger - kHealingRelief);
            n.thirst = std::max(0, n.thirst - kHealingRelief);
            n.fatigue = std::max(0, n.fatigue - kHealingRelief);
        }
        if (contains(family, "purification"))
            w.magic.purity = std::min(100, w.magic.purity + kPurificationBoost);
        return;
    }

    // "favour"/"offering" families: the base +2/-5 favour swing already
    // applied inside sim::perform_rite covers this (Magic.cpp reading 2).
    // Substitution, ancestors and divine intervention: no canon rite names
    // these families yet (scenario_rite.md "not exercised, out of scope") —
    // left a no-op rather than guessed at.
}

}  // namespace

Id commit_crime(WorldState& w, const Id& criminal, const Id& law_row,
                const Id& place_city, const std::vector<Id>& witnesses, const Id& victim) {
    Id first_witness;
    for (const Id& npc_id : witnesses) {
        witness(w.population, npc_id, criminal,
                "witnessed a " + law_row + " at " + place_city, w.day);
        if (first_witness.empty()) first_witness = npc_id;
    }

    Crime& crime = report_crime(w.justice, criminal, law_row, law_row, w.day, first_witness);
    crime.victim = victim;
    crime.place_city = place_city;
    if (!first_witness.empty()) {
        // city-life §3.1/§3.3.2-3: alarm -> the watch's pursuit -> detention.
        // Wave 1 collapses the chase itself into a stage transition, same
        // scope as Justice.cpp's own same-tick hearing; the field exists so a
        // later, per-tick guard simulation has somewhere to land each step.
        crime.stage = "alarmed";
        crime.stage = "pursued";
        crime.stage = "detained";
        // Schedules the real window between detention and the hearing
        // (city-life §3.3.3; scenario_crime.md gap 1) — tick_justice() will
        // not auto-hear this crime before w.day + kHearingDelayDays.
        crime.hearing_day = w.day + kHearingDelayDays;
    }
    return crime.id;
}

Id city_faction(const Db& db, const Id& place_city) {
    std::optional<Row> city = db.find("cities", place_city);
    if (city && !city->get("alias_of").empty()) city = db.find("cities", city->get("alias_of"));
    const Id j = city ? city->get("jurisdiction") : Id{};
    return j.empty() ? Id("the_empire") : j;  // the Empire claims every unassigned place
}

void hold_due_hearings(WorldState& w) {
    std::vector<Crime> due;
    for (const Crime& c : w.justice.open_crimes)
        if (!c.witnessed_by.empty() && c.hearing_day > 0 && w.day >= c.hearing_day) due.push_back(c);
    for (const Crime& c : due) (void)hold_crime_hearing(w, c.id, c.victim, c.place_city);
}

Hearing hold_crime_hearing(WorldState& w, const Id& crime_id, const Id& victim,
                           const Id& place_city) {
    // Captured before hold_hearing() removes the crime from the docket.
    Id law_row, criminal;
    const Crime* c = find_crime(w.justice, crime_id);
    if (c == nullptr)  // unknown or already heard: nothing sealed, no consequences
        return Hearing{crime_id, w.day, "dismissed", "", 0};
    law_row = c->law_row;
    criminal = c->criminal;

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

    const Id faction = city_faction(w.db, place_city);
    add_standing(w.faction, faction, standing_penalty(sealed.verdict));
    if (sealed.verdict == "exile" || sealed.verdict == "death") outlaw(w.faction, faction);

    return sealed;
}

void complete_quest(WorldState& w, const Id& def_id, DayNumber day) {
    const QuestDef* def = nullptr;
    for (const QuestDef& d : w.quests.defs)
        if (d.id == def_id) { def = &d; break; }

    if (find_active(w.quests, def_id) == nullptr) return;  // only an active quest pays, once
    complete(w.quests, def_id, day);  // Quests::complete — unchanged, active -> completed

    if (def == nullptr) return;  // no canon def loaded: no reward data to pay
    if (def->reward_silver > 0) credit_purse(w.property, "player", def->reward_silver);
    if (!def->reward_faction.empty() && def->reward_standing != 0)
        add_standing(w.faction, def->reward_faction, def->reward_standing);
}

void fail_quest(WorldState& w, const Id& def_id, DayNumber day) {
    if (find_active(w.quests, def_id) == nullptr) return;
    fail(w.quests, def_id);  // Quests::fail — no reward on the failure path
    w.quests.journal[def_id].push_back(JournalEntry{day, "failed", "abandoned"});
}

RiteResult perform_rite_action(WorldState& w, const Id& performer, const Id& rite_id,
                               const Id& place) {
    RiteInputs inputs;
    inputs.performer_knows_rite = knows_rite(w.magic, rite_id);

    // Read-only lookup: a performer never seen before must not create an
    // inventory entry on a path that may still end in refusal.
    const std::vector<Id> keys = rite_material_keys(w.db, rite_id);
    const auto inv_it = w.inventories.find(performer);
    for (const Id& key : keys) {
        if (key == "water") {  // Crafting.cpp/Needs.cpp rule: free, never held
            inputs.materials_held[key] = 1;
            continue;
        }
        int have = 0;
        if (inv_it != w.inventories.end()) {
            const auto it = inv_it->second.counts.find(key);
            if (it != inv_it->second.counts.end()) have = static_cast<int>(it->second);
        }
        inputs.materials_held[key] = have;
    }

    w.magic.place = place;
    const RiteResult result = perform_rite(w.context(), w.magic, rite_id, inputs);
    if (!result.performed) return result;  // refusal: no inventory entry ever created, nothing consumed

    // Magic.hpp: "a failed rite still consumes the materials"; materials are
    // presence-only (Magic.cpp reading 5), so a performed attempt (success
    // or failure) consumes exactly 1 unit of each required non-water key.
    Inventory& inv = w.inventories[performer];
    for (const Id& key : keys) {
        if (key == "water") continue;
        const auto it = inv.counts.find(key);
        if (it == inv.counts.end() || it->second <= 0) continue;
        it->second -= 1;
        if (it->second <= 0) inv.counts.erase(it);
    }

    if (result.succeeded) apply_rite_effect(w, performer, result);
    return result;
}

}  // namespace sim
