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
// W4-C: the wild lands (regions, travel, bandits, raider camps, the raid formula).
#include "sim/Wild.hpp"
// W4-B: combat
#include "sim/Combat.hpp"
// W5: divine wrath (per-offender, per-deity wrath with the gods).
#include "sim/Divine.hpp"
// P0a: goods in the world and in containers (FND-04).
#include "sim/WorldItems.hpp"
// P0a: the player notice feed (FND-09).
#include "sim/Notices.hpp"

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
    // --- W4-C: the wild lands beyond the walls (sim/Wild.hpp). Loaded from
    // canon by init(); ticked by advance_days() after the hearings through
    // tick_wild (sim/WildActions.hpp), which writes other modules' state only
    // through their public APIs. Saved as the trailing WILD snapshot section.
    WildState wild;
    // --- end W4-C

    // W4-B: combat — health, stamina, zonal wounds, arms and armour worn,
    // prisoners, duels, deaths. Written by sim/Combat.hpp and its caller layer
    // sim/CombatActions.hpp; healed once a day by advance_days.
    CombatState combat;

    // --- W5: divine wrath (sim/Divine.hpp). Per-offender, per-deity wrath
    // with the gods, accrued by the one-line hooks in Faction.cpp
    // (break_oath), Rites.cpp (perform_rite_in_world: impure/failed rites and
    // the atonement rite) and Justice.cpp (hold_hearing: divine-offence
    // convictions); decayed and applied once a day by advance_days through
    // tick_divine. Saved as the trailing DIVINE_* snapshot sections.
    DivineState divine;
    // --- end W5

    // --- P0a: goods lying in the world and in containers (sim/WorldItems.hpp),
    // written only by the verbs in sim/ItemActions.hpp. Saved as the
    // optional trailing WORLD_ITEMS_* / CONTAINERS sections.
    WorldItemsState world_items;
    // FND-09: the player notice feed, written by advance_days from what the
    // day's ticks did to the player's world. Saved as the optional NOTICES section.
    NoticeState notices;

    // Loads canon from canon_dir, seeds markets and people, prepares the calendar.
    void init(const std::string& canon_dir, std::uint64_t world_seed);

    // Advances `days` days in fixed tick order (Context.hpp). Fully deterministic.
    void advance_days(int days);

    // The context for the current morning (non-const: it exposes the world's
    // rng so modules can draw). Used by the engine layer too.
    WorldContext context();
};

}  // namespace sim
