#pragma once
// Rites.hpp — K-1: the magic loop's world verbs (knowledge -> offering ->
// rite -> effect). A WorldState& verb layer, like sim/Actions.hpp: it
// sequences Magic (unchanged — perform_rite still writes ONLY MagicState)
// with the caller-side outcome the Magic contract leaves to the caller:
//
//   1. KNOWLEDGE (rpg-systems §2.1 "by use/teachers/texts"; Magic.hpp
//      RiteInputs: "learned from text or teacher (caller tracks)"): a rite is
//      learned from a teacher whose schedule role, or a text item the reader
//      holds, is listed for it in db/canon/rite_teachings.csv (INVENTED).
//      Knowledge persists on MagicState::known_rites and feeds
//      RiteInputs::performer_knows_rite — still a gate, not a weight.
//   2. OFFERINGS (Magic.hpp:16 "a failed rite still consumes the materials"):
//      materials come from the performer's inventory (WorldState::inventories,
//      the W2-I bag Needs and Crafting already use). One unit of each
//      rites.csv material that is an items.csv item is consumed on every
//      PERFORMED rite, success or failure; a refusal consumes nothing. A
//      material naming no item ("vocal performance", "the deity's true name")
//      is intangible: the performer who knows the rite supplies it.
//   3. EFFECTS (INVENTED: EFFECT — README working rule 4): on success, the
//      rite's effect_family is applied to the world (RiteEffectsState and
//      favour via Magic's own add_favour). Values: see the constants below;
//      ledger: docs/proposals/invented-ledger-rites.md.
//
// Performer: MagicState is one performer's magic (favour, purity, place,
// known rites) — the player (game-design §1). Offerings therefore come from
// inventories["player"].
//
// Determinism: no wall clock, no static mutable state. The only draw beyond
// Magic's own is the omen's, from w.rng.fork(day ^ kOmenSalt) — a fork, so
// the world rng never advances.
#include "sim/World.hpp"

#include <string>
#include <vector>

namespace sim {

// The one performer MagicState models.
inline const Id kRitePerformer = "player";

// INVENTED: EFFECT constants (retunable; ledgered).
constexpr int kOfferingFavour = 5;       // "offering (favour)": extra favour on success
constexpr int kHymnFavour = 3;           // "favour raising": favour with the god addressed
constexpr int kWardDays = 30;            // "protection": a ward holds this many days
constexpr int kOmenTruthPct = 75;        // "divination": how often the sign reads true
constexpr std::uint64_t kOmenSalt = 0x0BA2u;  // barû — the omen's own rng stream

// --- 1. knowledge ------------------------------------------------------------

struct RiteLearnResult {
    bool learned = false;
    // "" when learned; else one of: unknown_rite, already_known,
    // unknown_teacher, not_taught_by_teacher, not_taught_in_text, text_not_held
    std::string refusal_reason;
};

// The teacher is a Population npc; it teaches every rite whose
// rite_teachings row (via=teacher) names its schedule role.
RiteLearnResult learn_rite_from_teacher(WorldState& w, const Id& rite_id, const Id& teacher_npc);

// The reader (the performer) must hold the text item; reading does not
// consume it. Literacy (rpg-systems §2.5) has no kernel state yet: not gated.
RiteLearnResult learn_rite_from_text(WorldState& w, const Id& rite_id, const Id& text_item);

// Non-OPEN rite ids (sorted) the npc can teach / the item teaches.
std::vector<Id> rites_taught_by(const WorldState& w, const Id& teacher_npc);
std::vector<Id> rites_taught_in(const Db& db, const Id& text_item);

// --- 2 + 3. the rite in the world ------------------------------------------

struct RiteOutcome {
    RiteResult rite;                  // Magic's own result (score, performed, succeeded…)
    // "" when performed; Magic's refusal (unknown_rite, rite_not_known) or the
    // applier's own pre-rite refusal (no_deity_addressed, unknown_deity,
    // no_place_to_ward). A refusal changes no state at all.
    std::string refusal_reason;
    Id addressed;                     // the god (or, for protection, the place) the rite was aimed at
    std::vector<Id> consumed;         // item ids debited one unit each
    std::string effect;               // what the success did ("" when nothing applied)
};

// Performs rite_id as the performer, in the world:
//   target — for a deity="any" rite of the offering / favour-raising /
//   divination families: the deities.csv id addressed (required); for a
//   divination rite of a specific god it may name another god to ask about;
//   for protection: the place tag to ward ("" = MagicState::place).
// Sequence: refusals -> RiteInputs from knowledge + inventory -> Magic's
// perform_rite -> consume offerings -> favour deltas for the addressed god
// of a deity="any" rite (Magic.hpp:16-17's +2 / -5, which Magic itself
// cannot apply because RiteInputs names no god) -> on success, the effect.
RiteOutcome perform_rite_in_world(WorldState& w, const Id& rite_id, const Id& target = "");

// True iff a ward holds on `place` on `day`.
bool ward_holds(const RiteEffectsState& s, const std::string& place, DayNumber day);

}  // namespace sim
