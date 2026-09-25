#pragma once
// WildActions.hpp — W4-C: the wild's verbs over the whole WorldState. Like
// Actions.hpp and Rites.hpp, these are the only wild functions allowed to
// write more than one module's state: a raid takes goods off the market
// (Economy::consume, the theft/raid hook), kills or carries off people
// (WildState::npc_fate — Population rows are never deleted), starts rescue
// quests (Quests), files crimes (Actions::commit_crime), spreads rumours
// (Population::witness) and logs events (EventsState::fired). Standing is
// READ and moved only through Faction.hpp's public add_standing/outlaw —
// Faction.cpp is not touched (W4-D owns it).
//
// Determinism: every roll comes from w.rng.fork(...) salted by day, a stable
// id hash and WildState::action_serial for player verbs — never advancing the
// world rng, so no other module's stream moves. Integer maths only (D-022).
#include "sim/Wild.hpp"
#include "sim/World.hpp"

#include <string>
#include <vector>

namespace sim {

// The daily wild tick (WorldState::advance_days, after the hearings):
// weather -> caravans depart/advance/arrive -> herds and sites recover ->
// groups form / re-form, eat, recruit, desert, feud, move, raid -> captives
// past kCaptiveDays are sold east -> fear and grudges fade.
void tick_wild(WorldState& w);

// The day's weather id (weather.csv), a pure function of the world seed
// stream, the day, the season and the drought.
Id weather_on(const WorldState& w, DayNumber day);

// --- the raid formula (mechanics.md row 11; living-world §6) -----------------
// score = hunger + opportunity - defence - fear;
// chance_bp = clamp(score * kRaidBpPerPoint, 0, kRaidMaxBp)   (20 bp a point, 20% cap)
struct RaidAssessment {
    Id group;
    std::string target;   // best target today ("" = none available)
    Id place;
    int hunger = 0, opportunity = 0, defence = 0, fear = 0, score = 0;
    int chance_bp = 0;    // 0..kRaidMaxBp per day
};
RaidAssessment assess_raid(const WorldState& w, const Id& group_id, DayNumber day);

// --- travel (rows 31, 35, 36) -------------------------------------------------
struct TravelResult {
    int code = 0;          // 0 arrived; 1 halted on the way; < 0 refused (see below)
    std::string refusal;   // unknown_place | unknown_mode | needs_item | no_route | already_there | escorting
    Id from, to, stopped_at;
    Id mode;
    int minutes = 0;       // game-minutes spent (the caller advances its clock by them)
    std::vector<Id> path;  // nodes from `from` to where the player stands now
    std::vector<EncounterRecord> encounters;
    std::string halted_by; // "" | "defeat" | "collapse"
};
// Walks the cheapest route to `to` by `mode`, leg by leg: needs rise with the
// hours (thirst faster in heat and desert; water and food are taken from the
// player's inventory as needed), and each started hour rolls for an encounter
// weighted by danger, season, weather and time of day. `stance` sets how the
// player meets them: fight | pay | flee (hostile), rob | give | pass (others).
// Refusal codes: -1 bad argument, -2 unknown place, -3 unknown mode,
// -4 mode needs an item the player lacks, -5 no route, -6 already there,
// -7 escorting a caravan (travel with it, or abandon the escort first).
TravelResult travel(WorldState& w, const Id& to, const Id& mode, int start_hour,
                    const std::string& stance = "fight", int companions = 0);
// Cheapest node path by minutes for `mode` under today's weather (empty = none).
std::vector<Id> find_route(const WorldState& w, const Id& from, const Id& to, const Id& mode);

// --- player-driven outcomes (D-021 open play) --------------------------------
// code >= 0 success (see each), < 0 refusal; text says what happened.
struct WildActionResult {
    int code = 0;
    std::string text;
};
// Refusals shared by the verbs: -1 bad argument, -2 unknown camp/group/run/site,
// -3 the player is not where it happens, -4 not possible in this state,
// -5 cannot pay, -6 refused by the group.
WildActionResult scout_camp(WorldState& w, const Id& place, int hour);
// 1 cleared (bounty, standing, loot, captives freed), 0 beaten back.
WildActionResult attack_camp(WorldState& w, const Id& place, int companions, int hour);
// objective: free_captives | steal_loot | sabotage | learn. 1 done unseen, 0 caught (a fight).
WildActionResult infiltrate_camp(WorldState& w, const Id& place, const std::string& objective,
                                 int hour);
WildActionResult pay_off_group(WorldState& w, const Id& group);
WildActionResult ally_with_group(WorldState& w, const Id& group);
WildActionResult join_group(WorldState& w, const Id& group);
WildActionResult leave_group(WorldState& w);
// 1 the player now leads the group, 0 lost the duel.
WildActionResult challenge_leader(WorldState& w, const Id& group, int hour);
// Founds the player's own band ("player_band") at an empty camp site.
WildActionResult found_band(WorldState& w, const Id& camp_place, int companions);
// The player's group raids `target` (fields | herds | caravans | market) now.
// 1 success (loot share, infamy, a crime filed), 0 failed.
WildActionResult lead_raid(WorldState& w, const std::string& target, int hour);
WildActionResult ransom_captive(WorldState& w, const Id& npc);
WildActionResult escort_caravan(WorldState& w, const Id& run_id, int companions);
WildActionResult abandon_escort(WorldState& w);
// 1 robbed (goods to the inventory, a theft filed), 0 driven off.
WildActionResult rob_caravan(WorldState& w, const Id& run_id, int companions, int hour);
// Searches a ruin / cave / tomb: loot, risks; robbing a tomb is a crime (laws.csv tomb_robbery).
WildActionResult search_site(WorldState& w, const Id& place, int hour);

// --- rumours -------------------------------------------------------------------
// The wild rumours `npc` has heard (camps, raids, caravans), oldest first;
// hearing one about a camp marks that camp known to the player.
std::vector<std::string> hear_rumours(WorldState& w, const Id& npc);

}  // namespace sim
