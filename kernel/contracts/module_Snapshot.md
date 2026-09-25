# MODULE CONTRACT — Snapshot (T1: save/load)

**Status:** T1 agent. Implements World.hpp's "this is the save file" line.

**Owns:** `sim::save_world` / `sim::load_world` only (kernel/include/sim/Snapshot.hpp,
kernel/src/Snapshot.cpp). Reads every module's state; writes none of them except
through `load_world`'s full restore into a caller-owned `WorldState&`.

**May read:** every field of every module state (that's the point of a save).
**Must never:** invent canon, resolve an OPEN row, use wall-clock time, hold
its own Rng (it reads/writes `WorldState::rng`'s state via the two small
accessors added to `Rng.hpp` for this purpose — no other change to that file).

## Format

Plain UTF-8 text, `\n`-terminated lines, tab-separated fields. First line is
the literal version header `SIMSAVE 2`. Every field after it is one of:

- a **scalar line**: `TAG\t<value>`
- a **section**: a count line `TAG\t<n>` followed by exactly `n` data lines

Free-text fields (NPC names, rumour text, steward reports, place tags, quest
stage labels…) are escaped per-field before joining with tabs: `\`→`\\`,
tab→`\t`, `\n`→`\n`, `\r`→`\r` (C-style two-character escapes). This keeps the
file strictly line-oriented and diffable while letting canon-sourced free text
carry literal commas, semicolons, quotes or embedded newlines safely. Splitting
a line on a raw tab byte is therefore always safe — an escaped literal tab
never produces one.

Section order mirrors the fixed tick order in `Context.hpp` (economy →
population → faction → magic → justice → events → property → quests), after
the world scalars (day, seed, rng, drought_stage, war_stage). Within each
section, `std::map` fields iterate in key order (already deterministic) and
`std::vector` fields iterate in insertion order (already deterministic, since
the whole tick is deterministic) — so **identical `WorldState` content always
produces identical bytes**.

The `Db` (canon) and the `Calendar` are **not** in the save. `load_world` calls
`WorldState::init(canon_dir, seed)` first, which reloads canon and rebuilds the
calendar from `seasons.csv` — both are pure functions of canon + seed, never
part of "state" per World.hpp's own db comment. Every module state container is
then `clear()`-ed and refilled from the save, so a restored world has exactly
the saved content, not init's fresh-seeded content plus the save.

## Fields covered (every field of every state struct, per World.hpp)

- WorldState: `day`, `seed`, `rng` (via `Rng::state()`/`set_state()`), `facts`
  (`drought_stage`, `war_stage`). (`db`, `cal` excluded — canon-derived, not state.)
- EconomyState: `market_by_city` (nested `PriceBook::silver_by_item`), `stock_by_city_item`.
- PopulationState: `npcs` (every `Npc` field incl. nested `memory` vector), `knows`.
- FactionState: `standing_by_faction`, `oaths` (every `Oath` field), `oath_breaker_curse`.
- MagicState: `favour_by_deity`, `purity`, `place`.
- JusticeState: `open_crimes` (every `Crime` field), `verdicts` (every `Hearing` field), `next_id`.
- EventsState: `rules` (every `EventRule` field incl. variable `triggers`), `fired`, `fire_count_by_rule`.
- PropertyState: `assets`, `loans`, `purse_by_owner`, `steward_reports`, `next_id`.
- QuestState: `defs`, `active`, `completed`, `failed_list`.
- NeedsState (W2-I, additive section `NEEDS` after `QUESTS_*`): `by_actor` (hunger/thirst/fatigue per actor).
- `WorldState::inventories` (W2-I, additive section `INVENTORIES` after `NEEDS`): actor id -> `Inventory::counts`.
- K-1 (trailing, optional on load — a pre-K-1 save that ends after `INVENTORIES` still loads):
  `MAGIC_KNOWN` (`MagicState::known_rites`), `RITE_WARDS` (`WorldState::rite_effects.wards_by_place`:
  place, rite, laid, until), `RITE_OMENS` (`rite_effects.omens`: day, rite, subject, sign, confidence_pct).

## Errors

`load_world` throws `std::runtime_error` for: a missing/wrong version header,
a truncated file (unexpected end of data), a section/scalar tag mismatch, a
row with the wrong field count, or an unparsable integer/bool. Any other
exception surfacing during parse (e.g. from `std::stoll`) is caught and
re-thrown as `std::runtime_error` so callers only ever need to catch one type.

## INVENTED

- The save format itself (tab-separated, escaped, versioned text) — the
  contract only requires "deterministic, versioned, human-diffable"; the exact
  shape is this module's invention.
- The `Rng::state()`/`set_state()` accessors added to `Rng.hpp` (the header
  says a module may add a minimal state getter/setter if unreachable
  otherwise; `state_` was private with no accessor).

**Definition of done:** src/Snapshot.cpp implements every declaration in
include/sim/Snapshot.hpp; tests/test_snapshot.cpp passes; save/load is
byte-deterministic and a restored world advances identically to its source.

## W2-I update (atomicity)

`load_world` parses into a local `WorldState`, moved into the caller's
reference only once every section has parsed without throwing. A
truncated/malformed save now throws with the caller's live world left
byte-for-byte untouched (see `test_load_world_is_atomic_on_failure`).

## W4-C update (the wild lands)

One more trailing, optional section: `WILD\t<n>`, then n rows produced by `wild_save_rows` (sim/Wild.hpp; row kinds S/G/C/V/R/E/F/T) and restored by `wild_load_rows`. `Reader::peek_tag()` reads the next tag without consuming it, so the section is found in any order among the trailing sections. It is absent in older saves, where the fresh `init_wild` state stands. Static wild tables are canon and are never saved.

## W4-B update (combat)

The combat sections follow the K-1, W4-A (CHAR_*) and W4-C (WILD) sections: `COMBAT_ACTORS` with its armour, wound, lasting-effect and durability sub-rows, then `COMBAT_HOSTILITY`, `COMBAT_DUELS`, `COMBAT_PRISONERS`, `COMBAT_DEATHS` and `COMBAT_SEQ`. They are optional on load. `Reader::peek_tag()` recognises them by tag, so they load correctly whatever other optional sections precede them. A save without them loads with an empty `CombatState`. See `tests/test_scenario_duel.cpp` `test_saves_before_and_after_combat`.
