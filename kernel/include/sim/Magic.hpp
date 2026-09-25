#pragma once
// Magic.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// The magic of the gods: NO mana — a rite succeeds through the five powers
// (rpg-systems §10.1): favour with the specific god, knowledge of the rite,
// materials, purity and place, and time. Deterministic given (ctx, state, inputs).
//
// Success formula (fixed here so every implementer computes the same thing).
// D-022: integer basis points (0..10000), so the result is identical on every
// CPU, and each rite has its own roll on a given day:
//   score_bp = favour * 4000 / 100          (integer division; favour 0..100)
//            + 2000 if the performer has all rite materials
//            + 2000 if purity >= the rite's purity_required (blank = 0)
//            + 1000 if the place is acceptable: performer_place matches the
//                   rite's place requirement, empty requirement = anywhere
//            + 1000 if the time is acceptable: day is a festival day when the
//                   rite demands one, else always
//   roll     = ctx.rng.fork(day).fork(stable_hash(rite_id)).below(10000)
//              (stable_hash = FNV-1a 64 over the id bytes, sim/Rng.hpp;
//               below = unbiased uniform integer in [0, n))
//   success  = roll < score_bp
// RiteResult::score is score_bp / 10000.0, for display only.
// A failed rite still consumes the materials and can anger the deity
// (favour -5). A performed rite raises favour with its deity by +2.
//
// Invariants:
//  - rites come from db/canon/rites.csv (deity, materials, effect_family);
//    a rite missing from the db cannot be performed
//  - the EFFECT is the game (INVENTED: EFFECT); no effect application here —
//    the caller reads RiteResult and applies the outcome
//  - writes ONLY MagicState
#include "sim/Types.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>


namespace sim {

struct WorldContext;  // defined in sim/Context.hpp (the seam — never included by module headers)

struct MagicState {
    std::map<Id, int> favour_by_deity;  // deities.csv id -> 0..100 (50 neutral birth)
    int purity = 100;                   // 0..100, the condition from rpg-systems §1.2
    // performer's current place tag, e.g. "temple:city_of_the_moon", "riverbank"
    std::string place;
    // K-1: the rites this performer has learned (rites.csv ids) — from a
    // teacher or a text (sim/Rites.hpp, rite_teachings.csv). The caller
    // fills RiteInputs::performer_knows_rite from knows_rite(); the fixed
    // formula and perform_rite() are unchanged.
    std::set<Id> known_rites;
};

struct RiteInputs {
    std::map<Id, std::int64_t> materials_held;  // what the performer carries
    bool performer_knows_rite = false;          // learned from text or teacher (caller tracks)
};

struct RiteResult {
    bool performed = false;
    bool succeeded = false;
    Id rite_id;
    Id deity;
    std::string effect_family;   // from rites.csv; empty when refused
    std::string refusal_reason;  // set when performed == false
    double score = 0.0;          // score_bp / 10000.0, for display only (D-022)
    int score_bp = 0;            // D-022: the score in basis points, 0..10000
};

// Raises/lowers favour (clamped 0..100). Keep the free functions so ticks and
// the engine layer share one path.
void add_favour(MagicState& state, const Id& deity, int delta);
int favour(const MagicState& state, const Id& deity);

// K-1 (additive): rite knowledge, the state the RiteInputs flag is read from.
// learn_rite returns false (and changes nothing) when already known or the id
// is empty; it does not check canon — the teaching verbs in sim/Rites.hpp do.
bool knows_rite(const MagicState& state, const Id& rite_id);
bool learn_rite(MagicState& state, const Id& rite_id);

RiteResult perform_rite(const WorldContext& ctx, MagicState& state,
                        const Id& rite_id, const RiteInputs& inputs);

}  // namespace sim
