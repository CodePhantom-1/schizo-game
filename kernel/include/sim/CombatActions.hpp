#pragma once
// CombatActions.hpp — W4-B: the caller layer over sim/Combat.hpp. Like
// Actions.hpp, these are WorldState& verbs that sequence several modules'
// state: Combat (the fight), Needs (wounds tire and parch), Population and
// Events (a death is known), Justice (assault and killing are crimes, via
// Actions::commit_crime), Property (fees, ransom, debt), inventories (ammo,
// loot, kit). Combat.hpp itself writes only CombatState.
//
// W4-C (wild lands, bandits): call attack_in_world / skirmish_in_world for
// fights that should touch the world (ammo, deaths, events); call
// sim::resolve_attack / sim::resolve_skirmish directly for pure what-ifs on a
// scratch CombatState. Mark bandits with sim::set_outlaw (killing them is
// then lawful, laws.csv slaying_a_robber) and give them role "bandit" or
// apply_style_in_world(w, id, "drought_bandit").
//
// W4-A (character progression): combat_inputs_for() is the one merge point —
// replace its defaults with attribute()/effective_skill(). Nothing else reads
// attributes or skills.
//
// Determinism: all draws come from w.rng.fork(...) inside Combat.cpp; the
// world rng is never advanced. Integer maths only (D-022).
#include "sim/Combat.hpp"
#include "sim/World.hpp"

#include <string>
#include <vector>

namespace sim {

// --- tuning (INVENTED; docs/proposals/invented-ledger-combat.md) -------------------
constexpr Silver kHealerFee = 5;          // an asû's visit, silver grains
constexpr Silver kRepairFeePerBand = 5;   // full repair = band x this
constexpr Silver kRecastFeePerBand = 5;   // recasting = band(into) x this
constexpr Silver kTinFee = 20;            // tin bought to make bronze of copper
constexpr Silver kRansomSilver = 30;      // a captive's price (Code of Hammurabi §32 model)
constexpr int kRansomLoanRatePct = 20;    // the shortfall becomes a debt at the customary rate
constexpr int kRansomLoanTermDays = 90;

// The W4-A merge point: the inputs an actor fights with. Today: the actor's
// combat style (style_inputs) when one is applied, else defaults (the
// player: skill 20 in everything; npcs: 15).
CombatInputs combat_inputs_for(const WorldState& w, const Id& actor);

// Arms an npc on his first fight: applies style_for(faction, role) (giving
// the kit into his inventory) if he has no combatant yet. No-op for the
// player, unknown ids, or an actor already known to Combat.
void ensure_combatant(WorldState& w, const Id& actor);

// Equips an arms item the actor holds (inventory count >= 1).
// 0 ok; -2 unknown/OPEN arms item; -3 not in inventory; -4 actor dead.
int equip_in_world(WorldState& w, const Id& actor, const Id& item);

// One blow in the world: ammo is checked and spent for ranged weapons (the
// refusal "no_ammunition"), a kill runs on_killed. zone_hint -1 = anywhere.
AttackResult attack_in_world(WorldState& w, const Id& attacker, const Id& defender, int zone_hint);

// A group fight in the world. Fighters holding a ranged weapon with no ammo
// fight with their hands; ammo is spent per shot (clamped at 0); every death
// runs on_killed.
SkirmishResult skirmish_in_world(WorldState& w, const std::vector<Id>& side_a,
                                 const std::vector<Id>& side_b, int max_rounds = 20);

// The death hook: removes a killed npc from Population (and so from every
// schedule — npc_task_at answers nullopt), cuts his `knows` edges after his
// acquaintances remember "mourns <name>, killed by <killer>", drops his
// Needs, frees his prisoners, and logs a "combat_death" TriggeredEvent. His
// inventory stays under his id: the body can be looted.
void on_killed(WorldState& w, const Id& victim, const Id& killer, const std::string& cause);

// Hours pass for one body: Combat (bleeding, stamina, knockout) plus the
// Needs it costs — pain tires (fatigue +1/h per 25 open wound damage), a
// bleeding man thirsts (+1/h), hot armour tires the awake (+1/h per 4 heat).
// A death from bleeding runs on_killed. Returns true if the actor died.
bool advance_body_hours(WorldState& w, const Id& actor, int hours, bool resting);

// Rest: advance_body_hours(resting) and Needs as sleep (advance_needs sleeping).
bool rest_in_world(WorldState& w, const Id& actor, int hours);

// Treatment. method: "bind" (a linen_bandage is used if held — better on
// grave wounds), "herbs" (needs and spends one healing_herbs), "healer"
// (`healer` = a living npc whose role is physician; kHealerFee from the
// actor's purse to the healer's).
// Returns wounds improved (>= 0), or -2 unknown method, -3 no herbs,
// -4 not a healer, -5 cannot pay, -6 actor dead.
int treat_in_world(WorldState& w, const Id& actor, const std::string& method, const Id& healer);

// The justice side (city-life §3; laws.csv). Decides what the attacker's
// deeds against the victim were in law and files it through commit_crime:
//   victim yielded to / captive of the attacker   -> murder | assault
//   victim an outlaw (Combat flag, or the player outlawed by any faction)
//                                                  -> slaying_a_robber (dismissed)
//   an agreed duel that day                        -> duel_killing (a death) | nothing (wounds)
//   the victim struck first that day               -> self_defence (dismissed)
//   otherwise                                      -> murder (a death) | assault (wounds)
// Returns the law row filed ("" when there is nothing to file: no
// hostility on record, or wounds in an agreed duel); *crime_id receives the
// Justice crime id. Witnesses remember; the hearing follows kHearingDelayDays later.
Id file_combat_crime(WorldState& w, const Id& attacker, const Id& victim, const Id& place_city,
                     const std::vector<Id>& witnesses, Id* crime_id = nullptr);

// Ransom: the captive pays kRansomSilver to his captor from his purse; any
// shortfall becomes a loan (debtor captive, creditor captor). Frees him.
// Returns the silver paid now, or -1 if he is no one's prisoner.
Silver ransom_prisoner(WorldState& w, const Id& captive, Id* loan_id = nullptr);

// Looting: a dead body, or the looter's own prisoner, gives up everything he
// carries (inventory moved, gear unequipped, wear carried over). Returns the
// number of items moved, or -1 if the body cannot be looted.
int loot_body(WorldState& w, const Id& looter, const Id& body);

// Repair / recasting at a smith (a living npc whose people.csv work_place is
// a places.csv row of kind "smithy"). Fees go from the actor's purse to the
// smith's.
// repair: 0 ok; -2 unknown arms item; -3 not held; -4 not a smith;
//         -5 cannot pay; -6 nothing to repair; -7 broken metal must be recast.
// recast: 0 ok; -2 unknown arms item; -3 not held; -4 not a smith;
//         -5 cannot pay; -7 not castable metal (flint, iron, leather...).
int repair_at_smith(WorldState& w, const Id& actor, const Id& item, const Id& smith);
int recast_at_smith(WorldState& w, const Id& actor, const Id& from, const Id& into, const Id& smith);

// Gives the style's kit into the actor's inventory (one of each, when not
// already held) and applies the style. False for an unknown style.
bool apply_style_in_world(WorldState& w, const Id& actor, const Id& style_id);
// The style an npc fights in (style_for(faction, role)); "" for unknown npcs.
Id style_of_npc(const WorldState& w, const Id& npc);

// The day's healing, bleeding-out and death hooks (called by advance_days).
void tick_combat_world(WorldState& w);

}  // namespace sim
