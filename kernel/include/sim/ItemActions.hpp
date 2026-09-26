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

// --- FND-04: goods in the world and in containers ----------------------------
// Codes and reason keys: kernel/contracts/verb_results.md.
struct ItemResult {
    int code = 0;          // >= 0 units moved (add_container: 0); < 0 a refusal
    std::string reason;    // "" or the refusal key
    Id id;                 // the world item created (drop_item)
    bool theft = false;    // the move took someone else's goods
};

// The ownership rule shared by pick_up and take_out: the rightful owner is the
// stack's owner, else the container's. Taking goods whose rightful owner is
// someone else flags them stolen (keeping that owner) and, if anyone
// witnessed it, files a theft (Actions::commit_crime) in the place's city.
// Taking your own goods clears both marks.

// Drops up to qty units of the actor's stack (by index) at a place. The goods
// stay the dropper's (owner set to the actor unless already someone else's).
ItemResult drop_item(WorldState& w, const Id& actor, std::size_t stack_index, int qty,
                     const Id& place, int x_cm, int y_cm, int z_cm);
// Picks up to qty units of a world item, never past the 125% carrying line.
ItemResult pick_up(WorldState& w, const Id& actor, const Id& world_item, int qty,
                   const std::vector<Id>& witnesses);
ItemResult add_container(WorldState& w, const Id& id, const Id& place, const std::string& kind,
                         const Id& owner, int capacity_g);
// Puts up to qty units of the actor's stack into a container, as many as fit.
// Goods put in a container that is not the actor's stay the actor's.
ItemResult put_in(WorldState& w, const Id& actor, const Id& container, std::size_t stack_index, int qty);
// Takes up to qty units of a container's stack (by index), never past 125%.
ItemResult take_out(WorldState& w, const Id& actor, const Id& container, std::size_t stack_index,
                    int qty, const std::vector<Id>& witnesses);
// The world items lying at a place, in order of creation.
std::vector<const WorldItem*> world_items_at(const WorldState& w, const Id& place);
// Units of the stack's item the actor can still take without passing 125%.
int units_that_fit(const WorldState& w, const Id& actor, const ItemStack& s);

}  // namespace sim
