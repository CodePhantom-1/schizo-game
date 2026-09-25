// Magic.cpp — the five powers of a rite (contract: kernel/contracts/module_Magic.md).
//
// Implements every declaration in sim/Magic.hpp against the FIXED formula of
// Magic.hpp:8-16. Deterministic given (ctx, state, inputs): no wall clock, no
// static mutable state, no threads; the single draw comes from
// ctx.rng.fork(day).unit() exactly as the header prescribes (Magic.hpp:15).
//
// Readings the header forces / leaves to the implementer (each documented here):
//
//  1. Refusals (performed == false, no draw, no favour change, nothing consumed):
//     - the rite row is missing from db/canon/rites.csv, or its `tag` is OPEN
//       ("OPEN rows mark missing canon; they cannot ship" — db/schema/rites.md;
//       "a rite missing from the db cannot be performed" — Magic.hpp:21).
//       Refusal reason: "unknown_rite".
//     - the performer does not know the rite (RiteInputs::performer_knows_rite).
//       The score formula (Magic.hpp:8-15) has no knowledge term, and the parent
//       blueprint makes knowledge a gate, not a weight ("you must know the
//       rite" — rpg-systems §10.1, power 2), so knowledge refuses the attempt.
//       Refusal reason: "rite_not_known". These two reason strings are this
//       module's own vocabulary (not canon data).
//
//  2. Favour deltas (Magic.hpp:17-18), applied to the rite's `deity` column:
//     - a performed rite raises favour by +2 (the header says "performed",
//       its RiteResult distinguishes performed from succeeded, and the fixed
//       formula provides only ONE draw — so the +2 keys on performed==true);
//     - a failed rite additionally lowers favour by 5 ("can anger the deity
//       (favour -5)"; taken as the fixed amount, unconditional — the fixed
//       formula has no second draw from which to roll anger). Net effect:
//       success +2, failure -3.
//
//  3. deity == "any" (3 of the 4 canon rites): "any" is not a deities.csv id,
//     and favour_by_deity is documented as "deities.csv id -> 0..100"
//     (Magic.hpp:37). RiteInputs carries no field naming which god the
//     performer addresses, so a specific god cannot be resolved: the result
//     echoes deity == "any" and NO favour change is applied. (The favour
//     score component still uses the neutral default of 50 below.)
//
//  4. Absent favour entry == 50: the map field is documented "0..100
//     (50 neutral birth)" (Magic.hpp:37), so a deity never touched stands at
//     neutral 50, and add_favour on an absent deity starts from 50.
//
//  5. Materials (score weight 0.20): the rite's `materials` column is a
//     ';'-separated canon prose list ("fish; sea gems"); items.csv is empty
//     (no item ids exist yet), so each entry is normalized mechanically —
//     trimmed, ASCII-lowercased, whitespace runs folded to '_' — into the Id
//     key the caller must hold with a count > 0 in
//     RiteInputs::materials_held. Quantities are not modeled (the column
//     carries none): presence only. All entries required for the 0.20.
//
//  6. Place (score weight 0.10, Magic.hpp:10-12): an empty `place`
//     requirement means anywhere. Otherwise the requirement is split on ';',
//     a trailing "(...)" aside is dropped, and each requirement word matches
//     the performer's place KIND (the text before any ':' of MagicState::place,
//     e.g. "temple:city_of_the_moon" -> "temple") when the words are equal or
//     one is a prefix of the other ("temples" ~ "temple", "shrines" ~
//     "shrine", "diviner's" ~ "diviner"). Word-level matching is the
//     mechanical reading of "matches" for prose requirements like
//     "temples and shrines"; exact string equality would make the component
//     unreachable for every canon row.
//
//  7. Time (score weight 0.10, Magic.hpp:13-14): the rite demands a festival
//     day iff its `time_window` column mentions "festival" (the column is
//     empty in all canon rows today; calendar.csv festival_days is an OPEN
//     row). Otherwise time is acceptable.
//
//  8. Purity requirement (score weight 0.20, Magic.hpp:10): read from the
//     rite's own `purity_required` column (db/schema/rites.md), parsed as an
//     integer defaulting to 0 when the column is empty or unparseable — the
//     header's "default 0" (K-1: purity gating becomes real once a row
//     carries a value; rows with none keep the old non-gating default).
//
//  K-1 additions (kernel/contracts/scenario_rite.md gaps 1/2/5): durable
//  rite knowledge (learn_rite/knows_rite), the material-key normalizer
//  exposed for Actions.cpp (rite_material_keys), and the timed-effect/omen
//  storage (add_ward/add_omen/active_wards/has_active_ward) Actions.cpp
//  writes into after interpreting a RiteResult. None of this decides WHEN an
//  effect fires — Magic.hpp:22-23 still holds — it only owns MagicState.
//
//  9. Consumption of materials is the CALLER's outcome to apply (Magic.hpp:22
//     -23: "no effect application here — the caller reads RiteResult and
//     applies the outcome"; this module writes ONLY MagicState, and
//     RiteInputs::materials_held is const).
#include "sim/Magic.hpp"

#include "sim/Context.hpp"

#include <cstdlib>

namespace sim {
namespace {

constexpr int kNeutralFavour = 50;  // Magic.hpp:37 "50 neutral birth"

char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowered(std::string s) {
    for (char& c : s) c = ascii_lower(c);
    return s;
}

std::string trimmed(const std::string& s) {
    const auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && is_space(s[b])) ++b;
    while (e > b && is_space(s[e - 1])) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> split_on(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string item;
    for (const char c : s) {
        if (c == sep) {
            out.push_back(std::move(item));
            item.clear();
        } else {
            item.push_back(c);
        }
    }
    out.push_back(std::move(item));
    return out;
}

// "sea gems" -> "sea_gems"; whitespace runs fold to a single '_'.
std::string material_key(const std::string& raw) {
    std::string out;
    bool pending_space = false;
    for (const char c : trimmed(raw)) {
        if (c == ' ' || c == '\t') {
            pending_space = !out.empty();
            continue;
        }
        if (pending_space) {
            out.push_back('_');
            pending_space = false;
        }
        out.push_back(ascii_lower(c));
    }
    return out;
}

// "the first great temple (origin myth)" -> "the first great temple".
std::string without_parenthetical(const std::string& raw) {
    const std::string t = trimmed(raw);
    const std::size_t open = t.find('(');
    return trimmed(open == std::string::npos ? t : t.substr(0, open));
}

// Performer place kind: "temple:city_of_the_moon" -> "temple"; "riverbank" -> "riverbank".
std::string place_kind(const std::string& place) {
    const std::string t = trimmed(place);
    const std::size_t colon = t.find(':');
    return lowered(trimmed(colon == std::string::npos ? t : t.substr(0, colon)));
}

bool word_matches_kind(const std::string& word, const std::string& kind) {
    if (word.empty() || kind.empty()) return false;
    return word == kind || word.starts_with(kind) || kind.starts_with(word);
}

// Magic.hpp:10-12 — empty requirement = anywhere; otherwise word-level match.
bool place_acceptable(const std::string& performer_place, const std::string& requirement_raw) {
    if (trimmed(requirement_raw).empty()) return true;  // empty requirement = anywhere
    const std::string kind = place_kind(performer_place);
    if (kind.empty()) return false;
    for (const std::string& entry : split_on(requirement_raw, ';')) {
        const std::string requirement = lowered(without_parenthetical(entry));
        if (requirement.empty()) continue;
        for (const std::string& word : split_on(requirement, ' ')) {
            if (word_matches_kind(word, kind)) return true;
        }
    }
    return false;
}

// All ';'-separated canon materials present with count > 0 in what the performer carries.
bool has_all_materials(const Row& rite, const RiteInputs& inputs) {
    for (const std::string& entry : split_on(rite.get("materials"), ';')) {
        const std::string key = material_key(entry);
        if (key.empty()) continue;
        const auto it = inputs.materials_held.find(key);
        if (it == inputs.materials_held.end() || it->second <= 0) return false;
    }
    return true;
}

// Magic.hpp:13-14 — a festival is demanded iff the rite's time_window says so.
bool time_acceptable(const Row& rite, const WorldContext& ctx) {
    const std::string window = lowered(rite.get("time_window"));
    if (window.find("festival") == std::string::npos) return true;
    return ctx.cal.is_festival(ctx.day);
}

// Magic.hpp:10 — the rite's purity requirement (K-1: rites.csv now carries a
// purity_required column; empty/unparseable keeps the documented default 0,
// same convention Crafting.cpp uses for its own numeric columns).
int purity_requirement(const Row& rite) {
    const std::string v = trimmed(rite.get("purity_required"));
    return v.empty() ? 0 : std::atoi(v.c_str());
}

bool is_a_specific_deity(const Id& deity) {
    return !deity.empty() && deity != "any";
}

}  // namespace

void add_favour(MagicState& state, const Id& deity, int delta) {
    int current = kNeutralFavour;
    const auto it = state.favour_by_deity.find(deity);
    if (it != state.favour_by_deity.end()) current = it->second;
    current += delta;
    if (current < 0) current = 0;
    if (current > 100) current = 100;
    state.favour_by_deity[deity] = current;
}

int favour(const MagicState& state, const Id& deity) {
    const auto it = state.favour_by_deity.find(deity);
    return it == state.favour_by_deity.end() ? kNeutralFavour : it->second;
}

RiteResult perform_rite(const WorldContext& ctx, MagicState& state,
                        const Id& rite_id, const RiteInputs& inputs) {
    RiteResult result;
    result.rite_id = rite_id;

    const std::optional<Row> rite = ctx.db.find("rites", rite_id);
    if (!rite.has_value() || rite->get("tag") == "OPEN") {
        result.refusal_reason = "unknown_rite";  // missing canon (or OPEN: cannot ship)
        return result;
    }
    result.deity = rite->get("deity");

    if (!inputs.performer_knows_rite) {
        result.refusal_reason = "rite_not_known";  // rpg-systems §10.1 power 2: a gate, not a weight
        return result;
    }

    // The five powers, weighted exactly as Magic.hpp:8-15 fixes them.
    const double favour_component =
        0.40 * (static_cast<double>(favour(state, result.deity)) / 100.0);
    const double score =
        favour_component +
        0.20 * (has_all_materials(*rite, inputs) ? 1.0 : 0.0) +
        0.20 * (state.purity >= purity_requirement(*rite) ? 1.0 : 0.0) +
        0.10 * (place_acceptable(state.place, rite->get("place")) ? 1.0 : 0.0) +
        0.10 * (time_acceptable(*rite, ctx) ? 1.0 : 0.0);

    result.score = score;
    result.succeeded = ctx.rng.fork(static_cast<std::uint64_t>(ctx.day)).unit() < score;
    result.performed = true;
    result.effect_family = rite->get("effect_family");

    // Magic.hpp:17-18 — the performed rite raises favour by +2; a failed one
    // additionally angers the deity by -5. No specific god for deity == "any".
    if (is_a_specific_deity(result.deity)) {
        add_favour(state, result.deity, +2);
        if (!result.succeeded) add_favour(state, result.deity, -5);
    }
    return result;
}

// --- K-1: durable knowledge (gap 1) ----------------------------------------

void learn_rite(MagicState& state, const Db& db, const Id& rite_id) {
    const std::optional<Row> row = db.find("rites", rite_id);
    if (!row.has_value() || row->get("tag") == "OPEN") return;  // nothing canon to learn
    state.known_rites.insert(rite_id);
}

bool knows_rite(const MagicState& state, const Id& rite_id) {
    return state.known_rites.find(rite_id) != state.known_rites.end();
}

// --- K-1: the material-key normalizer, exposed for Actions.cpp (gap 2) -----

std::vector<Id> rite_material_keys(const Db& db, const Id& rite_id) {
    std::vector<Id> keys;
    const std::optional<Row> row = db.find("rites", rite_id);
    if (!row.has_value() || row->get("tag") == "OPEN") return keys;
    for (const std::string& entry : split_on(row->get("materials"), ';')) {
        const std::string key = material_key(entry);
        if (!key.empty()) keys.push_back(key);
    }
    return keys;
}

// --- K-1: the timed-effect/omen surface (gap 5) -----------------------------

bool has_active_ward(const MagicState& state, const Id& target, DayNumber day) {
    for (const Ward& w : state.active_wards)
        if (w.target == target && day < w.expires_day) return true;
    return false;
}

std::vector<Ward> active_wards(const MagicState& state, DayNumber day) {
    std::vector<Ward> out;
    for (const Ward& w : state.active_wards)
        if (day < w.expires_day) out.push_back(w);
    return out;
}

Ward& add_ward(MagicState& state, const Id& rite_id, const Id& deity, const Id& target,
               const std::string& kind, DayNumber cast_day, int duration_days) {
    Ward w;
    w.id = "ward_" + std::to_string(state.next_effect_id++);
    w.rite_id = rite_id;
    w.deity = deity;
    w.target = target;
    w.kind = kind;
    w.cast_day = cast_day;
    w.expires_day = cast_day + (duration_days > 0 ? duration_days : 1);
    state.active_wards.push_back(std::move(w));
    return state.active_wards.back();
}

Omen& add_omen(MagicState& state, const Id& rite_id, const Id& deity, DayNumber day,
               const std::string& reading, double probability) {
    Omen o;
    o.id = "omen_" + std::to_string(state.next_effect_id++);
    o.rite_id = rite_id;
    o.deity = deity;
    o.day = day;
    o.reading = reading;
    o.probability = probability;
    state.omens.push_back(std::move(o));
    return state.omens.back();
}

}  // namespace sim
