#pragma once
// Wild.hpp — W4-C: the wild lands beyond the walls (D-002's ~20 km² region
// around the City of the Moon; D-021 open play). The state types and pure
// queries live here; the verbs that touch several modules' state (raids,
// travel, camp actions) live in sim/WildActions.hpp, the same split as
// Actions.hpp/Rites.hpp.
//
// Imported mechanics (docs/mechanics.md):
//   row 11  minor factions and the raid formula
//           (hunger + opportunity - defence - fear -> raid chance)
//   row 31  transport (foot, donkey, ox-cart, horse, chariot, boats)
//   row 35  no fast travel by default (travel costs game-minutes; the engine
//           plays them out — nothing here teleports)
//   row 36  one map + off-map ventures and journeys
// Content: db/canon/wild_places, wild_links, wild_groups, wild_encounters,
// caravans, transport_modes, weather (all INVENTED under D-018/D-021,
// ledgered in docs/proposals/invented-ledger-wild.md). Contract:
// kernel/contracts/module_Wild.md.
//
// D-022: every value that affects state or a roll is an integer — chances are
// basis points (0..10000), distances metres, durations minutes. No float.
#include "sim/Rng.hpp"
#include "sim/Types.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sim {

class Db;

// ---------------------------------------------------------------------------
// The combat seam (W4-B). A parallel track builds sim/Combat.hpp
// (resolve_attack / resolve_skirmish). Until the merge, every fight in the
// wild goes through this hook; WildState::skirmish == nullptr selects the
// integer stub below. At merge, W4-B wires an adapter:
//     w.wild.skirmish = &combat_skirmish_adapter;   // in WorldState::init
// and nothing else in the wild code changes. See module_Wild.md "Combat seam".
struct SkirmishSide {
    Id id;                 // group id, "player", encounter id, caravan run id
    int fighters = 1;      // bodies able to fight (>= 0)
    int prowess_pct = 100; // quality: 100 = an ordinary armed man
    int morale = 50;       // 0..100
    bool has_player = false;
};

struct SkirmishRequest {
    SkirmishSide attacker;
    SkirmishSide defender;
    Id place;              // node id where it happens
    int hour = 12;         // 0..23
    std::string context;   // "encounter" | "camp_assault" | "raid" | "duel" | "feud" | "infiltration"
};

struct SkirmishOutcome {
    bool attacker_won = false;
    int attacker_losses = 0;   // fighters killed or put out of the fight
    int defender_losses = 0;
    bool player_wounded = false;
    std::string note;
};

using SkirmishResolver = SkirmishOutcome (*)(const SkirmishRequest&, Rng&);

// The stub resolver: power = fighters x prowess x (50 + morale), each side's
// power scaled by an integer roll of 75..125%; ties go to the defender.
SkirmishOutcome stub_skirmish(const SkirmishRequest& req, Rng& rng);

// ---------------------------------------------------------------------------
// Static data (loaded from canon by init_wild; never saved — canon is data).

struct WildNode {
    Id id;
    std::string name;
    std::string kind;       // wild_places kind, places.csv kind, or "city" (off-map)
    std::string source;     // "wild" | "place" | "city"
    int x_m = 0, y_m = 0;   // metres from the Moon Gate (wild places only)
    int danger = 0;         // 0..10
    bool camp_site = false;
    Id edge_to;             // off-map city a journey starts toward ("" = none)
    std::vector<std::pair<Id, int>> loot;          // lootable contents (ruins, caves, tombs)
    std::vector<std::pair<std::string, int>> risks; // risk token -> basis points
    int restock_days = 0;   // 0 = never refills (a robbed tomb stays robbed)
};

struct WildLink {
    Id id, a, b;
    std::string kind;       // road | track | river | marsh_path | desert_track | journey
    int distance_m = 0;
    int danger = 0;
};

struct TransportMode {
    Id id;
    std::string name;
    int speed_m_per_h = 4000;
    Id requires_item;       // "" = always available
    std::vector<std::string> link_kinds;
};

struct EncounterDef {
    Id id;
    std::string name, kind;  // kind: bandits | raiders | refugees | merchants | animals
    bool hostile = false;
    int weight = 0, danger_min = 0;
    std::vector<std::string> terrains;  // link kinds / place kinds; empty = anywhere
    std::string hours;                  // any | day | night | dusk_night
    std::vector<Id> seasons;            // empty = every season
    int drought_pct = 0, war_pct = 0;   // weight +pct per stage (may be negative)
    int foes_min = 1, foes_max = 1, prowess_pct = 100;
};

struct GroupDef {
    Id id;
    std::string name, kind;   // bandits | raiders | partisans
    Id faction;               // factions.csv id ("" = outlaws of no faction)
    std::vector<std::string> leaders;
    std::vector<Id> camp_sites;  // first = home camp; the rest are where it moves when threatened
    std::vector<Id> territory;
    std::vector<std::string> forms_when;  // "start" | drought_gte:N | war_gte:N (AND)
    int form_days = 0, repop_days = 60;
    int base_strength = 4, max_strength = 12;
    int start_supplies = 50, start_morale = 50, prowess_pct = 100;
    std::map<std::string, int> prefers;   // raid target -> preference weight
    std::vector<Id> grudges;              // faction ids and group ids it hates
};

struct CaravanDef {
    Id id;
    std::string name;
    Id faction;
    std::vector<Id> route;                // node ids, edge -> the Moon Gate
    std::vector<std::pair<Id, int>> goods;
    int every_days = 10, offset_days = 0, guards = 3;
};

struct WeatherDef {
    Id id;
    std::string name;
    std::map<Id, int> season_weights;
    int drought_weight = 0;
    int travel_pct = 100, desert_travel_pct = 100;
    int thirst_per_hour = 0, encounter_bp = 0, raid_opportunity = 0;
    bool raids = true;
};

// ---------------------------------------------------------------------------
// Dynamic state (saved by Snapshot as the trailing optional WILD section).

struct Camp {
    Id place;
    Id group;
    std::string status;          // "occupied" | "cleared" | "abandoned"
    int lookout = 0;             // alertness 0..100 of the posted lookout (0 = none)
    int alarm_days = 0;          // days the camp stays on alert after a threat
    std::map<Id, int> loot;      // the loot pile: item -> units
    std::vector<Id> captives;    // npc ids held here
    bool known_to_player = false;  // the player has heard where it is
    bool scouted = false;          // the player has looked it over
    DayNumber since = 0;           // day the current status began
};

struct Group {
    Id id;
    bool active = false;
    Id camp;                 // current camp place ("" when inactive)
    std::string leader;      // name; "player" when the player leads it
    int strength = 0, peak_strength = 0, supplies = 0, morale = 0;
    int pressure = 0;        // consecutive days forms_when has held
    DayNumber formed_day = 0, cleared_day = 0;
    int times_formed = 0, times_cleared = 0, raids = 0;
    int grudge_vs_player = 0;   // 0..100
    DayNumber paid_until = 0;   // bribed: no raids on the city, no attacks on the player
    bool allied = false;
    bool player_member = false;
    bool player_led = false;
    bool rumoured = false;      // the city has heard where it camps now
};

struct CaravanRun {
    Id id;                    // "<caravan>#<departure day>"
    Id def;
    int leg = 0;              // index into the def's route of the current node
    int guards = 0;
    std::map<Id, int> goods;
    bool escorted = false;
    int escort_fighters = 0;  // the player plus companions when escorted
    DayNumber departed = 0;
};

struct RaidRecord {
    Id id;                    // "raid_<n>"
    DayNumber day = 0;
    Id group;
    std::string target;       // fields | herds | caravans | market
    Id place;
    int chance_bp = 0;
    bool success = false;
    int goods_taken = 0;
    std::map<Id, int> goods;  // what was carried off, item -> units
    std::vector<Id> slain, taken;
    std::string summary;
};

struct EncounterRecord {
    Id id;                    // "enc_<n>"
    DayNumber day = 0;
    int hour = 0;
    Id link;
    Id encounter;             // wild_encounters id
    std::string kind;
    Id group;                 // "" unless a wild group's men
    std::string stance;       // fight | pay | flee | rob | give | pass
    std::string outcome;      // won | lost | paid | fled | passed | met | robbed_them | gave
    int foes = 0, foes_lost = 0, allies_lost = 0;
};

struct SiteState {
    std::map<Id, int> loot_left;
    int searches = 0;
    DayNumber last_search = 0;
    DayNumber emptied_day = 0;
};

struct WildState {
    // --- static (init_wild, from canon) ---
    std::map<Id, WildNode> nodes;
    std::vector<WildLink> links;
    std::map<Id, TransportMode> modes;
    std::vector<EncounterDef> encounter_defs;
    std::vector<GroupDef> group_defs;
    std::vector<CaravanDef> caravan_defs;
    std::vector<WeatherDef> weather_defs;
    SkirmishResolver skirmish = nullptr;   // nullptr = stub_skirmish (W4-B wires at merge)

    // --- dynamic (saved) ---
    std::map<Id, Group> groups;
    std::map<Id, Camp> camps;              // by camp place
    std::vector<CaravanRun> caravans;      // runs on the road
    std::vector<RaidRecord> raids;         // most recent kMaxRaidLog, oldest first
    std::vector<EncounterRecord> encounters;  // most recent kMaxEncounterLog
    std::map<Id, std::string> npc_fate;    // npc -> "slain" | "captive:<group>" | "sold_east"
    std::map<Id, DayNumber> captive_since; // npc -> day taken
    std::map<Id, SiteState> sites;         // lootable site state by place
    Id player_place = "moon_gate_place";
    Id player_group;                       // group the player belongs to / leads
    Id escorting;                          // caravan run the player escorts
    int herd_head = 0;                     // the city's flocks on the grazing lands
    int player_fear = 0;                   // 0..100: what the wild fears of the player
    int player_wounds = 0;                 // wounds taken in the wild (combat seam)
    int infamy = 0;                        // raids the player has led
    int next_raid = 1, next_encounter = 1, action_serial = 0;
    int caravans_arrived = 0, caravans_robbed = 0;
};

// Tunables (INVENTED, D-018; ledgered). Integer only (D-022).
constexpr int kRaidBpPerPoint = 20;      // raid chance: score x 20 bp ...
constexpr int kRaidMaxBp = 2000;         // ... capped at 20% a day
constexpr int kMaxRaidLog = 64;
constexpr int kMaxEncounterLog = 64;
constexpr int kHerdStart = 60;
constexpr int kHerdMax = 80;
constexpr int kCaptiveDays = 45;          // a captive not freed by then is sold east
constexpr int kBountyPerFighter = 6;      // silver per fighter of a cleared camp
constexpr int kTributePerFighter = 5;     // silver per fighter to buy a month's peace
constexpr int kTributeDays = 30;
constexpr int kRansomSilver = 40;
constexpr int kEscortFee = 12;
constexpr int kCaravanLegsPerDay = 2;
constexpr int kRescueRewardSilver = 30;

// Loads the static tables and seeds the dynamic state (groups whose
// forms_when is "start" form on day 1; the herds; lootable sites).
// Idempotent over the static part.
void init_wild(const Db& db, WildState& s);

// --- pure queries ---------------------------------------------------------
const WildNode* find_node(const WildState& s, const Id& id);
const GroupDef* find_group_def(const WildState& s, const Id& id);
const Group* find_group(const WildState& s, const Id& id);
const Camp* find_camp(const WildState& s, const Id& place);
std::vector<const WildLink*> links_of(const WildState& s, const Id& node);
// The other end of a link seen from `node` ("" if the link does not touch it).
Id other_end(const WildLink& l, const Id& node);
bool in_territory(const GroupDef& g, const Id& place);
// "" | "slain" | "captive:<group>" | "sold_east".
std::string npc_fate(const WildState& s, const Id& npc);
bool npc_out_of_play(const WildState& s, const Id& npc);
// Minutes to cross one link by `mode` under `weather` (integer; >= 1).
int leg_minutes(const WildState& s, const WildLink& l, const TransportMode& m, const Id& weather);
// Snapshot seam: the dynamic WildState as rows of fields (the Snapshot
// writer escapes them), and back. wild_load_rows throws std::runtime_error on
// a malformed row; static data is left as init_wild built it.
std::vector<std::vector<std::string>> wild_save_rows(const WildState& s);
void wild_load_rows(WildState& s, const std::vector<std::vector<std::string>>& rows);

// A forms_when list holds under these facts ("start" always holds).
bool conditions_hold(const std::vector<std::string>& when, const WorldFacts& facts);

}  // namespace sim
