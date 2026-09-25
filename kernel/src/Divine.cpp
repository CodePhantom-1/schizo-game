// Divine.cpp — implements sim/Divine.hpp (W5: divine wrath).
//
// A caller-side module in the K-1 shape: state of its own (DivineState) plus
// verbs the hooks in Faction.cpp (break_oath), Rites.cpp (perform_rite_in_
// world) and Justice.cpp (hold_hearing) each call with one line. It writes
// DivineState, and MagicState only through Magic's own add_favour (the Rites
// favour system) — and only on the divine tick, from the pending penalties
// the hooks banked (Faction.cpp and Justice.cpp have no writable favour
// state at their call sites).
//
// Determinism: no wall clock, no static mutable state, no threads, no
// randomness — wrath is pure bookkeeping over the call sequence, and the
// decay counter is an integer day count. Identical calls, identical bytes.
#include "sim/Divine.hpp"

#include "sim/Magic.hpp"
#include "sim/Rites.hpp"  // kRitePerformer: the one performer MagicState models
#include "sim/World.hpp"

#include <algorithm>
#include <optional>

namespace sim {
namespace {

// The one accrual path every offence takes: clamps 0..100, then walks the
// tier ladder crossed — each tier >= 2 banks a favour penalty (the performer
// only: MagicState is one performer's favour, sim/Magic.hpp), tier 3 lays the
// curse. Crossing DOWN never clears a curse (only atonement does).
void accrue(DivineState& s, const Id& offender, const Id& deity, int amount) {
    if (deity.empty() || amount <= 0) return;
    int& v = s.wrath_by_offender[offender][deity];
    const int from_tier = wrath_tier(v);
    v = std::clamp(v + amount, 0, kWrathMax);
    for (int t = from_tier + 1; t <= wrath_tier(v); ++t) {
        if (t >= 2 && offender == kRitePerformer) {
            // The god's displeasure lands on the performer's favour at the
            // next divine tick (the hooks cannot write favour where they run).
            s.pending_favour_penalty[deity] += -kTierFavourPenalty;
        }
        if (t == 3) s.curse_by_offender[offender] = deity;
    }
}

std::string trimmed(const std::string& s) {
    const auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && is_space(s[b])) ++b;
    while (e > b && is_space(s[e - 1])) --e;
    return s.substr(b, e - b);
}

// rites.csv purity_required, parsed exactly as Magic.cpp reads it (reading 8):
// blank or not a plain non-negative integer is the default 0.
int purity_requirement_of(const Row& rite) {
    const std::string cell = trimmed(rite.get("purity_required"));
    if (cell.empty() || cell.size() > 9) return 0;
    int value = 0;
    for (const char c : cell) {
        if (c < '0' || c > '9') return 0;
        value = value * 10 + (c - '0');
    }
    return value;
}

bool is_specific_deity(const Id& deity) { return !deity.empty() && deity != "any"; }

std::optional<Row> live_row(const Db& db, const std::string& table, const Id& id) {
    if (id.empty()) return std::nullopt;
    const std::optional<Row> r = db.find(table, id);
    if (!r || r->get("tag") == "OPEN") return std::nullopt;
    return r;
}

// The city's patron deity: the first deities.csv row (canon row order, i.e.
// the senior god listed first) whose cult_sites names the city — nanna for
// the City of the Moon (the temple of sun and moon), enlil for the City of
// Kings, and so on. "" when no cult is identifiable there.
Id patron_deity_of(const Db& db, const Id& place_city) {
    if (place_city.empty()) return Id{};
    for (const Row& d : db.rows("deities")) {
        if (d.get("tag") == "OPEN") continue;
        if (d.get("cult_sites").find(place_city) != std::string::npos) return d.at("id");
    }
    return Id{};
}

}  // namespace

// --- queries -----------------------------------------------------------------

int wrath(const DivineState& s, const Id& offender, const Id& deity) {
    const auto per = s.wrath_by_offender.find(offender);
    if (per == s.wrath_by_offender.end()) return 0;
    const auto it = per->second.find(deity);
    return it == per->second.end() ? 0 : std::clamp(it->second, 0, kWrathMax);
}

int wrath_tier(int wrath_value) {
    if (wrath_value >= kWrathCurseAt) return 3;
    if (wrath_value >= kWrathDisfavourAt) return 2;
    if (wrath_value >= kWrathIllOmenAt) return 1;
    return 0;
}

bool is_cursed(const DivineState& s, const Id& offender) {
    return s.curse_by_offender.count(offender) != 0;
}

Id curse_deity(const DivineState& s, const Id& offender) {
    const auto it = s.curse_by_offender.find(offender);
    return it == s.curse_by_offender.end() ? Id{} : it->second;
}

int divine_omen_penalty(const WorldState& w, const Id& offender, const Id& deity) {
    return wrath_tier(wrath(w.divine, offender, deity)) * kIllOmenFavourStep;
}

// --- the three designer hooks --------------------------------------------------

void note_oath_break(DivineState& s, const Id& offender) {
    accrue(s, offender, kOathWitnessDeity, kOathBreakWrath);
}

void note_rite_offence(WorldState& w, const Id& rite_id, const Id& addressed, bool succeeded) {
    // The deity the rite addresses: the resolved target (Rites.cpp already
    // validated it as a live deities.csv row), else the row's own deity
    // column. A rite that addresses no god (a ward on a place, a protection
    // rite of deity "any") offends nobody.
    Id deity;
    if (live_row(w.db, "deities", addressed)) {
        deity = addressed;
    } else {
        const std::optional<Row> rite = live_row(w.db, "rites", rite_id);
        if (rite && is_specific_deity(rite->get("deity"))) deity = rite->get("deity");
    }
    if (deity.empty()) return;

    // mechanics.md rows 13/20: purity is load-bearing. Impure is below the
    // rite's own requirement, with kImpureBelow as the floor for rows that
    // set none.
    const std::optional<Row> rite = live_row(w.db, "rites", rite_id);
    const int bar = std::max(rite ? purity_requirement_of(*rite) : 0, kImpureBelow);
    if (w.magic.purity < bar) accrue(w.divine, kRitePerformer, deity, kImpureRiteWrath);
    if (!succeeded) accrue(w.divine, kRitePerformer, deity, kFailedRiteWrath);
}

void note_divine_conviction(DivineState& s, const Db& db, const Id& criminal,
                            const Id& law_row, const Id& crime_kind, const Id& place_city) {
    // The offences against the gods (laws.csv): sacrilege — the temple's own
    // city's patron deity (else the generic ledger, An the father of the
    // gods); tomb_robbery — the underworld queen ("the stones of the dead
    // curse the violator"); oath_breaking — the oath witness. Any other crime
    // offends no god, and neither does an acquittal (the caller checks).
    const Id kind = !law_row.empty() ? law_row : crime_kind;
    Id deity;
    if (kind == "sacrilege") deity = patron_deity_of(db, place_city);
    else if (kind == "tomb_robbery") deity = kTombDeity;
    else if (kind == "oath_breaking") deity = kOathWitnessDeity;
    else return;
    if (deity.empty()) deity = kDivineLedgerDeity;
    accrue(s, criminal, deity, kConvictionWrath);
}

// --- atonement and time ---------------------------------------------------------

std::string perform_atonement(WorldState& w, const Id& deity) {
    DivineState& s = w.divine;
    const auto per = s.wrath_by_offender.find(kRitePerformer);
    if (per == s.wrath_by_offender.end() || per->second.count(deity) == 0) return {};
    int& v = per->second[deity];
    const int before = v;
    v = std::max(0, v - kAtonementWrathDrop);

    // The curse lifts when the wrath that laid it falls below the curse tier.
    const auto curse = s.curse_by_offender.find(kRitePerformer);
    if (curse != s.curse_by_offender.end() && curse->second == deity && v < kWrathCurseAt)
        s.curse_by_offender.erase(curse);

    // The appeased god withdraws his pending anger and recovers some favour.
    s.pending_favour_penalty.erase(deity);
    add_favour(w.magic, deity, kAtonementFavour);
    return "atonement:" + deity + ":" + std::to_string(before) + "->" + std::to_string(v);
}

void break_oath_in_world(WorldState& w, const Id& oath_id) {
    // Faction's own break (unchanged: the drop, the curse flag) plus the gods
    // noticing — one call, both books written.
    break_oath(w.faction, oath_id, &w.divine);
}

void tick_divine(WorldState& w) {
    DivineState& s = w.divine;

    // The displeasure the hooks banked lands on the performer's favour.
    if (!s.pending_favour_penalty.empty()) {
        for (const auto& [deity, delta] : s.pending_favour_penalty) add_favour(w.magic, deity, delta);
        s.pending_favour_penalty.clear();
    }

    // Slow decay: one point per kWrathDecayDays. The cursing god's wrath on a
    // cursed offender does not decay — a curse persists until atonement.
    if (++s.decay_acc >= kWrathDecayDays) {
        s.decay_acc = 0;
        for (auto& [offender, per_deity] : s.wrath_by_offender) {
            const auto curse = s.curse_by_offender.find(offender);
            for (auto& [deity, value] : per_deity) {
                if (curse != s.curse_by_offender.end() && curse->second == deity) continue;
                value = std::max(0, value - 1);
            }
        }
    }
}

}  // namespace sim
