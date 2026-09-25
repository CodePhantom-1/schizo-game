#pragma once
// Magic.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// The magic of the gods: NO mana — a rite succeeds through the five powers
// (rpg-systems §10.1): favour with the specific god, knowledge of the rite,
// materials, purity and place, and time. Deterministic given (ctx, state, inputs).
//
// Success formula (fixed here so every implementer computes the same thing):
//   score = 0.40 * (favour/100)
//         + 0.20 * (has all rite materials)
//         + 0.20 * (purity >= rite's purity requirement, default 0)
//         + 0.10 * (place is acceptable: performer_place matches the rite's
//                   place requirement, empty requirement = anywhere)
//         + 0.10 * (time is acceptable: day is a festival day when the rite
//                   demands one, else true)
//   success = ctx.rng.fork(day).unit() < score
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
class Db;              // sim/Db.hpp — read-only canon access (forward declared, as Needs.hpp does)

// K-1 gap 5: a timed effect other systems can query (protection wards, and
// the "blessing"/"curse and binding" families reuse the same shape — an
// effect that lives on a target until a day). Actions.cpp decides WHEN one
// of these is created (Magic.hpp:22-23 still holds: no effect application
// in this module); MagicState just owns the record once Actions writes it.
struct Ward {
    Id id;
    Id rite_id;
    Id deity;      // "any" when the rite names no specific god
    Id target;      // actor id or place tag the ward covers
    std::string kind;  // "protection" | "blessing" | "curse" (rpg-systems §10.3 families)
    DayNumber cast_day = 0;
    DayNumber expires_day = 0;  // active while day < expires_day
};

// K-1 gap 5: the divination family's "world-facing omen record with
// probabilities" (rpg-systems §10.3: "omens with probabilities, not
// certainties") that events/quests can read.
struct Omen {
    Id id;
    Id rite_id;
    Id deity;
    DayNumber day = 0;
    std::string reading;      // INVENTED: EFFECT — e.g. "favourable"/"ill-favoured"/"uncertain"
    double probability = 0.0;  // the rite's score at the moment of the reading
};

struct MagicState {
    std::map<Id, int> favour_by_deity;  // deities.csv id -> 0..100 (50 neutral birth)
    int purity = 100;                   // 0..100, the condition from rpg-systems §1.2
    // performer's current place tag, e.g. "temple:city_of_the_moon", "riverbank"
    std::string place;

    // K-1 gap 1: durable rite knowledge. MagicState already models a single
    // implicit performer (favour/purity/place carry no actor key), so this
    // follows the same shape rather than inventing a per-actor map.
    std::set<Id> known_rites;

    // K-1 gap 5: the effect surfaces Actions.cpp writes into after a
    // successful rite (protection/blessing/curse wards, divination omens).
    std::vector<Ward> active_wards;
    std::vector<Omen> omens;
    int next_effect_id = 1;  // deterministic Ward/Omen id counter (no wall clock, no rng)
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
    double score = 0.0;
};

// Raises/lowers favour (clamped 0..100). Keep the free functions so ticks and
// the engine layer share one path.
void add_favour(MagicState& state, const Id& deity, int delta);
int favour(const MagicState& state, const Id& deity);

RiteResult perform_rite(const WorldContext& ctx, MagicState& state,
                        const Id& rite_id, const RiteInputs& inputs);

// K-1 gap 1: "learned from a teacher/text/temple" becomes durable state —
// RiteInputs::performer_knows_rite (still "caller tracks" per call) should
// be filled from knows_rite() rather than re-derived from nothing every
// attempt. A no-op for an unknown/OPEN rite id (canon_lint.py: OPEN rows
// cannot ship, so there is nothing to learn).
void learn_rite(MagicState& state, const Db& db, const Id& rite_id);
bool knows_rite(const MagicState& state, const Id& rite_id);

// The rite's `materials` column (rites.csv), normalized to the same Id keys
// Magic.cpp's has_all_materials() checks (trimmed, lowercased, spaces ->
// '_', split on ';'). Exposed so a caller (Actions.cpp) can build
// RiteInputs::materials_held and consume the same keys from an inventory
// without re-implementing the normalization. Empty for an unknown/OPEN rite.
std::vector<Id> rite_material_keys(const Db& db, const Id& rite_id);

// K-1 gap 5: query helpers for the timed-effect surface (protection/
// blessing/curse). `day` filters to effects still active (day < expires_day).
bool has_active_ward(const MagicState& state, const Id& target, DayNumber day);
std::vector<Ward> active_wards(const MagicState& state, DayNumber day);

// Appends a new Ward/Omen with a deterministic id ("ward_<n>"/"omen_<n>",
// MagicState::next_effect_id) and returns a pointer into the state's own
// vector (invalidated by the next add_* call, same convention as
// std::vector::push_back). Actions.cpp calls these; this module only owns
// the storage (Magic.hpp:22-23: it does not decide when an effect fires).
Ward& add_ward(MagicState& state, const Id& rite_id, const Id& deity, const Id& target,
               const std::string& kind, DayNumber cast_day, int duration_days);
Omen& add_omen(MagicState& state, const Id& rite_id, const Id& deity, DayNumber day,
               const std::string& reading, double probability);

}  // namespace sim
