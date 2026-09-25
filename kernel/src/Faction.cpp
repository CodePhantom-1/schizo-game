// Faction.cpp — standing (0..100), seating tiers, sworn oaths, the one-Great-King
// rule and the oath-breaker curse flag. Implements every declaration in
// sim/Faction.hpp exactly; ONE documented discrepancy remains and is a
// deliberate OPEN WALL, not an oversight — see swear() below (the header's
// oath-gate text cannot be implemented without an API/header change this
// module is not allowed to make).
//
// Determinism: no wall clock, no static mutable state, no threads, and no
// randomness — the documented faction mechanics are pure bookkeeping, so
// ctx.rng is never touched and identical (state, inputs) yield identical bytes.
//
// Writes ONLY FactionState. Canon is never needed here: swear()/break_oath()
// take no WorldContext, so no db table is consulted and no row is resolved.
#include "sim/Faction.hpp"

#include <algorithm>
#include <utility>

namespace sim {
namespace {

constexpr int kStandingMin = 0;    // Faction.hpp invariant: clamped to [0, 100]
constexpr int kStandingMax = 100;
constexpr int kOathBreakDrop = 40; // Faction.hpp: "drops standing ... by 40"

int clamp_standing(int v) { return std::clamp(v, kStandingMin, kStandingMax); }

}  // namespace

int standing(const FactionState& state, const Id& faction_id) {
    const auto it = state.standing_by_faction.find(faction_id);
    return it == state.standing_by_faction.end() ? 0 : clamp_standing(it->second);
}

void add_standing(FactionState& state, const Id& faction_id, int delta) {
    // Unknown factions start at 0 (the documented default) and the result of
    // the write is clamped — this is the one path every standing write takes.
    int& s = state.standing_by_faction[faction_id];
    s = clamp_standing(s + delta);
}

std::string tier_of(int standing_value) {
    // rpg-systems §4.2: 0-19 Stranger, 20-49 Known, 50-74 Trusted,
    // 75-89 Sworn, 90-100 Oath-bound. Stored standings are always clamped,
    // so the clamp here only makes the function total for raw caller input.
    const int s = clamp_standing(standing_value);
    if (s < 20) return "Stranger";
    if (s < 50) return "Known";
    if (s < 75) return "Trusted";
    if (s < 90) return "Sworn";
    return "Oath-bound";
}

Oath* swear(FactionState& state, const Id& swearer, const Id& to_faction, DayNumber day) {
    // The one-Great-King rule (rpg-systems §4.2), documented in Faction.hpp as
    // "refuses (returns nullptr) if the swearer already holds an unbroken oath
    // to a major faction".
    //
    // OPEN WALL (DECISIONS.md D-010: open walls escalate, never get filled):
    // the frozen signature takes no WorldContext, so the factions.csv `kind`
    // column ("major") cannot be consulted, and hardcoding the major set would
    // be canon invention. The enforced superset is therefore DELIBERATE: the
    // swearer may hold no unbroken oath at all. Every refusal the documented
    // rule requires is contained in this one; only oaths to non-major factions
    // are additionally blocked (e.g. holding sky_cult kind=minor and swearing
    // to the_empire kind=major: the header doc would accept, this refuses —
    // pinned by test_oath_gate_enforced_superset_open_wall so the discrepancy
    // cannot drift silently). Reconciliation before merge is the coordinator's
    // call, and this module may write neither the header nor a new signature:
    // EITHER amend the Faction.hpp oath-gate comment to document this enforced
    // superset, OR extend the API with a seam that can consult db `kind`.
    for (const Oath& o : state.oaths)
        if (o.swearer == swearer && !o.broken) return nullptr;

    Oath oath;
    oath.id = "oath_" + std::to_string(state.oaths.size() + 1);  // "oath_<n>" in sequence
    oath.swearer = swearer;
    oath.to_faction = to_faction;
    oath.day = day;
    oath.broken = false;
    state.oaths.push_back(std::move(oath));
    return &state.oaths.back();
}

void break_oath(FactionState& state, const Id& oath_id) {
    for (Oath& o : state.oaths) {
        if (o.id != oath_id) continue;
        if (o.broken) return;  // already broken: one drop and one curse, per oath
        o.broken = true;
        add_standing(state, o.to_faction, -kOathBreakDrop);  // clamped [0, 100]
        state.oath_breaker_curse = true;  // the curse's bite is Magic's problem
        return;
    }
    // Unknown oath id: nothing to break — absence handled gracefully.
}

void outlaw(FactionState& state, const Id& faction_id) {
    state.standing_by_faction[faction_id] = 0;  // rank 0: no standing left to lose
    state.outlawed_by_faction[faction_id] = true;
}

bool is_outlawed(const FactionState& state, const Id& faction_id) {
    const auto it = state.outlawed_by_faction.find(faction_id);
    return it != state.outlawed_by_faction.end() && it->second;
}

void tick_faction(const WorldContext& ctx, FactionState& state, int days) {
    // The contract documents no per-day faction dynamics (no standing decay,
    // no oath expiry — mechanics.md row 26 keeps rpg-systems §4.2 "unchanged"
    // and adds no clock). The tick therefore maintains the module's invariants
    // only: every recorded standing is clamped back into [0, 100]; oaths and
    // the curse flag are untouched. Identical state in, identical state out.
    // `days` is accepted for the fixed tick signature but drives nothing —
    // FactionState holds no per-day data. ctx is not read: nothing documented
    // to read. (open wall — recorded in the module result)
    (void)ctx;
    (void)days;
    for (auto& entry : state.standing_by_faction)
        entry.second = clamp_standing(entry.second);
}

}  // namespace sim
