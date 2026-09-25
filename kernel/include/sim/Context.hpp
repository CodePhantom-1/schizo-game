#pragma once
// Context.hpp — THE INTEGRATION SEAM (coordinator-owned; fleet agents read it,
// never edit it). It is the one file allowed to include every module.
//
// THE TICK ORDER IS FIXED (and deterministic — modules read other states as of
// the same morning, before their own tick):
//   economy -> population -> faction -> magic -> justice -> events -> property -> quests
#include "sim/Types.hpp"
#include "sim/Db.hpp"
#include "sim/Rng.hpp"
#include "sim/Time.hpp"

#include "sim/Economy.hpp"
#include "sim/Population.hpp"
#include "sim/Faction.hpp"
#include "sim/Magic.hpp"
#include "sim/Justice.hpp"
#include "sim/Events.hpp"
#include "sim/Property.hpp"
#include "sim/Quests.hpp"
#include "sim/Needs.hpp"
#include "sim/Crafting.hpp"

#include <map>

namespace sim {

struct WorldContext {
    const Db& db;
    Rng& rng;                    // the world's one random source (per-tick: forked by module salt)
    DayNumber day;               // the day being ticked
    const Calendar& cal;
    const WorldFacts& facts;     // drought, war — read-only for everyone

    const EconomyState& economy;
    const PopulationState& population;
    const FactionState& faction;
    const MagicState& magic;
    const JusticeState& justice;
    const EventsState& events;
    const PropertyState& property;
    const QuestState& quests;

    // W2-I: not part of the fixed tick order (the engine owns hours, not
    // days), but exposed here read-only so any module can see an actor's
    // needs/inventory as of this morning if it ever needs to.
    const NeedsState& needs;
    const std::map<Id, Inventory>& inventories;
};

}  // namespace sim
