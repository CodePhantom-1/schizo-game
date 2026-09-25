#pragma once
// WildInternal.hpp — W4-C private helpers shared by the Wild*.cpp files.
// Not a public header: nothing outside kernel/src/Wild*.cpp includes it.
#include "sim/WildActions.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sim::wild {

constexpr const char* kCity = "city_of_the_moon";   // D-005: the one simulated city
constexpr const char* kGate = "moon_gate_place";
constexpr const char* kPlayer = "player";
constexpr const char* kPlayerBand = "player_band";

// W5-B: the belligerents of the world's war against the city's alliance
// (city_of_the_moon's jurisdiction, cities.csv). The Empire is the rebellion's
// enemy (wb §4.1-4.2); the eastern barbarians raid the settled lands, the
// rebellion's included (wb §4.1, §4.3; events.csv rebel_agents_agitate,
// eastern_raiders_pillage). INVENTED glue, ledgered in
// docs/proposals/invented-ledger-faction-raids.md.
constexpr const char* kEmpireFaction = "the_empire";
constexpr const char* kBarbarianFaction = "the_barbarians";

// Stable 64-bit id hash (FNV-1a) for rng salts.
std::uint64_t salt_of(const std::string& s);
// A fresh stream for (day, what) — never advances the world rng.
Rng stream(const WorldState& w, DayNumber day, const std::string& what);
// A stream for one player verb (bumps action_serial so repeated verbs differ).
Rng action_stream(WorldState& w, const std::string& what);
inline int roll_bp(Rng& r) { return r.int_in(0, 9999); }

std::vector<std::string> split(const std::string& s, char sep);
int to_int(const std::string& s, int fallback = 0);
std::vector<std::pair<Id, int>> parse_pairs(const std::string& s);  // "a:1;b:2"

// Runs the combat seam (w.wild.skirmish or the stub).
SkirmishOutcome fight(WorldState& w, const SkirmishRequest& req, Rng& r);

// The city's faction (cities.csv jurisdiction of the City of the Moon).
Id city_faction_id(const WorldState& w);

// Watch still in play: watchmen, gatekeepers and paladins neither slain nor captive.
int watch_alive(const WorldState& w);
// First npc in play with one of `roles`, starting at a salted offset ("" = none).
Id pick_npc(const WorldState& w, const std::vector<std::string>& roles, Rng& r);

// A world event in EventsState::fired (and its fire count, keeping
// sum(fire_count_by_rule) == fired.size()).
void log_event(WorldState& w, const Id& rule_id, const std::string& summary);
// A rumour told to the city's talkers (gatekeepers, tavern keeper, herdsman,
// traders): subject "camp:<place>" / "group:<id>" / "wild:<what>".
void spread_rumour(WorldState& w, const Id& subject, const std::string& fact);

// Captives.
void take_captive(WorldState& w, const Id& npc, Group& g, Camp& camp);
void free_captive(WorldState& w, const Id& npc, bool rescued_by_player);
void kill_npc(WorldState& w, const Id& npc);

// Camps.
Camp& camp_ref(WorldState& w, const Id& place);
void clear_camp(WorldState& w, Group& g, bool by_player);
Id free_camp_site(const WorldState& w, const GroupDef& def, const Id& except);
void move_camp(WorldState& w, Group& g, const std::string& why);

// The player's side of a fight.
SkirmishSide player_side(const WorldState& w, int companions);
void player_defeated(WorldState& w, const std::string& by);

// Player purse / inventory.
Silver purse_of(const WorldState& w);
void give_player(WorldState& w, const Id& item, int units);

// Raid resolution shared by the tick and lead_raid.
// One target's side of the raid formula for group `g` on `day`.
RaidAssessment assess_target(const WorldState& w, const Group& g, const GroupDef& def,
                             const std::string& target, DayNumber day);
RaidRecord resolve_raid(WorldState& w, Group& g, const GroupDef& def, const RaidAssessment& a,
                        Rng& r, bool player_led, int hour);

// W5-B faction politics of one raid: what `def`'s faction owes or fears of
// `holder` (the city's faction for city targets, the caravan's faction for
// caravans). Folded into assess_target's opportunity/defence; see
// docs/proposals/invented-ledger-faction-raids.md for every rule.
struct RaidPolitics {
    int opportunity = 0;   // war, a grudge, outlawry
    int defence = 0;       // a live treaty reining the band
    bool blocked = false;  // a live no_raids treaty forbids this holder's things
    std::string note;      // what fired, ','-joined ("war", "grudge", "treaty", "outlaw")
};
RaidPolitics raid_politics(const WorldState& w, const GroupDef& def, const Id& holder);

// Where a raid target lies (the first target node in the group's territory).
Id target_place(const WorldState& w, const GroupDef& def, const std::string& target);
CaravanRun* caravan_in_territory(WorldState& w, const GroupDef& def);
const CaravanRun* caravan_in_territory(const WorldState& w, const GroupDef& def);
const CaravanDef* caravan_def(const WorldState& w, const Id& id);

void push_raid(WorldState& w, RaidRecord rec);
void push_encounter(WorldState& w, EncounterRecord rec);

}  // namespace sim::wild
