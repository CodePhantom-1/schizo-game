#pragma once
// RiteEffects.hpp — K-1: the world state a rite's EFFECT leaves behind
// (state only; the verbs that write it live in sim/Rites.hpp).
//
// Magic.hpp: "the EFFECT is the game (INVENTED: EFFECT); no effect
// application here — the caller reads RiteResult and applies the outcome".
// Magic keeps writing ONLY MagicState; the caller-side applier
// (Rites.cpp::perform_rite_in_world) records what a successful rite did to
// the world here. Every value in this file is INVENTED: EFFECT and is
// ledgered in docs/proposals/invented-ledger-rites.md.
#include "sim/Types.hpp"

#include <map>
#include <string>
#include <vector>

namespace sim {

// The protection family (zisurru_warding, "protecting people and places with
// different types of flour" — notes L198): a ward on a place tag.
struct Ward {
    std::string place;     // a place tag, e.g. "household:player" or "temple:city_of_the_moon"
    Id rite_id;            // which rite laid it
    DayNumber laid = 0;    // day it was laid (or last renewed)
    DayNumber until = 0;   // last day it holds (inclusive)
};

// The divination family (barutu_haruspicy, "omens with probabilities" —
// rpg-systems §10.3; game-design §8.1: omens are never certainties).
struct Omen {
    DayNumber day = 0;
    Id rite_id;
    Id subject;              // the deity asked about
    std::string sign;        // "favourable" | "unfavourable"
    int confidence_pct = 0;  // how often a sign like this reads true (never 100)
};

struct RiteEffectsState {
    std::map<std::string, Ward> wards_by_place;  // place tag -> the ward on it
    std::vector<Omen> omens;                     // every omen read, in order
};

}  // namespace sim
