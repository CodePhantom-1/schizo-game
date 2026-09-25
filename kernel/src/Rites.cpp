// Rites.cpp — implements sim/Rites.hpp (K-1: knowledge, offerings, effects).
//
// Like Actions.cpp, a WorldState& verb file allowed to write more than one
// module's state: MagicState (only through Magic's own free functions —
// learn_rite, perform_rite, add_favour), WorldState::inventories (offerings)
// and WorldState::rite_effects (wards, omens). Magic.cpp itself still writes
// ONLY MagicState, and the fixed success formula is untouched.
#include "sim/Rites.hpp"

#include "sim/Progression.hpp"  // W4-A: the Sacred skills grow by performing

#include <algorithm>
#include <optional>

namespace sim {
namespace {

std::string trim(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// Magic.cpp's material normalization (reading 5), repeated here so the
// offering keys are exactly the keys perform_rite checks: trimmed,
// ASCII-lowercased, whitespace runs folded to '_'.
std::string material_key(const std::string& raw) {
    std::string out;
    bool pending_space = false;
    for (const char c : trim(raw)) {
        if (c == ' ' || c == '\t') {
            pending_space = !out.empty();
            continue;
        }
        if (pending_space) {
            out.push_back('_');
            pending_space = false;
        }
        out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

std::vector<std::string> material_keys(const Row& rite) {
    std::vector<std::string> keys;
    std::string item;
    const std::string raw = rite.get("materials") + ";";
    for (const char c : raw) {
        if (c == ';') {
            const std::string k = material_key(item);
            if (!k.empty()) keys.push_back(k);
            item.clear();
        } else {
            item.push_back(c);
        }
    }
    return keys;
}

std::optional<Row> live_row(const Db& db, const std::string& table, const Id& id) {
    if (id.empty()) return std::nullopt;
    std::optional<Row> r = db.find(table, id);
    if (!r || r->get("tag") == "OPEN") return std::nullopt;
    return r;
}

// A material is an offering to debit iff it names a live items.csv row.
bool is_item(const Db& db, const std::string& key) { return live_row(db, "items", key).has_value(); }

bool is_specific_deity(const Id& deity) { return !deity.empty() && deity != "any"; }

// The eight effect families (rpg-systems §10; game-design §8.1) as the canon
// rites.csv effect_family prose names them today. A family with no applier
// yet maps to "" and applies nothing.
std::string family_of(const std::string& effect_family) {
    const std::string f = lower(trim(effect_family));
    if (f.rfind("offering", 0) == 0) return "offering";
    if (f.rfind("favour", 0) == 0) return "favour_raising";
    if (f.rfind("protection", 0) == 0) return "protection";
    if (f.rfind("divination", 0) == 0) return "divination";
    return "";
}

bool teaching_matches(const Row& t, const Id& rite_id, const std::string& via) {
    return t.get("tag") != "OPEN" && t.get("rite_id") == rite_id && lower(trim(t.get("via"))) == via;
}

int held(const WorldState& w, const Id& item) {
    const auto inv = w.inventories.find(kRitePerformer);
    if (inv == w.inventories.end()) return 0;
    const auto it = inv->second.counts.find(item);
    return it == inv->second.counts.end() ? 0 : it->second;
}

}  // namespace

// --- knowledge -------------------------------------------------------------

RiteLearnResult learn_rite_from_teacher(WorldState& w, const Id& rite_id, const Id& teacher_npc) {
    RiteLearnResult r;
    if (!live_row(w.db, "rites", rite_id)) { r.refusal_reason = "unknown_rite"; return r; }
    if (knows_rite(w.magic, rite_id)) { r.refusal_reason = "already_known"; return r; }
    const Npc* npc = find_npc(w.population, teacher_npc);
    if (npc == nullptr) { r.refusal_reason = "unknown_teacher"; return r; }
    const std::vector<Id> taught = rites_taught_by(w, teacher_npc);
    if (std::find(taught.begin(), taught.end(), rite_id) == taught.end()) {
        r.refusal_reason = "not_taught_by_teacher";
        return r;
    }
    r.learned = learn_rite(w.magic, rite_id);
    return r;
}

RiteLearnResult learn_rite_from_text(WorldState& w, const Id& rite_id, const Id& text_item) {
    RiteLearnResult r;
    if (!live_row(w.db, "rites", rite_id)) { r.refusal_reason = "unknown_rite"; return r; }
    if (knows_rite(w.magic, rite_id)) { r.refusal_reason = "already_known"; return r; }
    const std::vector<Id> taught = rites_taught_in(w.db, text_item);
    if (std::find(taught.begin(), taught.end(), rite_id) == taught.end()) {
        r.refusal_reason = "not_taught_in_text";
        return r;
    }
    if (held(w, text_item) <= 0) { r.refusal_reason = "text_not_held"; return r; }
    r.learned = learn_rite(w.magic, rite_id);  // the tablet is read, not consumed
    return r;
}

std::vector<Id> rites_taught_by(const WorldState& w, const Id& teacher_npc) {
    std::vector<Id> out;
    const Npc* npc = find_npc(w.population, teacher_npc);
    if (npc == nullptr || npc->role.empty()) return out;
    const std::string role = lower(trim(npc->role));
    for (const Row& t : w.db.rows("rite_teachings")) {
        const Id rite = t.get("rite_id");
        if (!teaching_matches(t, rite, "teacher")) continue;
        if (lower(trim(t.get("source"))) != role) continue;
        if (!live_row(w.db, "rites", rite)) continue;
        out.push_back(rite);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::vector<Id> rites_taught_in(const Db& db, const Id& text_item) {
    std::vector<Id> out;
    if (text_item.empty()) return out;
    for (const Row& t : db.rows("rite_teachings")) {
        const Id rite = t.get("rite_id");
        if (!teaching_matches(t, rite, "text")) continue;
        if (trim(t.get("source")) != text_item) continue;
        if (!live_row(db, "rites", rite)) continue;
        out.push_back(rite);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// --- the rite in the world ---------------------------------------------------

RiteOutcome perform_rite_in_world(WorldState& w, const Id& rite_id, const Id& target) {
    RiteOutcome out;
    const std::optional<Row> rite = live_row(w.db, "rites", rite_id);
    const bool known = knows_rite(w.magic, rite_id);

    // Magic's own refusals first, so their reasons stay Magic's vocabulary.
    if (!rite || !known) {
        RiteInputs none;
        none.performer_knows_rite = known;
        out.rite = perform_rite(w.context(), w.magic, rite_id, none);  // refuses: no draw, no change
        out.refusal_reason = out.rite.refusal_reason;
        return out;
    }

    // Resolve what the rite is aimed at before anything is spent.
    const Id deity = rite->get("deity");
    const std::string family = family_of(rite->get("effect_family"));
    if (family == "protection") {
        out.addressed = target.empty() ? w.magic.place : target;
        if (trim(out.addressed).empty()) { out.refusal_reason = "no_place_to_ward"; return out; }
    } else if (family == "offering" || family == "favour_raising" || family == "divination") {
        // A named target overrides only an "any" rite, or picks the god a
        // divination asks about; an offering to a named god stays his.
        const bool may_retarget = !is_specific_deity(deity) || family == "divination";
        out.addressed = (may_retarget && !target.empty()) ? target : deity;
        if (!is_specific_deity(out.addressed)) { out.refusal_reason = "no_deity_addressed"; return out; }
        if (!live_row(w.db, "deities", out.addressed)) { out.refusal_reason = "unknown_deity"; return out; }
    }

    // The offering: what the performer carries, plus the intangibles the
    // knowing performer supplies himself.
    RiteInputs inputs;
    inputs.performer_knows_rite = true;
    const std::vector<std::string> keys = material_keys(*rite);
    for (const std::string& k : keys)
        inputs.materials_held[k] = is_item(w.db, k) ? held(w, k) : 1;

    out.rite = perform_rite(w.context(), w.magic, rite_id, inputs);
    if (!out.rite.performed) {  // defensive: every Magic refusal was pre-checked above
        out.refusal_reason = out.rite.refusal_reason;
        return out;
    }

    // Magic.hpp:16 — performed, success or failure, the offerings are spent.
    Inventory& inv = w.inventories[kRitePerformer];
    for (const std::string& k : keys) {
        if (!is_item(w.db, k)) continue;
        auto it = inv.counts.find(k);
        if (it == inv.counts.end() || it->second <= 0) continue;
        --it->second;
        out.consumed.push_back(k);
    }

    // Magic.hpp:16-17's favour deltas for the god a deity="any" rite
    // addressed (Magic.cpp reading 3 cannot resolve him; the caller can).
    const bool any_rite_with_god = !is_specific_deity(deity) && family != "protection" &&
                                   is_specific_deity(out.addressed);
    if (any_rite_with_god) {
        add_favour(w.magic, out.addressed, +2);
        if (!out.rite.succeeded) add_favour(w.magic, out.addressed, -5);
    }

    // W4-A: a performed rite, success or failure, is use of its tradition's
    // Sacred skill (skills.csv grows_by rite:<tradition>; mechanics.md row 21).
    (void)note_rite(w, rite_id, out.rite.succeeded);

    if (!out.rite.succeeded) return out;  // a failed rite does nothing to the world

    // INVENTED: EFFECT — the effect families.
    if (family == "offering") {
        // W4-A: rite_favour_bp talents/perks add to the favour (integer bp).
        const int gain = kOfferingFavour + kOfferingFavour * w.character.derived.rite_favour_bp / kBp;
        add_favour(w.magic, out.addressed, gain);
        out.effect = "favour:" + out.addressed + ":+" + std::to_string(gain);
    } else if (family == "favour_raising") {
        const int gain = kHymnFavour + kHymnFavour * w.character.derived.rite_favour_bp / kBp;  // W4-A
        add_favour(w.magic, out.addressed, gain);
        out.effect = "favour:" + out.addressed + ":+" + std::to_string(gain);
    } else if (family == "protection") {
        Ward& ward = w.rite_effects.wards_by_place[out.addressed];
        const DayNumber until = w.day + kWardDays - 1;
        ward.place = out.addressed;
        ward.rite_id = rite_id;
        ward.laid = w.day;
        ward.until = std::max(ward.until, until);
        out.effect = "ward:" + out.addressed + ":until=" + std::to_string(ward.until);
    } else if (family == "divination") {
        // The question the liver answers: does the god look on the asker
        // with favour (favour >= the neutral 50)? The sign reads true only
        // kOmenTruthPct of the time — never a certainty.
        const bool truth = favour(w.magic, out.addressed) >= 50;
        const double draw =
            w.rng.fork(static_cast<std::uint64_t>(w.day) ^ kOmenSalt).unit();
        const bool reads_true = draw < static_cast<double>(kOmenTruthPct) / 100.0;
        Omen omen;
        omen.day = w.day;
        omen.rite_id = rite_id;
        omen.subject = out.addressed;
        omen.sign = (truth == reads_true) ? "favourable" : "unfavourable";
        omen.confidence_pct = kOmenTruthPct;
        w.rite_effects.omens.push_back(omen);
        out.effect = "omen:" + omen.subject + ":" + omen.sign + ":" + std::to_string(kOmenTruthPct);
    }
    return out;
}

bool ward_holds(const RiteEffectsState& s, const std::string& place, DayNumber day) {
    const auto it = s.wards_by_place.find(place);
    return it != s.wards_by_place.end() && day >= it->second.laid && day <= it->second.until;
}

}  // namespace sim
