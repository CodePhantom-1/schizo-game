// CApi.h — the C ABI over the world kernel (coordinator-owned contract).
//
// WHY A C API: SimRuntime (UE5) links this kernel into a build system with its
// own flags. A C boundary means no STL/ABI coupling across toolchains — the
// engine passes strings and integers, the kernel does the rest.
//
// Ownership: SimWorld is owned by the caller (create → use → destroy).
// Strings: outputs are written into caller buffers (cap-limited); inputs are
// borrowed, never stored.
// Exceptions: none ever cross this boundary. On an internal failure (e.g.
// out of memory) a function returns its error value (-1, or nullptr for the
// constructors); void functions become a no-op for the failed step.
// Canon: create/load return nullptr when canon_dir holds no seasons.csv.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimWorld SimWorld;  // opaque — one deterministic world

// Lifecycle ---------------------------------------------------------------
SimWorld* sim_world_create(const char* canon_dir, uint64_t seed);
// Frees the world. A null world is a no-op; never throws.
void sim_world_destroy(SimWorld* world);

// Clock -------------------------------------------------------------------
void sim_world_advance_days(SimWorld* world, int days);
int64_t sim_world_day(const SimWorld* world);
// Season id for today ("rains"/"sowing"/"harvest"/"vintage"). Writes at most
// cap-1 bytes plus NUL; returns the untruncated length, or -1 on error.
int sim_world_season(const SimWorld* world, char* out, int cap);

// World facts (act content, coordinator-owned scalars) ---------------------
void sim_world_set_drought(SimWorld* world, int stage);
int sim_world_drought(const SimWorld* world);
void sim_world_set_war(SimWorld* world, int stage);
int sim_world_war(const SimWorld* world);

// Economy ------------------------------------------------------------------
int64_t sim_world_price(const SimWorld* world, const char* city, const char* item);

// Faction standing (0..100) ------------------------------------------------
int sim_world_standing(const SimWorld* world, const char* faction);
void sim_world_add_standing(SimWorld* world, const char* faction, int delta);

// The gods -----------------------------------------------------------------
int sim_world_favour(const SimWorld* world, const char* deity);
void sim_world_add_favour(SimWorld* world, const char* deity, int delta);

// Counters the engine UI reads ---------------------------------------------
int sim_world_verdict_count(const SimWorld* world);
int sim_world_event_count(const SimWorld* world);
int sim_world_npc_count(const SimWorld* world);

// Save / load ---------------------------------------------------------------
// Writes the world's save (Snapshot.hpp SIMSAVE format) to `path`.
// Returns 0 on success; -1 on a null argument; -2 if the file could not be
// opened for writing.
int sim_world_save(const SimWorld* world, const char* path);

// Loads a world previously saved with sim_world_save. `canon_dir` is the
// canon directory to rebuild the db/calendar from (same conventions as
// sim_world_create). Returns a new SimWorld the caller owns, or nullptr on
// a null argument, a file that could not be opened, or a malformed save.
SimWorld* sim_world_load(const char* canon_dir, const char* path);

// Buffer variants (cheap: the save is already an in-memory string).
// Writes at most cap-1 bytes plus NUL; returns the untruncated length, or
// -1 on a null argument. (out = nullptr, cap = 0) asks for the length only.
int sim_world_save_to_buffer(const SimWorld* world, char* out, int cap);
// `data` is the NUL-terminated save text (as written by save_to_buffer or
// read from a sim_world_save file). Returns a new SimWorld, or nullptr on a
// null argument or a malformed save.
SimWorld* sim_world_load_from_buffer(const char* canon_dir, const char* data);

// Needs (hunger/thirst/fatigue, 0..100) --------------------------------------
// Each getter returns -1 for a null world/actor (an actor never seen yet
// reads as 0/0/0, matching Needs.hpp's needs_of()).
int sim_world_hunger(const SimWorld* world, const char* actor);
int sim_world_thirst(const SimWorld* world, const char* actor);
int sim_world_fatigue(const SimWorld* world, const char* actor);

// Advances one actor's needs by `hours` game-hours (the engine owns the
// sub-day clock; the day tick itself stays daily). `sleeping` is 0 or 1.
void sim_world_advance_needs(SimWorld* world, const char* actor, int hours, int sleeping);

// Eats/drinks one unit of `item` from the actor's own inventory (given/held
// via sim_world_give_item). "water" is always drinkable for
// sim_world_drink without touching the inventory (Needs.hpp's virtual
// always-available id); every other item, for both eat and drink, is
// consumed from inventory on success.
// Return codes (both functions):
//   0   ok — hunger/thirst applied and (unless "water") 1 unit consumed
//  -1   null world/actor/item
//  -2   the actor's inventory holds none of `item` (and item != "water")
//  -3   item is unknown, or not edible/drinkable (Needs.cpp categories)
int sim_world_eat(SimWorld* world, const char* actor, const char* item);
int sim_world_drink(SimWorld* world, const char* actor, const char* item);

// Named condition effects at the actor's current thresholds
// (need_effects(), semicolon-joined, e.g. "hungry;parched"). Writes at most
// cap-1 bytes plus NUL; returns the untruncated length, or -1 on a null
// argument.
int sim_world_need_effects(const SimWorld* world, const char* actor, char* out, int cap);

// Inventory ------------------------------------------------------------------
// Count of `item` the actor holds, or -1 on a null argument.
int sim_world_item_count(const SimWorld* world, const char* actor, const char* item);

// Adds `qty` (may be negative) to the actor's count of `item`; the count
// never drops below 0. Returns the resulting (clamped) count, or -1 on a
// null argument.
int sim_world_give_item(SimWorld* world, const char* actor, const char* item, int qty);

// Crafting --------------------------------------------------------------------
// Crafts `recipe` `times` times from the actor's own inventory, consuming
// its own inventory's items in place (Crafting.hpp: all-or-nothing).
// `stations_semicolon_list` is e.g. "quern;oven" (a null or empty string
// means no stations at hand).
// Return codes:
//   0   ok
//  -1   null world/actor/recipe, or times <= 0
//  -2   unknown or OPEN recipe id
//  -3   required station not in the list
//  -4   insufficient inputs in the actor's inventory
int sim_world_craft(SimWorld* world, const char* actor, const char* recipe,
                     const char* stations_semicolon_list, int times);

// Schedule --------------------------------------------------------------------
// The task in force for `role` at `hour` (0..23) on the current day.
// Writes at most cap-1 bytes plus NUL; returns the untruncated length, or
// -1 on a null argument or no schedule rows at all for that role.
int sim_world_task_at(const SimWorld* world, const char* role, int hour, char* out, int cap);

// Rites (K-1: knowledge -> offerings -> effects; sim/Rites.hpp) -------------
// The performer is the player: MagicState is one performer's magic, and
// offerings are debited from the "player" inventory (sim_world_give_item).

// 1 if the player knows `rite`, 0 if not, -1 on a null argument.
int sim_world_knows_rite(const SimWorld* world, const char* rite);
// Known rite ids, sorted, semicolon-joined. Buffer convention (writes at most
// cap-1 bytes plus NUL; returns the untruncated length, -1 on null).
int sim_world_known_rites(const SimWorld* world, char* out, int cap);

// Learns `rite` from the npc `teacher` (a Population npc whose schedule role
// rite_teachings.csv lists for it) / from the text item `item` held in the
// player's inventory (read, not consumed).
// Return codes (both functions):
//   0   learned
//   1   already known (nothing changes)
//  -1   null argument
//  -2   unknown or OPEN rite
//  -3   unknown teacher npc                        (teacher variant only)
//  -4   this teacher / text does not teach the rite
//  -5   the text is not in the player's inventory  (text variant only)
int sim_world_learn_rite_from_teacher(SimWorld* world, const char* rite, const char* teacher);
int sim_world_learn_rite_from_text(SimWorld* world, const char* rite, const char* item);

// Rite ids the npc teaches / the text item teaches, sorted, semicolon-joined
// (empty for an unknown npc/item). Buffer convention; -1 on null.
int sim_world_rites_taught_by(const SimWorld* world, const char* teacher, char* out, int cap);
int sim_world_rites_taught_in(const SimWorld* world, const char* item, char* out, int cap);

// The performer's place tag ("temple:city_of_the_moon", "household:player"...)
// the rite's place requirement is matched against, and his purity (0..100,
// set clamps). Getters: buffer convention / -1 on null.
void sim_world_set_rite_place(SimWorld* world, const char* place);
int sim_world_rite_place(const SimWorld* world, char* out, int cap);
int sim_world_purity(const SimWorld* world);
void sim_world_set_purity(SimWorld* world, int purity);

// Performs `rite` in the world (Rites.hpp perform_rite_in_world). `target`
// (may be null/empty): the deities.csv id a deity="any" rite addresses, or
// asks about (divination); the place tag a protection rite wards ("" = the
// rite place). `effect_out` (may be null) receives what the success did, e.g.
// "favour:the_two_waters:+5", "ward:household:player:until=35",
// "omen:inanna:favourable:75" -- empty on failure or refusal.
// Return codes:
//   1   performed and succeeded (effect applied; offerings consumed)
//   0   performed and failed (offerings still consumed; favour may fall)
//  -1   null world/rite
//  -2   unknown or OPEN rite
//  -3   the player does not know the rite
//  -4   nothing addressed (an "any" rite with no god, a ward with no place)
//  -5   target names no live deities.csv row
// A negative code changes no state, except a -1 from an internal failure
// (e.g. out of memory), which may leave the rite partly applied.
int sim_world_perform_rite(SimWorld* world, const char* rite, const char* target,
                           char* effect_out, int cap);

// Last day (inclusive) a ward holds on `place`; 0 if none was ever laid; -1 null.
int64_t sim_world_ward_until(const SimWorld* world, const char* place);
// 1 if a ward holds on `place` today, else 0; -1 on null.
int sim_world_warded(const SimWorld* world, const char* place);

// Omens read so far, oldest first: sim_world_omen_count returns how many, or
// -1 on a null world. sim_world_omen writes omen `index` as
// "day;rite;subject;sign;confidence_pct" (buffer convention); -1 on a null
// argument or an index out of range.
int sim_world_omen_count(const SimWorld* world);
int sim_world_omen(const SimWorld* world, int index, char* out, int cap);

// Festivals (K-2) ---------------------------------------------------------------
// 1 when the current day is a festival day, 0 when not, -1 on a null world.
int sim_world_is_festival(const SimWorld* world);
// As above for any day number (>= 1); -1 on a null world or day < 1.
int sim_world_is_festival_day(const SimWorld* world, int64_t day);
// Today's festival id (festivals.csv, e.g. "new_waters"), "" when none.
// Writes at most cap-1 bytes plus NUL; returns the untruncated length, or -1
// on a null argument.
int sim_world_festival(const SimWorld* world, char* out, int cap);
// The festival id on any day number (>= 1), "" when none; -1 on a null
// argument or day < 1.
int sim_world_festival_on(const SimWorld* world, int64_t day, char* out, int cap);
// 1 when the market trades today, 0 when today's festival closes it, -1 on a
// null world.
int sim_world_market_open(const SimWorld* world);

// Calendar (A14, D-024 §7) ---------------------------------------------------
// Today's date: year (1-based), month 1..12, day of month 1..30. Returns 0,
// or -1 on a null world or any null out-pointer.
int sim_world_date(const SimWorld* world, int* year, int* month, int* day_of_month);
// Today's month name (months.csv), "" when the canon names no months.
int sim_world_month_name(const SimWorld* world, char* out, int cap);
// Today's moon: "new" | "waxing" | "full" | "waning".
int sim_world_moon_phase(const SimWorld* world, char* out, int cap);
// Today's moon illumination, 0 (new) .. 100 (full); -1 on a null world.
int sim_world_moon_illumination(const SimWorld* world);
// Today's omen from calendar_days.csv: "favourable" | "unfavourable" | "".
int sim_world_day_omen(const SimWorld* world, char* out, int cap);
// Today's named day of the month (e.g. the eššešu of Nanna), "" when none.
int sim_world_day_observance(const SimWorld* world, char* out, int cap);

// People and their day (K-2) ------------------------------------------------------
// The id of the npc at `index` (0..sim_world_npc_count-1). Writes at most
// cap-1 bytes plus NUL; returns the untruncated length, or -1 on a null
// argument or an index out of range.
int sim_world_npc_id(const SimWorld* world, int index, char* out, int cap);
// What npc `npc` is doing at `hour` (0..23) on the current day: per-person
// resolution (role + person overrides + season + festival). Each writes at
// most cap-1 bytes plus NUL and returns the untruncated length, or -1 on a
// null argument, an unknown npc, or an npc with no schedule at all.
//   _task_at     the task text
//   _place_at    where: a places.csv id ("" = unplaced / off the street)
//   _schedule_at the schedule row id (or festival id for a festival gathering)
int sim_world_npc_task_at(const SimWorld* world, const char* npc, int hour, char* out, int cap);
int sim_world_npc_place_at(const SimWorld* world, const char* npc, int hour, char* out, int cap);
int sim_world_npc_schedule_at(const SimWorld* world, const char* npc, int hour, char* out,
                              int cap);

// Character progression (W4-A; sim/Character.hpp, sim/Progression.hpp) -------
// mechanics.md rows 20-25. Actors: "player" (the full sheet) or any npc id
// (a light, read-only sheet). Nothing here gates on act, story or quest state
// (D-021); every gate is a number earned in play.
//
// The W4-A verbs share one set of refusal codes; a refusal changes no state:
//   -1  null argument or internal error
//   -2  unknown id (skill, talent, calling, teacher, employer, market, polity, patron)
//   -3  a requirement not met yet (level, skill, standing, literacy, calling held, outlawed)
//   -4  nothing more to gain here (practice/teacher/text cap reached, already chosen, top rank)
//   -5  cannot pay / not held / out of stock / no unspent talent point
//   -6  exhausted: sleep first (sim_world_advance_needs with sleeping = 1)
//   -7  refused by the other side (does not teach / hire / trade that; market
//       closed today; not a calling / not a specialisation of yours; bad hours
//       or quantity)
// sim_world_progression_refusal names the exact reason of the last refusal.

// Reads. Attribute 1..10, skill 0..100 (base or effective = base + talent
// and specialisation bonuses). -1 on a null argument, -2 for an unknown
// actor, attribute or skill.
int sim_world_attribute(const SimWorld* world, const char* actor, const char* attr);
int sim_world_skill(const SimWorld* world, const char* actor, const char* skill);
int sim_world_effective_skill(const SimWorld* world, const char* actor, const char* skill);

// The player's level (1..40), skill points earned (xp), the xp total at which
// the next level comes (0 at the cap), and unspent talent points. -1 on null.
int sim_world_level(const SimWorld* world);
int sim_world_character_xp(const SimWorld* world);
int sim_world_xp_for_next_level(const SimWorld* world);
int sim_world_talent_points(const SimWorld* world);

// Talents taken (in order) / choosable right now (sorted), semicolon-joined.
// Buffer convention (writes at most cap-1 bytes plus NUL; returns the
// untruncated length, -1 on null).
int sim_world_talents(const SimWorld* world, char* out, int cap);
int sim_world_talents_available(const SimWorld* world, char* out, int cap);
// 0 taken; else a refusal code.
int sim_world_choose_talent(SimWorld* world, const char* talent);

// The calling in `slot` (0 first calling, 1 specialisation, 2 second
// calling), "" when not chosen. Buffer convention; -1 on null or a bad slot.
int sim_world_calling(const SimWorld* world, int slot, char* out, int cap);
// 0 chosen; else a refusal code. Levels: 5 / 15 / 25.
int sim_world_choose_calling(SimWorld* world, const char* calling);
int sim_world_choose_specialisation(SimWorld* world, const char* specialisation);
int sim_world_choose_second_calling(SimWorld* world, const char* calling);

// Growing a skill. Each returns the skill points gained (>= 0; a session can
// gain 0 points and still count) or a refusal code. `hours` 1..12. Each
// session advances the player's needs by `hours` awake.
//   practice: alone, any skill, up to 25
//   train:    with a resident teacher (skill_teachings.csv), paying the fee
//             from the player's purse to the teacher, up to the teacher's skill
//   study:    from a text item the player holds (read, not consumed); needs
//             scribal arts 10; up to the text's limit
//   work:     a shift for a resident employer (work_roles.csv): the work's
//             skills grow and the wage (silver to the purse, or rations to
//             the inventory) is paid
int sim_world_practice_skill(SimWorld* world, const char* skill, int hours);
int sim_world_train_skill(SimWorld* world, const char* skill, const char* teacher, int hours);
int sim_world_study_skill(SimWorld* world, const char* skill, const char* item, int hours);
int sim_world_work(SimWorld* world, const char* employer, int hours);

// Who teaches a skill: "npc:max_level:fee_per_hour;..." (sorted by npc id);
// what an npc teaches: "skill:max_level:fee_per_hour;..." (sorted by skill).
// The fee is what the player would pay now. Buffer convention; -1 on null.
int sim_world_skill_teachers(const SimWorld* world, const char* skill, char* out, int cap);
int sim_world_skills_taught_by(const SimWorld* world, const char* npc, char* out, int cap);
// Every skill id (sorted), semicolon-joined. Buffer convention; -1 on null.
int sim_world_skill_ids(const SimWorld* world, char* out, int cap);

// Trade at a city's market (the verb trade skills grow by). Silver moves
// through the actor's purse; goods through its inventory and the market's
// stock. Prices: the market price, bettered by bargaining, talents and rank
// with the city's polity. Returns the silver paid / received (>= 0) or a
// refusal code. Quotes: the silver the deal would move, -1 on null, -7 when
// the market does not trade the item.
int64_t sim_world_purse(const SimWorld* world, const char* actor);  // -1 on null
int64_t sim_world_buy(SimWorld* world, const char* actor, const char* city, const char* item, int qty);
int64_t sim_world_sell(SimWorld* world, const char* actor, const char* city, const char* item, int qty);
int64_t sim_world_buy_quote(const SimWorld* world, const char* actor, const char* city,
                            const char* item, int qty);
int64_t sim_world_sell_quote(const SimWorld* world, const char* actor, const char* city,
                             const char* item, int qty);

// Social rank with a polity (factions.csv id): 0 the Outsider .. 6 Lugal
// (ranks.csv). raise: one tier up, by deeds (standing) and a patron's act (a
// resident of that polity; none needed for tier 6). Returns the new tier or
// a refusal code. Outlawry (an exile/death verdict) drops the rank to 0.
int sim_world_rank(const SimWorld* world, const char* polity);
int sim_world_raise_rank(SimWorld* world, const char* polity, const char* patron);

// A readable multi-line summary of the player's sheet for the UI (level, xp,
// callings, attributes, skills with effective values, talents, ranks).
// Buffer convention; -1 on null.
int sim_world_character_summary(const SimWorld* world, char* out, int cap);
// The exact reason of the last W4-A refusal ("" after a success), e.g.
// "teacher_surpassed". Buffer convention; -1 on null.
int sim_world_progression_refusal(const SimWorld* world, char* out, int cap);

// Combat (W4-B; sim/Combat.hpp + sim/CombatActions.hpp) ------------------------
// Actors are "player" or npc ids (any other id is a stranger the engine
// spawned — e.g. a W4-C bandit — and works the same). Zones: 0 head,
// 1 torso, 2 arms, 3 legs. Buffer convention as above (writes at most cap-1
// bytes plus NUL, returns the untruncated length, -1 on a null argument).
// An npc is armed with his combat style's kit the first time he fights.

// Equips an arms item (db/canon/arms.csv) the actor holds in inventory.
//   0 ok; -1 null; -2 unknown or OPEN arms item; -3 not in inventory; -4 actor dead
int sim_world_equip(SimWorld* world, const char* actor, const char* item);
// 0 unequipped; 1 was not equipped; -1 null.
int sim_world_unequip(SimWorld* world, const char* actor, const char* item);
// Equipped ids, semicolon-joined: weapon, shield, then armour (sorted).
int sim_world_equipped(const SimWorld* world, const char* actor, char* out, int cap);

// One blow. zone_hint 0..3 aims (lands there on a good enough margin); any
// other value strikes wherever it falls. `out` (may be null) receives
// "outcome=...;attacker=...;defender=...;weapon=...;chance_bp=...;roll_bp=...;
//  zone=...;aimed=0|1;type=cut|pierce|blunt;damage=N;severity=...;bleed=N;
//  gap=0|1;shield_broke=0|1;weapon_broke=0|1;armour_broke=0|1;winded=0|1"
// (a refusal writes "outcome=invalid;refusal=<why>").
// Returns 0 dodged, 1 blocked, 2 parried, 3 deflected (armour held),
// 4 wounded, 5 knocked out, 6 killed; -1 null; -2 refused (attacker
// incapacitated, defender already dead, same actor, no ammunition).
int sim_world_attack(SimWorld* world, const char* attacker, const char* defender, int zone_hint,
                     char* out, int cap);

// The body. Each returns -1 on a null argument (an actor never hurt reads as
// health 100, stamina 100, morale 50, no wounds).
int sim_world_health(const SimWorld* world, const char* actor);
int sim_world_stamina(const SimWorld* world, const char* actor);
int sim_world_morale(const SimWorld* world, const char* actor);
// Open (unhealed) wound damage on `zone` (0..3); -1 on null or a bad zone.
int sim_world_wound(const SimWorld* world, const char* actor, int zone);
// Health lost per hour to bleeding right now.
int sim_world_bleeding(const SimWorld* world, const char* actor);
// 1 dead, 0 alive, -1 null.
int sim_world_is_dead(const SimWorld* world, const char* actor);
// Conditions, sorted and semicolon-joined: bleeding, concussed, dead,
// knocked_out, limp, outlaw, prisoner, surrendered, useless_arm, weak_arm,
// winded, scar:<zone>.
int sim_world_combat_status(const SimWorld* world, const char* actor, char* out, int cap);
// Open wounds, "|"-joined, each "zone:type:severity:remaining:bleed:treatment"
// (treatment none|bound|herbs|healer).
int sim_world_wounds(const SimWorld* world, const char* actor, char* out, int cap);

// Time on the body. rest: `hours` of sleep — Needs advance as sleeping,
// wounds rest (stamina back fast; 8+ rest hours speed the day's healing).
// combat_advance: `hours` awake — bleeding, stamina, knockout, and the
// Needs wounds and hot armour cost (the base needs climb stays with
// sim_world_advance_needs). Both return 1 if the actor died (bled out),
// 0 otherwise, -1 on null. Days heal on their own (sim_world_advance_days).
int sim_world_rest(SimWorld* world, const char* actor, int hours);
int sim_world_combat_advance(SimWorld* world, const char* actor, int hours);
// A breather mid-fight: +5 stamina per round. Returns the new stamina, -1 null.
int sim_world_catch_breath(SimWorld* world, const char* actor, int rounds);

// Treatment. method: "bind" (uses a linen_bandage if held), "herbs" (spends
// one healing_herbs), "healer" (`healer` = a physician npc; 5 silver from the
// actor's purse). Returns wounds improved (>= 0); -1 null; -2 unknown method;
// -3 no herbs held; -4 not a healer; -5 cannot pay; -6 actor dead.
int sim_world_treat_wounds(SimWorld* world, const char* actor, const char* method,
                           const char* healer);

// What an npc does now given `allies` and `enemies` still standing: writes
// "fight"/"flee"/"surrender"/"none"; returns 0/1/2/3 respectively, -1 null.
int sim_world_stance(const SimWorld* world, const char* actor, int allies, int enemies, char* out,
                     int cap);
// Applies a combat_styles.csv style (gives the kit, equips it, sets morale).
// 0 ok; -1 null; -2 unknown style or dead actor.
int sim_world_apply_combat_style(SimWorld* world, const char* actor, const char* style);
// The style an npc fights in ("" for an unknown npc).
int sim_world_combat_style_of(const SimWorld* world, const char* npc, char* out, int cap);

// Yielding and prisoners. surrender / take_prisoner / release: 0 ok, -1 null,
// -2 not possible (dead, already a prisoner, not yielded to this captor...).
int sim_world_surrender(SimWorld* world, const char* who, const char* to);
int sim_world_take_prisoner(SimWorld* world, const char* captor, const char* captive);
int sim_world_release_prisoner(SimWorld* world, const char* captive);
// The captive pays his ransom (30 silver) to the captor, the shortfall as a
// loan; he goes free. Returns silver paid now; -1 null or not a prisoner.
// `loan_out` (may be null) receives the loan id ("" when paid in full).
int64_t sim_world_ransom(SimWorld* world, const char* captive, char* loan_out, int cap);
// Strips a dead body (or the looter's own prisoner). Items moved; -1 null;
// -2 the body cannot be looted.
int sim_world_loot(SimWorld* world, const char* looter, const char* body);

// Law. agree_duel: both men agree to fight today (0; -1 null). set_outlaw:
// marks an actor an outlaw/robber (flag 0/1) whom it is lawful to slay (0; -1).
int sim_world_agree_duel(SimWorld* world, const char* a, const char* b);
int sim_world_set_outlaw(SimWorld* world, const char* actor, int flag);
// Files the attacker's deeds against the victim with Justice (assault,
// murder, self_defence, slaying_a_robber, duel_killing — see
// CombatActions.hpp). `witnesses` is a semicolon list of npc ids (may be
// null/empty: unwitnessed, the crime stays open). `out` receives
// "<law_row>;<crime_id>". Returns 1 filed, 0 nothing to file, -1 null.
int sim_world_combat_crime(SimWorld* world, const char* attacker, const char* victim,
                           const char* place_city, const char* witnesses, char* out, int cap);

// The smith. repair: 0 ok; -1 null; -2 unknown arms item; -3 not held;
// -4 not a smith; -5 cannot pay; -6 nothing to repair; -7 broken metal must
// be recast. recast (melt `from`, cast `into`): 0 ok; -1..-5 as repair;
// -7 not castable metal.
int sim_world_repair(SimWorld* world, const char* actor, const char* item, const char* smith);
int sim_world_recast(SimWorld* world, const char* actor, const char* from, const char* into,
                     const char* smith);

// A group fight: semicolon lists of actor ids. Returns 0 if side A holds the
// field, 1 side B, 2 undecided; -1 null. `out` receives
// "rounds=N;blows=N;fled=a,b;surrendered=...;fallen=...".
int sim_world_skirmish(SimWorld* world, const char* side_a, const char* side_b, int max_rounds,
                       char* out, int cap);
// Deaths recorded so far (any cause); -1 null.
int sim_world_death_count(const SimWorld* world);

#ifdef __cplusplus
}  // extern "C"
#endif

// --- W4-C: the wild lands (places, travel, camps, raids) -----------------------
#include "sim/CApiWild.h"
// --- end W4-C

// --- W5-B: faction politics (tiers, oaths, outlawry, treaties, raid politics) --
#include "sim/CApiFaction.h"
// --- end W5-B
// --- W5-A: divine wrath (wrath queries, the curse, the oath-break verb) --------
#include "sim/CApiDivine.h"
// --- end W5-A
// --- W6-A: the player's verbs (carried-goods enumeration, best food/drink) ------
#include "sim/CApiVerbs.h"
#include "sim/CApiQuests.h"  // A7: quests, the journal, dialogue
#include "sim/CApiPeople.h"  // A7: npc names, memories, the events feed
#include "sim/CApiItems.h"   // P0a: stacks, carrying, world items, containers, minutes, notices
// --- end W6-A
