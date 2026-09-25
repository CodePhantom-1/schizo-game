#pragma once
// Character.hpp — W4-A: character progression (mechanics.md rows 20-25;
// contract: kernel/contracts/module_Character.md).
//
//   row 20  six attributes, range 1..10
//   row 21  skills in six groups, 0..100, grown by use / teachers / texts
//   row 22  levels: XP from skill points, cap 40, one talent per level
//   row 23  callings: choose at 5, specialise at 15, second calling at 25,
//           +50% growth for the calling's skills
//   row 24  social rank per polity (0 the Outsider .. 6 Lugal; ranks.csv)
//
// This header is the PURE part: the catalog read from canon (attributes,
// skills, callings, talents) and the character sheet with the functions that
// change it. Nothing here touches WorldState — the world verbs (practice,
// teachers, texts, work, trade, rank, the use hooks) live in
// sim/Progression.hpp.
//
// Determinism (D-022): integer maths only. Growth multipliers are basis
// points (10000 = x1; the calling's +50% is +5000 bp); skill progress is
// held in xp*bp units so no fraction is ever rounded away. No randomness, no
// wall clock, no static mutable state.
//
// Open play (D-021): nothing here reads act, story or quest state. Every gate
// is a number the player earns in play: level (from skill points), skill,
// talent points.
//
// Skill decay: none. The imported spec (mechanics.md row 21: "by
// use/teachers/texts") names no decay, so none is invented.
#include "sim/Db.hpp"
#include "sim/Types.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace sim {

// --- the imported numbers (mechanics.md rows 20-23) ---------------------------
constexpr int kAttrMin = 1;
constexpr int kAttrMax = 10;
constexpr int kSkillMax = 100;
constexpr int kLevelCap = 40;
constexpr int kFirstCallingLevel = 5;
constexpr int kSpecialisationLevel = 15;
constexpr int kSecondCallingLevel = 25;
constexpr int kBp = 10000;                // basis points: x1
constexpr int kCallingGrowthBp = 5000;    // "+50% growth"

// --- INVENTED numbers (retunable; docs/proposals/invented-ledger-character.md)
constexpr int kAttrStart = 5;             // every attribute starts mid-range
constexpr int kAttrGrowthBpPerPoint = 1000;  // governing attribute: +/-10% growth per point from 5
constexpr int kMinGrowthBp = 1000;        // growth never falls below x0.1
constexpr int kAttrExercisePerStep = 12;  // attribute rises after 12 x (next value) skill points under it

// Skill-point cost: xp needed to raise a skill from `value` to value+1.
int skill_point_cost(int value);           // 5 + value/2
// Level cost: skill points needed to go from `level` to level+1.
int level_cost(int level);                 // 5 + level
// Cumulative skill points needed to stand at `level` (level 1 = 0).
int xp_for_level(int level);

// --- effects: the grammar talents and specialisation perks share --------------
//   skill_bonus:<skill>:<n>   +n to the effective skill (clamped 0..100)
//   growth_bp:<skill>:<n>     +n bp growth for that skill
//   attribute:<attr>:<n>      +n to the attribute (clamped 1..10)
//   trade_bp:<n>              better prices at market (buy cheaper / sell dearer)
//   wage_bp:<n>               more wages from work
//   fee_bp:<n>                teacher fees (negative = cheaper)
//   study_bp:<n>              more xp from reading texts
//   rite_favour_bp:<n>        more favour from a successful offering / hymn
struct Effect {
    std::string kind;
    Id target;       // skill or attribute id ("" for the scalar kinds)
    int amount = 0;
};
// Parses a semicolon list; false (and `out` untouched) on any malformed entry.
bool parse_effects(const std::string& text, std::vector<Effect>& out);

// --- the catalog (canon; rebuilt at init, never saved) -------------------------
struct SkillDef {
    Id id;
    std::string name;
    Id group;                            // one of the six groups
    Id attribute;                        // the governing attribute
    std::vector<std::string> grows_by;   // use tokens (station:quern, verb:trade, rite:hymns…)
};

struct CallingDef {
    Id id;
    std::string name;
    bool specialisation = false;
    Id parent;                           // the calling a specialisation belongs to
    std::vector<Id> skills;              // favoured: +50% growth
    std::vector<Effect> perks;           // a specialisation's perks
};

struct TalentDef {
    Id id;
    std::string name;
    Id calling;                          // "" = any; else a calling or specialisation held
    Id skill;                            // prerequisite skill ("" = none)
    int min_level = 1;
    int min_skill = 0;
    std::vector<Effect> effects;
};

struct ProgressionCatalog {
    std::vector<Id> attributes;              // attributes.csv order
    std::map<Id, SkillDef> skills;
    std::map<Id, CallingDef> callings;       // callings and specialisations
    std::map<Id, TalentDef> talents;
};

// Every non-OPEN row of attributes/skills/callings/talents. A row whose effect
// text does not parse is skipped (tests assert every canon row parses).
ProgressionCatalog load_progression(const Db& db);

// --- the sheets ------------------------------------------------------------
struct SkillProgress {
    int value = 0;                   // 0..100
    std::int64_t progress = 0;       // xp*bp toward the next point
};

// Recomputed from talents + callings by refresh_derived(); never saved.
struct CharacterDerived {
    std::map<Id, int> skill_bonus;
    std::map<Id, int> growth_bp;
    std::map<Id, int> attr_bonus;
    std::set<Id> favoured;           // skills of every calling/specialisation held
    int trade_bp = 0;
    int wage_bp = 0;
    int fee_bp = 0;
    int study_bp = 0;
    int rite_favour_bp = 0;
};

// The player's full sheet.
struct CharacterState {
    std::map<Id, int> attributes;        // base 1..10 (every catalog attribute present)
    std::map<Id, int> attr_exercise;     // skill points earned under the attribute since its last rise
    std::map<Id, SkillProgress> skills;  // sparse: an absent skill is 0
    int level = 1;
    int xp = 0;                          // skill points earned, ever
    int talent_points = 0;               // unspent
    std::vector<Id> talents;             // in the order chosen
    Id calling;                          // first calling (level 5)
    Id specialisation;                   // of the first calling (level 15)
    Id second_calling;                   // (level 25)
    std::map<Id, int> rank_by_polity;    // factions.csv id -> tier 0..6 (absent = 0)
    CharacterDerived derived;
};

// The light sheet an NPC carries: values only — no progress, no talents.
struct NpcSheet {
    std::map<Id, int> attributes;    // absent = kAttrStart
    std::map<Id, int> skills;        // absent = 0
};

CharacterState new_character(const ProgressionCatalog& cat);
void refresh_derived(const ProgressionCatalog& cat, CharacterState& c);

// --- reads (W4-B takes these as plain inputs) --------------------------------
// attribute: base + talent/perk bonus, clamped 1..10; 0 for an attribute id
// the sheet does not carry.
int attribute(const CharacterState& c, const Id& attr);
int attribute(const NpcSheet& s, const Id& attr);
int skill_value(const CharacterState& c, const Id& skill);   // base 0..100
// effective_skill: base + talent/perk bonus, clamped 0..100.
int effective_skill(const CharacterState& c, const Id& skill);
int effective_skill(const NpcSheet& s, const Id& skill);
int npc_level(const NpcSheet& s);    // 1 + (sum of skills)/25, capped at 40
bool holds_calling(const CharacterState& c, const Id& calling_or_spec);
int rank(const CharacterState& c, const Id& polity);
// Growth multiplier (bp) for one skill: 10000 + governing attribute
// (+/-1000 per point from 5) + 5000 if favoured + talent/perk growth_bp;
// never below kMinGrowthBp.
int growth_bp(const ProgressionCatalog& cat, const CharacterState& c, const Id& skill);

// --- growth ----------------------------------------------------------------
struct SkillGain {
    int points = 0;                      // skill points gained (sum across skills)
    int levels = 0;                      // levels gained
    std::vector<Id> attributes_raised;   // attributes that rose by exercise
    void add(const SkillGain& o);
};

// Adds `xp` (scaled by growth_bp) to the skill; raises it while progress
// covers skill_point_cost, never past `cap` (a teacher's or text's limit, or
// kSkillMax). At the cap the leftover progress is dropped. Every point is one
// character xp (row 22) and one exercise of the governing attribute.
// Unknown skill, xp <= 0, or already at cap: no change.
SkillGain gain_skill_xp(const ProgressionCatalog& cat, CharacterState& c, const Id& skill, int xp,
                        int cap = kSkillMax);

// Applies every level the xp has earned (one talent point each), up to the
// cap. Called by gain_skill_xp; exposed for tests. Returns levels gained.
int apply_levels(CharacterState& c);

// --- choices (refusal "" = done) --------------------------------------------
// choose_talent: unknown_talent | already_taken | no_talent_point |
//   level_too_low | calling_required | skill_too_low
std::string choose_talent(const ProgressionCatalog& cat, CharacterState& c, const Id& talent);
// Why the talent cannot be chosen now ("" = it can).
std::string talent_refusal(const ProgressionCatalog& cat, const CharacterState& c, const Id& talent);
std::vector<Id> available_talents(const ProgressionCatalog& cat, const CharacterState& c);
// choose_calling: unknown_calling | not_a_calling | level_too_low | already_chosen
std::string choose_calling(const ProgressionCatalog& cat, CharacterState& c, const Id& calling);
// choose_specialisation: unknown_calling | not_a_specialisation | level_too_low |
//   no_calling | not_of_your_calling | already_chosen
std::string choose_specialisation(const ProgressionCatalog& cat, CharacterState& c, const Id& spec);
// choose_second_calling: unknown_calling | not_a_calling | level_too_low |
//   no_calling | same_calling | already_chosen
std::string choose_second_calling(const ProgressionCatalog& cat, CharacterState& c, const Id& calling);

// A readable, deterministic, multi-line summary for the UI.
std::string character_summary(const ProgressionCatalog& cat, const CharacterState& c);

}  // namespace sim
