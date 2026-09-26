#pragma once
// ItemActions.hpp — P0a: the player's hands over the whole WorldState.
// Carrying (FND-03), and (Task 6) goods in the world and in containers with
// ownership and theft (FND-04). Like Actions.hpp, these are caller-layer
// verbs: they may write several modules' state, only through public APIs.
// Integer maths only (D-022). Contract: kernel/contracts/module_Items.md.
#include "sim/Items.hpp"
#include "sim/World.hpp"

namespace sim {

// FND-03 (INVENTED, ledgered in docs/proposals/invented-ledger-items.md).
constexpr int kBaseCarryG = 30000;        // a body carries 30 kg ...
constexpr int kCarryPerStrengthG = 3000;  // ... plus 3 kg per Strength point
constexpr int kSilverGPerShekel = 8;      // silver weighs 8 g a shekel ...
constexpr int kGrainsPerShekel = 180;     // ... of 180 grains (Silver's unit)
constexpr int kBurdenedPct = 100;         // over this: burdened (slow, no sprint)
constexpr int kPinnedPct = 125;           // over this: pinned (cannot move)

// Grams the actor can carry unburdened; an unknown actor counts as Strength 5.
long long capacity_g(const WorldState& w, const Id& actor);
// Grams carried: every stack plus the purse's silver.
long long carried_g(const WorldState& w, const Id& actor);
// 0 normal (<= 100%), 1 burdened (<= 125%), 2 pinned (> 125%).
int encumbrance(const WorldState& w, const Id& actor);

}  // namespace sim
