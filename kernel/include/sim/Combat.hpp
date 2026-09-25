#pragma once
// Combat.hpp — W4-B: the body in a fight (health, stamina, zonal wounds,
// bleeding, knockout, death), arms and armour from db/canon/arms.csv, the
// deterministic attack resolver, group skirmishes, morale/stance, surrender
// and prisoners, healing over days, wear, repair and recasting.
//
// Imported mechanics: docs/mechanics.md row 20 (conditions: zonal wounds),
// row 29 (weapons: damage type, reach, speed, weight, balance, durability,
// quality; flint -> copper -> arsenical -> tin bronze; iron a curiosity;
// recasting), row 30 (armour: 4 zones, 2 layers, cultural shields/helmets,
// weight/stamina/heat trade-offs). Open play (D-021): nothing here reads act
// or quest state. Every tuning number is INVENTED machinery, listed in
// docs/proposals/invented-ledger-combat.md and kernel/contracts/module_Combat.md.
//
// Invariants:
//  - D-022: integer maths only. No float or double touches a value that
//    affects state or a roll; chances are basis points (0..10000) against a
//    bounded integer draw (Rng::int_in).
//  - randomness: every attack draws from base.fork(salt(attacker, defender,
//    seq)) — never advances the world rng; CombatState::seq makes successive
//    attacks differ and is saved.
//  - writes ONLY CombatState. Cross-module consequences (Needs fatigue,
//    Population removal on death, Justice crimes, purses, inventories) live in
//    the caller layer, sim/CombatActions.hpp.
//  - reads canon through Db only: arms.csv (OPEN rows skipped),
//    combat_styles.csv.
//  - W4-A seam: attributes and skills arrive as plain values in CombatInputs;
//    nothing here depends on sim/Character.hpp.
#include "sim/Rng.hpp"
#include "sim/Types.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace sim {

class Db;
struct Needs;

// --- zones ------------------------------------------------------------------
constexpr int kZoneHead = 0;
constexpr int kZoneTorso = 1;
constexpr int kZoneArms = 2;
constexpr int kZoneLegs = 3;
constexpr int kZoneCount = 4;
const char* zone_name(int zone);                 // "head"/"torso"/"arms"/"legs"; "" if out of range
int zone_from_name(const std::string& name);     // -1 if unknown

// --- inputs (the W4-A seam) ---------------------------------------------------
// Plain numbers the caller supplies. At merge, W4-A's Character fills
// strength/agility/endurance from attribute() and `skills` from
// effective_skill(); until then CombatActions supplies defaults/styles.
struct CombatInputs {
    int strength = 5;   // attributes, 1..10 (mechanics row 20)
    int agility = 5;
    int endurance = 5;
    // Skill tokens are arms.csv `skill` values (blades, axes, maces, spears,
    // bows, slings, shields) plus "dodge" and "unarmed"; 0..100.
    std::map<std::string, int> skills;
    int default_skill = 15;   // for any token absent from `skills`
    // Stance thresholds (combat styles set them): health at/below which the
    // fighter tries to flee / yields. surrender_at 0 = never yields.
    int flee_at = 30;
    int surrender_at = 15;
    int favoured_zone = -1;   // the zone this fighter aims for (-1 = none)
    int skill(const std::string& token) const;
};

// --- arms (db/canon/arms.csv) -------------------------------------------------
struct ArmsDef {
    Id id;
    std::string slot;          // melee | ranged | shield | armour
    std::string skill;         // skill token
    std::string damage_type;   // cut | pierce | blunt ("" for shields/armour)
    int damage = 0;
    int reach_cm = 0;          // melee reach, or range for ranged weapons
    int speed = 0;             // 1..10
    int weight_g = 0;
    int balance = 0;           // 1..10
    int durability = 0;        // max durability
    int quality = 50;          // 0..100
    std::vector<int> zones;    // armour: covered zones
    std::string layer;         // armour: under | over
    int prot_cut = 0, prot_pierce = 0, prot_blunt = 0;
    int block = 0;             // shield: block bonus
    int stamina = 0;           // stamina cost per swing / encumbrance
    int heat = 0;              // armour/shield heat load
    std::string material_tier; // flint | copper | arsenical | tin_bronze | iron | leather | linen | organic
    Id ammo;                   // ranged: items.csv id consumed per shot ("" = none)
    std::string culture;
    bool two_handed() const;   // bows and slings: no shield alongside
    int protection(const std::string& damage_type) const;
};

// The arms row for `item`, or nullopt (unknown or OPEN).
std::optional<ArmsDef> arms_def(const Db& db, const Id& item);
// The bare hands: the virtual weapon "fists" (not a canon row, like "water").
ArmsDef fists();

// --- the body -------------------------------------------------------------------
// Wound severity tiers (from the net damage of one blow).
constexpr int kScratch = 1, kLight = 2, kSerious = 3, kGrave = 4, kMortal = 5;
const char* severity_name(int severity);

// Treatment given to a wound (the best one received is kept).
constexpr int kTreatNone = 0, kTreatBound = 1, kTreatHerbs = 2, kTreatHealer = 3;

struct Wound {
    int zone = kZoneTorso;
    std::string type;          // cut | pierce | blunt
    int severity = kScratch;
    int damage = 0;            // the blow's net damage
    int remaining = 0;         // damage not yet healed (0 = closed)
    int bleed = 0;             // health lost per hour while bleeding
    int clot_hours = 0;        // hours until it clots by itself (0 = never / not bleeding)
    int treatment = kTreatNone;
    DayNumber day = 0;
};

struct Combatant {
    Id id;
    int health = 100;          // 0..health_cap(); 0 = dead
    int stamina = 100;         // 0..100
    int morale = 50;           // 0..100
    int ko_hours = 0;          // > 0: knocked out for that many more hours
    bool dead = false;
    bool outlaw = false;       // a robber/outlaw: slaying him is lawful (laws.csv slaying_a_robber)
    int rest_hours = 0;        // hours rested since the last daily heal
    Id style;                  // combat_styles.csv id applied, "" = none
    Id surrendered_to;         // "" = not yielded
    Id captor;                 // "" = free; else a prisoner of this actor
    Id last_attacker;
    Id weapon;                 // arms id ("" = fists)
    Id shield;
    std::vector<Id> armour;    // worn pieces (no two share a zone+layer)
    std::vector<Wound> wounds; // open and closed-this-day wounds, in order received
    std::vector<std::string> lasting;  // sorted: "scar:<zone>", "limp", "weak_arm"
    std::map<Id, int> durability;      // item id -> current durability (absent = full)
};

struct Hostility {        // who struck first between two actors, and when
    Id aggressor;
    Id other;
    DayNumber day = 0;
};

struct Duel {             // an agreed fight (laws.csv duel_killing)
    Id a, b;
    DayNumber day = 0;
};

struct Prisoner {
    Id captive, captor;
    DayNumber day = 0;
};

struct DeathRecord {
    Id id;
    DayNumber day = 0;
    Id killer;                 // "" = unknown
    std::string cause;         // slain | bled_out
};

struct CombatState {
    std::map<Id, Combatant> by_actor;
    std::vector<Hostility> hostilities;  // at most one per unordered pair
    std::vector<Duel> duels;
    std::vector<Prisoner> prisoners;
    std::vector<DeathRecord> deaths;     // ordered by day
    std::uint64_t seq = 0;               // attack counter (rng salt)
};

// Returns the actor's combatant, creating a fresh one (health 100) the first time.
Combatant& combatant_of(CombatState& s, const Id& actor);
const Combatant* find_combatant(const CombatState& s, const Id& actor);

// Derived body values.
int health_cap(const Combatant& c);          // 100 - open wound damage / 2
int open_damage(const Combatant& c, int zone);  // zone = -1: all zones
int bleeding(const Combatant& c);            // health lost per hour right now
bool conscious(const Combatant& c);          // alive and not knocked out
bool can_fight(const Combatant& c);          // conscious, not yielded, not a prisoner
int armour_weight_g(const Db& db, const Combatant& c);   // worn armour + shield
int armour_heat(const Db& db, const Combatant& c);
int current_durability(const Db& db, const Combatant& c, const Id& item);
// Current conditions, sorted: bleeding, concussed, dead, knocked_out, limp,
// prisoner, surrendered, useless_arm, weak_arm, winded, scar:<zone>...
std::vector<std::string> combat_effects(const Combatant& c);

// --- equipment ------------------------------------------------------------------
enum class EquipResult { Ok, UnknownItem, Dead };
// Equips an arms item into its slot (weapon / shield / armour zones+layer),
// displacing whatever it overlaps; a two-handed weapon displaces the shield
// and a shield displaces a two-handed weapon. Does not check inventory (the
// caller layer does).
EquipResult equip(const Db& db, CombatState& s, const Id& actor, const Id& item);
bool unequip(CombatState& s, const Id& actor, const Id& item);   // false if not equipped
std::vector<Id> equipped(const Combatant& c);   // weapon, shield, armour (sorted)

// --- the attack -------------------------------------------------------------------
enum class AttackOutcome {
    Dodged = 0, Blocked = 1, Parried = 2, Deflected = 3,  // no wound
    Wounded = 4, KnockedOut = 5, Killed = 6,               // a wound landed
    Invalid = -1,                                          // nothing happened
};

struct AttackResult {
    AttackOutcome outcome = AttackOutcome::Invalid;
    std::string refusal;       // Invalid: why ("attacker_incapacitated", "defender_dead", "same_actor")
    Id attacker, defender, weapon;
    int zone = -1;
    bool aimed = false;        // landed on the zone_hint
    std::string damage_type;
    int damage = 0;            // net damage after armour and zone
    int severity = 0;
    int bleed = 0;
    int chance_bp = 0;         // hit chance in basis points
    int roll_bp = 0;           // the draw, 0..9999
    bool found_gap = false;    // a well-placed blow halved the armour
    bool shield_broke = false, weapon_broke = false, armour_broke = false;
    bool winded = false;       // the attacker lacked the stamina for a full blow
    std::string text;          // "outcome=wounded;zone=torso;type=cut;damage=12;..."
};

// Per-actor inputs for one attack: body, and (optional) Needs as of now.
struct Fighter {
    Id id;
    CombatInputs in;
    const Needs* needs = nullptr;   // exhaustion/starvation penalties when set
};

// Resolves one blow. zone_hint 0..3 aims (landing there on a good enough
// margin), anything else strikes wherever the blow falls. Ranged weapons are
// resolved as a shot (no parry; shields matter more); ammo is the caller's.
// A defender who is knocked out, yielded or a prisoner is struck unopposed.
// Records the first aggressor of the pair for the day (Hostility) unless
// the pair already has one. Writes only CombatState.
AttackResult resolve_attack(const Db& db, const Rng& base, CombatState& s, const Fighter& attacker,
                            const Fighter& defender, int zone_hint, DayNumber day);

// --- morale and stance -----------------------------------------------------------
enum class Stance { Fight = 0, Flee = 1, Surrender = 2, None = -1 };
const char* stance_name(Stance st);
// What an NPC does now, from health, morale and the odds (allies/enemies
// still standing, the fighter himself not counted among allies). A limping
// fighter cannot flee and yields instead (or fights on if he never yields).
Stance choose_stance(const Combatant& c, const CombatInputs& in, int allies_standing,
                     int enemies_standing);

// --- group skirmish -----------------------------------------------------------------
struct SkirmishResult {
    int winner = -1;                   // 0 = side A holds the field, 1 = side B, -1 = undecided
    int rounds = 0;
    std::vector<AttackResult> log;     // every blow in order
    std::vector<Id> fled, surrendered, fallen;  // fallen = knocked out or killed
};

// Group vs group (W4-C raids, bandit ambushes). Each round, every fighter
// still able (sides interleaved A0,B0,A1,B1,...) takes a stance; fighters
// strike the weakest standing enemy (lowest health, then id) at their
// favoured zone; the fall of a comrade shakes every ally's morale. Ends when
// one side has nobody fighting, or after max_rounds. `inputs` maps actor ->
// inputs (missing = defaults). Writes only CombatState.
SkirmishResult resolve_skirmish(const Db& db, const Rng& base, CombatState& s,
                                const std::vector<Id>& side_a, const std::vector<Id>& side_b,
                                const std::map<Id, CombatInputs>& inputs,
                                const std::map<Id, const Needs*>& needs, DayNumber day,
                                int max_rounds = 20);

// --- time and healing -----------------------------------------------------------------
// Hours pass for one actor: bleeding drains health (death at 0, cause
// "bled_out"), light wounds clot, stamina returns, knockouts wear off.
// Returns true if the actor died in these hours.
bool advance_combat_hours(CombatState& s, const Id& actor, int hours, bool resting, DayNumber day);
// A breather within a fight: +kBreathStamina stamina per round.
void catch_breath(CombatState& s, const Id& actor, int rounds);
// One day of healing for everyone alive: open wounds close by a treatment-
// and rest-dependent amount, health climbs toward its cap, closed serious+
// wounds leave scars, grave leg/arm wounds a healer never saw leave a
// lasting limp / weak arm. Conscious NPCs (not "player") bind their own
// bleeding wounds; anyone still bleeding bleeds out the day. Returns the ids
// that died today (for the caller layer's death hook).
std::vector<Id> tick_combat(CombatState& s, DayNumber day);

// Treatment: kTreatBound (a linen bandage or torn cloth — `with_linen`),
// kTreatHerbs (a plaster), kTreatHealer (an asû's hands). Returns the number
// of wounds that were improved.
int treat_wounds(CombatState& s, const Id& actor, int method, bool with_linen);

// --- yielding, prisoners, duels, outlaws ------------------------------------------------
bool surrender(CombatState& s, const Id& who, const Id& to);   // false if dead/prisoner
// A yielded (to the captor) or knocked-out actor becomes the captor's prisoner.
bool take_prisoner(CombatState& s, const Id& captor, const Id& captive, DayNumber day);
bool release_prisoner(CombatState& s, const Id& captive);
const Prisoner* find_prisoner(const CombatState& s, const Id& captive);
void agree_duel(CombatState& s, const Id& a, const Id& b, DayNumber day);
bool duel_agreed(const CombatState& s, const Id& a, const Id& b, DayNumber day);
// Who struck first between a and b on `day` ("" = no hostility that day).
Id first_aggressor(const CombatState& s, const Id& a, const Id& b, DayNumber day);
void set_outlaw(CombatState& s, const Id& actor, bool outlaw);

// Marks the actor dead (health 0) and records it. No-op if already dead.
void mark_dead(CombatState& s, const Id& actor, const Id& killer, const std::string& cause,
               DayNumber day);

// --- combat styles (db/canon/combat_styles.csv) ---------------------------------------------
struct CombatStyle {
    Id id;
    Id faction;
    std::string role;
    Id weapon, shield;
    std::vector<Id> armour;
    int skill = 15, morale = 50, flee_at = 30, surrender_at = 15, favoured_zone = -1;
};
std::optional<CombatStyle> combat_style(const Db& db, const Id& style_id);
// The style for an npc: a row whose role matches (case-insensitive), else
// one whose faction matches, else "commoner".
Id style_for(const Db& db, const Id& faction, const std::string& role);
// Inputs a style gives (weapon/shield skills = style skill, dodge = skill-10).
CombatInputs style_inputs(const CombatStyle& st);
// Sets the combatant's style and morale and equips the kit (no inventory
// check — CombatActions gives the kit first). False for an unknown style.
bool apply_style(const Db& db, CombatState& s, const Id& actor, const Id& style_id);

// --- tuning (INVENTED; see module_Combat.md) --------------------------------------------------
constexpr int kBreathStamina = 5;

}  // namespace sim
