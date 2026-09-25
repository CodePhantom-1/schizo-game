#pragma once
// Progression.hpp — W4-A: the character verbs in the world. A WorldState&
// verb layer like sim/Actions.hpp and sim/Rites.hpp: it sequences the pure
// sheet functions of sim/Character.hpp with the modules' own free functions
// (Property's purse, Economy's stock, Needs, Population lookups). No module
// contract changes; contract: kernel/contracts/module_Character.md.
//
// How a skill grows (mechanics.md row 21, "by use/teachers/texts"):
//   USE      note_use(): every skill whose skills.csv grows_by lists the token.
//            Wired callers: crafting (station:<recipe station>, CApi
//            sim_world_craft), eating (verb:eat, CApi sim_world_eat), rites
//            (rite:<tradition>, Rites.cpp), trade (verb:trade / verb:sell,
//            buy/sell below), reading (verb:read, study_text), an unseen
//            crime (verb:crime_unseen, Actions.cpp commit_crime), and work
//            (work_roles.csv). Hooks for tracks still being built:
//            note_skill_use(w, actor, weapon_skill, xp) from combat (W4-B),
//            note_use(w, actor, "verb:treat" | "verb:hunt" | "verb:fish" |
//            "verb:travel_by_boat" | "verb:travel_wild", xp) (W4-B / W4-C).
//   TEACHERS train_with_teacher(): a resident whose role skill_teachings.csv
//            lists for the skill, at its fee per hour, up to the teacher's own
//            skill (the NPC's light sheet).
//   TEXTS    study_text(): a text item held, up to the row's max_level;
//            reading needs kLiteracySkill scribal arts (row 25).
//   PRACTICE practice_skill(): alone, any skill, but only up to kPracticeCap.
//
// Actors: "player" carries the full CharacterState; an npc carries a light
// NpcSheet (seeded from its role: a teacher knows what it teaches). Only the
// player's sheet grows here — NPC sheets are read-only to these verbs.
//
// Refusals change no state at all. Determinism (D-022): integer maths only,
// no randomness, no wall clock. Open play (D-021): nothing reads act, story or
// quest state.
#include "sim/World.hpp"

#include <string>
#include <vector>

namespace sim {

inline const Id kPlayerActor = "player";

// INVENTED numbers (retunable; docs/proposals/invented-ledger-character.md).
constexpr int kPracticeXpPerHour = 2;
constexpr int kPracticeCap = 25;        // practice alone takes a skill this far
constexpr int kTeacherXpPerHour = 5;
constexpr int kStudyXpPerHour = 4;
constexpr int kReadingXpPerHour = 1;    // scribal arts grow from any reading
constexpr int kLiteracySkill = 10;      // scribal arts needed to read a text
constexpr int kWorkXpPerHour = 2;       // per skill the work lists
constexpr int kCraftXpPerHour = 3;
constexpr int kEatXp = 1;
constexpr int kRiteXp = 4;              // a performed rite, success or failure
constexpr int kRiteSuccessXp = 4;       // extra on success
constexpr int kTradeXpBase = 2;
constexpr int kTradeXpPerSilver = 20;   // +1 xp per this much silver changing hands
constexpr int kTradeXpMax = 10;
constexpr int kStealthXp = 6;
constexpr int kMaxSessionHours = 12;
constexpr int kBargainBpPerPoint = 10;  // buy: -0.1% per effective bargaining point
constexpr int kSellBaseBp = 5000;       // the market buys at half its price...
constexpr int kSellBpPerPoint = 20;     // ...+0.2% per bargaining point
constexpr int kRankTradeBpPerTier = 100;  // +1% per rank tier with the market's polity
// Standing (0..100) a polity must hold for the player before a patron can
// raise him to each tier (index = tier). Tier 6 (Lugal) needs no patron.
constexpr int kRankStanding[7] = {0, 10, 25, 45, 60, 75, 95};

struct ProgressResult {
    bool ok = false;
    std::string refusal;   // "" when ok
    SkillGain gain;
    Silver silver = 0;     // fee paid / wage or trade silver
    int items = 0;         // items paid as wages / traded
};

// --- lookups ----------------------------------------------------------------
// 1..10 / 0..100 for "player" or any npc; -1 for an unknown actor, attribute
// or skill.
int actor_attribute(const WorldState& w, const Id& actor, const Id& attr);
int actor_skill(const WorldState& w, const Id& actor, const Id& skill);
int actor_effective_skill(const WorldState& w, const Id& actor, const Id& skill);

// --- use ---------------------------------------------------------------------
// Grows one skill of the actor's sheet by use (player only; npc: no-op).
SkillGain note_skill_use(WorldState& w, const Id& actor, const Id& skill, int xp);
// Grows every skill whose grows_by lists `token` (player only).
SkillGain note_use(WorldState& w, const Id& actor, const std::string& token, int xp);
// A successful craft: kCraftXpPerHour x recipe hours x times, to the recipe
// station's skills.
SkillGain note_craft(WorldState& w, const Id& actor, const Id& recipe, int times);
// A performed rite (the performer is the player).
SkillGain note_rite(WorldState& w, const Id& rite, bool succeeded);

// --- practice, teachers, texts, work -----------------------------------------
// Refusals: unknown_skill | bad_hours | exhausted | practice_capped
ProgressResult practice_skill(WorldState& w, const Id& skill, int hours);
// Refusals: unknown_skill | bad_hours | unknown_teacher | not_taught_by_teacher |
//   teacher_surpassed | exhausted | cannot_afford
ProgressResult train_with_teacher(WorldState& w, const Id& skill, const Id& teacher, int hours);
// Refusals: unknown_skill | bad_hours | not_taught_in_text | text_not_held |
//   cannot_read | text_exhausted | exhausted
ProgressResult study_text(WorldState& w, const Id& skill, const Id& item, int hours);
// Refusals: bad_hours | unknown_employer | not_an_employer | skill_too_low | exhausted
ProgressResult work_for(WorldState& w, const Id& employer, int hours);

struct Teaching {
    Id skill;
    Id source;          // npc id (teacher) or item id (text)
    int max_level = 0;
    Silver fee_per_hour = 0;
};
std::vector<Teaching> skills_taught_by(const WorldState& w, const Id& npc);   // by skill id
std::vector<Teaching> teachers_of(const WorldState& w, const Id& skill);      // by npc id
std::vector<Teaching> skills_taught_in(const Db& db, const Id& item);
// The fee the player would pay per hour to this teacher for this skill (after
// fee_bp); -1 when the npc does not teach it.
Silver teacher_fee(const WorldState& w, const Id& skill, const Id& teacher);

// --- trade -------------------------------------------------------------------
// Buys qty from the city's market into the actor's inventory, paid from the
// actor's purse; sells the other way. Prices: Economy's price_of, bettered by
// bargaining, trade_bp talents/perks and rank with the city's polity.
// Refusals: bad_quantity | unknown_market | not_sold_here | market_closed |
//   out_of_stock | cannot_afford (buy) | not_held (sell)
ProgressResult buy_from_market(WorldState& w, const Id& actor, const Id& city, const Id& item, int qty);
ProgressResult sell_to_market(WorldState& w, const Id& actor, const Id& city, const Id& item, int qty);
// Total silver for the deal, or -1 when the market does not trade the item.
Silver quote_buy(const WorldState& w, const Id& actor, const Id& city, const Id& item, int qty);
Silver quote_sell(const WorldState& w, const Id& actor, const Id& city, const Id& item, int qty);

// --- rank (row 24) -------------------------------------------------------------
// Raises the player one tier with the polity: deeds (standing >=
// kRankStanding[tier]) + a patron's act (a resident of that polity) for tiers
// 1..5; tier 6 needs only the standing. Refusals: unknown_polity | outlawed |
// at_highest_rank | standing_too_low | unknown_patron | patron_not_of_polity
ProgressResult raise_rank(WorldState& w, const Id& polity, const Id& patron);
// Outlawry: rank 0 with the polity (Actions.cpp, on exile/death).
void strip_rank(WorldState& w, const Id& actor, const Id& polity);

// --- init --------------------------------------------------------------------
// Seeds every npc's light sheet from its role (teachings: the teacher's skill
// is the row's max_level; work: a worker knows its trade at kWorkerSkill).
constexpr int kWorkerSkill = 30;
void seed_npc_sheets(WorldState& w);

}  // namespace sim
