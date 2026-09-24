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
};

}  // namespace sim
