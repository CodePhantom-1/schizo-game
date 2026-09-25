#pragma once
// World.hpp — CONTRACT (implemented by the coordinator in Wave 2; do not change API).
// WorldState aggregates every module's state and owns the deterministic daily
// tick. This is the save file: serialize the whole struct, restore it, and the
// world continues identically.
//
// DoD scenario (plan v2 §5): ten in-game years headless — prices move with the
// drought, raid chance responds to hunger and defence, a rite fails when
// performed impure, a rumour crosses the city at walking speed.
#include "sim/Context.hpp"
#include "sim/RiteEffects.hpp"
#include "sim/Character.hpp"  // W4-A
// W4-B: combat
#include "sim/Combat.hpp"

namespace sim {

struct WorldState {
    DayNumber day = 1;
    std::uint64_t seed = 1;
    Rng rng{1};
    Calendar cal{};
    WorldFacts facts{};
    Db db{};

    EconomyState economy;
    PopulationState population;
    FactionState faction;
    MagicState magic;
    JusticeState justice;
    EventsState events;
    PropertyState property;
    QuestState quests;

    // W2-I: needs (hunger/thirst/fatigue) and per-actor inventories. Not in
    // the fixed daily tick order — the engine advances needs by the hour
    // through the C API (sim_world_advance_needs), not here.
    NeedsState needs;
    std::map<Id, Inventory> inventories;  // actor id -> Inventory ("player" + npcs)

    // K-1: what successful rites did to the world (wards, omens). Written only
    // by the caller-side applier in sim/Rites.hpp — never by Magic, never by
    // the daily tick.
    RiteEffectsState rite_effects;

    // W4-A: character progression (sim/Character.hpp, sim/Progression.hpp).
    // The catalog is canon, rebuilt by init() (never saved); the player's
    // sheet and the npcs' light sheets are saved (Snapshot's trailing
    // CHAR_* sections). Written only by the caller-side verbs in
    // sim/Progression.hpp — never by a module tick.
    ProgressionCatalog progression;
    CharacterState character;
    std::map<Id, NpcSheet> npc_sheets;  // npc id -> light sheet

    // W4-B: combat — health, stamina, zonal wounds, arms and armour worn,
    // prisoners, duels, deaths. Written by sim/Combat.hpp and its caller layer
    // sim/CombatActions.hpp; healed once a day by advance_days.
    CombatState combat;

    // Loads canon from canon_dir, seeds markets and people, prepares the calendar.
    void init(const std::string& canon_dir, std::uint64_t world_seed);

    // Advances `days` days in fixed tick order (Context.hpp). Fully deterministic.
    void advance_days(int days);

    // The context for the current morning (non-const: it exposes the world's
    // rng so modules can draw). Used by the engine layer too.
    WorldContext context();
};

}  // namespace sim
