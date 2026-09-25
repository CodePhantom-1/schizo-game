// test_character.cpp — W4-A: character progression (sim/Character.hpp, the
// pure sheet) and its world verbs (sim/Progression.hpp): canon catalog, growth
// maths, levels, talents, callings, attributes, teachers, texts, work, trade,
// rank, the use hooks (craft, eat, rite, unseen crime), Snapshot round trip
// and determinism.
#include "sim/Actions.hpp"
#include "sim/CApi.h"
#include "sim/Character.hpp"
#include "sim/Progression.hpp"
#include "sim/Rites.hpp"
#include "sim/Schedule.hpp"
#include "sim/Snapshot.hpp"
#include "sim/Test.hpp"
#include "sim/World.hpp"

#include <algorithm>
#include <stdexcept>
#include <set>
#include <string>
#include <vector>

using namespace sim;

namespace {

constexpr std::uint64_t kSeed = 4242;
const char* kCity = "city_of_the_moon";

// The first npc whose role is `role` (the D-020 street has one for every role).
Id npc_with_role(const WorldState& w, const std::string& role) {
    for (const Npc& n : w.population.npcs)
        if (n.role == role) return n.id;
    return "";
}

// A day (>= 2) on which the market trades.
DayNumber open_market_day(const WorldState& w) {
    for (DayNumber d = 2; d < 400; ++d)
        if (market_open(w.db, w.cal, d)) return d;
    return 0;
}

}  // namespace

// --- canon ---------------------------------------------------------------------

// Six attributes, six groups; every row parses; every reference resolves.
static bool test_catalog_is_whole() {
    WorldState w;
    w.init("../db/canon", kSeed);
    const ProgressionCatalog& cat = w.progression;
    SIM_CHECK_EQ(cat.attributes.size(), std::size_t{6});
    std::set<Id> groups;
    for (const auto& [id, s] : cat.skills) {
        groups.insert(s.group);
        SIM_CHECK(std::find(cat.attributes.begin(), cat.attributes.end(), s.attribute) != cat.attributes.end());
    }
    SIM_CHECK_EQ(groups.size(), std::size_t{6});
    SIM_CHECK(groups.count("sacred") == 1);  // row 21: the Sacred group
    for (const char* sacred : {"rites", "divination", "incantation"}) SIM_CHECK(cat.skills.count(sacred) == 1);

    // Every live canon row made it into the catalog (a row whose effect text
    // fails to parse would be silently skipped).
    for (const char* table : {"skills", "callings", "talents"}) {
        std::size_t live = 0;
        for (const Row& r : w.db.rows(table))
            if (r.get("tag") != "OPEN") ++live;
        const std::size_t loaded = std::string(table) == "skills"   ? cat.skills.size()
                                   : std::string(table) == "callings" ? cat.callings.size()
                                                                      : cat.talents.size();
        SIM_CHECK_EQ(loaded, live);
    }

    auto target_ok = [&](const Effect& e) {
        if (e.kind == "skill_bonus" || e.kind == "growth_bp") return cat.skills.count(e.target) == 1;
        if (e.kind == "attribute")
            return std::find(cat.attributes.begin(), cat.attributes.end(), e.target) != cat.attributes.end();
        return e.target.empty();
    };
    int callings = 0;
    for (const auto& [id, c] : cat.callings) {
        for (const Id& s : c.skills) SIM_CHECK(cat.skills.count(s) == 1);
        for (const Effect& e : c.perks) SIM_CHECK(target_ok(e));
        if (c.specialisation) {
            const auto parent = cat.callings.find(c.parent);
            SIM_CHECK(parent != cat.callings.end() && !parent->second.specialisation);
            SIM_CHECK(!c.perks.empty());  // a specialisation always has a real perk
        } else {
            ++callings;
            SIM_CHECK(c.parent.empty());
        }
    }
    SIM_CHECK(callings >= 2);  // a second calling must be possible
    for (const Row& r : w.db.rows("callings")) {  // the specialisations column matches the rows
        for (const Id& spec : [&] {
                 std::vector<Id> v;
                 std::string cur;
                 for (const char ch : r.get("specialisations") + ";") {
                     if (ch == ';') { if (!cur.empty()) v.push_back(cur); cur.clear(); }
                     else cur.push_back(ch);
                 }
                 return v;
             }())
            SIM_CHECK(cat.callings.at(spec).parent == r.at("id"));
    }
    for (const auto& [id, t] : cat.talents) {
        SIM_CHECK(!t.effects.empty());  // every talent does something
        for (const Effect& e : t.effects) SIM_CHECK(target_ok(e));
        if (!t.calling.empty()) SIM_CHECK(cat.callings.count(t.calling) == 1);
        if (!t.skill.empty()) SIM_CHECK(cat.skills.count(t.skill) == 1);
        SIM_CHECK(t.min_level >= 1 && t.min_level <= kLevelCap);
        SIM_CHECK(t.min_skill >= 0 && t.min_skill <= kSkillMax);
    }
    return true;
}

// Every skill has a real use path in the kernel (a teacher on the street, a
// work shift, or a wired use token) and every grows_by token is either wired
// or one of the documented hooks for the tracks still being built.
static bool test_every_skill_has_a_use_path() {
    WorldState w;
    w.init("../db/canon", kSeed);
    const std::set<std::string> wired_verbs = {"verb:trade", "verb:sell", "verb:eat", "verb:read",
                                               "verb:crime_unseen"};
    const std::set<std::string> hooks = {"verb:combat", "verb:treat", "verb:hunt", "verb:fish",
                                         "verb:travel_by_boat", "verb:travel_wild"};
    std::set<std::string> stations;
    for (const Recipe& r : load_recipes(w.db)) stations.insert("station:" + r.station);
    std::set<std::string> traditions;
    for (const Row& r : w.db.rows("rites")) {
        std::string t = r.get("tradition");
        for (char& c : t) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        traditions.insert("rite:" + t);
    }
    for (const auto& [id, def] : w.progression.skills) {
        bool wired = false;
        for (const std::string& tok : def.grows_by) {
            const bool is_wired = wired_verbs.count(tok) || stations.count(tok) || traditions.count(tok);
            SIM_CHECK(is_wired || hooks.count(tok) == 1);
            wired = wired || is_wired;
        }
        const bool taught = !teachers_of(w, id).empty();
        bool worked = false;
        for (const Row& r : w.db.rows("work_roles"))
            if (("," + r.get("skills") + ",").find(id) != std::string::npos &&
                !npc_with_role(w, r.get("role")).empty())
                worked = true;
        if (!(wired || taught || worked)) std::fprintf(stderr, "skill without a use path: %s\n", id.c_str());
        SIM_CHECK(wired || taught || worked);
    }
    return true;
}

// --- the pure sheet --------------------------------------------------------------

static bool test_fresh_prisoner_and_growth_maths() {
    WorldState w;
    w.init("../db/canon", kSeed);
    CharacterState& c = w.character;
    SIM_CHECK_EQ(c.level, 1);
    SIM_CHECK_EQ(c.xp, 0);
    SIM_CHECK_EQ(c.talent_points, 0);
    for (const Id& a : w.progression.attributes) SIM_CHECK_EQ(attribute(c, a), kAttrStart);
    for (const auto& [id, s] : w.progression.skills) SIM_CHECK_EQ(effective_skill(c, id), 0);
    SIM_CHECK_EQ(attribute(c, "no_such_attr"), 0);

    // The curves.
    SIM_CHECK_EQ(skill_point_cost(0), 5);
    SIM_CHECK_EQ(skill_point_cost(50), 30);
    SIM_CHECK_EQ(xp_for_level(1), 0);
    SIM_CHECK_EQ(xp_for_level(2), 6);
    SIM_CHECK_EQ(xp_for_level(5), 6 + 7 + 8 + 9);

    // Growth in basis points (D-022): x1 at attribute 5, +50% favoured.
    SIM_CHECK_EQ(growth_bp(w.progression, c, "cooking"), kBp);
    const SkillGain g = gain_skill_xp(w.progression, c, "cooking", 5);  // exactly one point
    SIM_CHECK_EQ(g.points, 1);
    SIM_CHECK_EQ(skill_value(c, "cooking"), 1);
    SIM_CHECK_EQ(c.xp, 1);
    SIM_CHECK_EQ(c.skills.at("cooking").progress, 0);
    // A cap stops growth and drops the leftover.
    const SkillGain capped = gain_skill_xp(w.progression, c, "cooking", 10000, 3);
    SIM_CHECK_EQ(capped.points, 2);
    SIM_CHECK_EQ(skill_value(c, "cooking"), 3);
    SIM_CHECK_EQ(c.skills.at("cooking").progress, 0);
    SIM_CHECK_EQ(gain_skill_xp(w.progression, c, "cooking", 10000, 3).points, 0);
    SIM_CHECK_EQ(gain_skill_xp(w.progression, c, "no_such_skill", 100).points, 0);
    SIM_CHECK_EQ(gain_skill_xp(w.progression, c, "cooking", 0).points, 0);
    return true;
}

// Levels come from skill points (one talent point each); callings open at
// 5 / 15 / 25 and add +50% growth; talents need their level, calling, skill
// and a point.
static bool test_levels_callings_and_talents() {
    WorldState w;
    w.init("../db/canon", kSeed);
    const ProgressionCatalog& cat = w.progression;
    CharacterState& c = w.character;

    SIM_CHECK_EQ(choose_calling(cat, c, "priest"), std::string("level_too_low"));
    c.xp = xp_for_level(5) - 1;
    SIM_CHECK_EQ(apply_levels(c), 3);
    SIM_CHECK_EQ(c.level, 4);
    SIM_CHECK_EQ(c.talent_points, 3);
    SIM_CHECK_EQ(choose_calling(cat, c, "priest"), std::string("level_too_low"));
    c.xp += 1;
    SIM_CHECK_EQ(apply_levels(c), 1);
    SIM_CHECK_EQ(choose_calling(cat, c, "exorcist"), std::string("not_a_calling"));
    SIM_CHECK_EQ(choose_calling(cat, c, "nobody"), std::string("unknown_calling"));
    SIM_CHECK_EQ(choose_calling(cat, c, "priest"), std::string(""));
    SIM_CHECK_EQ(choose_calling(cat, c, "healer"), std::string("already_chosen"));
    SIM_CHECK_EQ(growth_bp(cat, c, "rites"), kBp + kCallingGrowthBp);  // "+50% growth"
    SIM_CHECK_EQ(growth_bp(cat, c, "cooking"), kBp);

    SIM_CHECK_EQ(choose_specialisation(cat, c, "exorcist"), std::string("level_too_low"));
    c.xp = xp_for_level(15);
    apply_levels(c);
    SIM_CHECK_EQ(choose_specialisation(cat, c, "priest"), std::string("not_a_specialisation"));
    SIM_CHECK_EQ(choose_specialisation(cat, c, "physician"), std::string("not_of_your_calling"));
    SIM_CHECK_EQ(choose_specialisation(cat, c, "exorcist"), std::string(""));
    SIM_CHECK_EQ(effective_skill(c, "incantation"), 10);  // the exorcist's perk
    SIM_CHECK_EQ(growth_bp(cat, c, "medicine"), kBp + kCallingGrowthBp);

    SIM_CHECK_EQ(choose_second_calling(cat, c, "healer"), std::string("level_too_low"));
    c.xp = xp_for_level(25);
    apply_levels(c);
    SIM_CHECK_EQ(choose_second_calling(cat, c, "priest"), std::string("same_calling"));
    SIM_CHECK_EQ(choose_second_calling(cat, c, "healer"), std::string(""));
    SIM_CHECK_EQ(growth_bp(cat, c, "medicine"), kBp + kCallingGrowthBp);  // not stacked twice

    // Talents.
    SIM_CHECK_EQ(c.talent_points, 24);
    SIM_CHECK_EQ(choose_talent(cat, c, "true_names"), std::string("skill_too_low"));
    c.skills["rites"].value = 40;
    SIM_CHECK_EQ(choose_talent(cat, c, "true_names"), std::string(""));
    SIM_CHECK_EQ(effective_skill(c, "rites"), 50);
    SIM_CHECK_EQ(c.derived.rite_favour_bp, 5000);
    SIM_CHECK_EQ(choose_talent(cat, c, "true_names"), std::string("already_taken"));
    SIM_CHECK_EQ(choose_talent(cat, c, "shield_wall"), std::string("calling_required"));
    SIM_CHECK_EQ(choose_talent(cat, c, "broad_shoulders"), std::string(""));
    SIM_CHECK_EQ(attribute(c, "strength"), 6);
    SIM_CHECK_EQ(c.attributes.at("strength"), 5);  // the base is untouched
    SIM_CHECK_EQ(c.talent_points, 22);
    c.talent_points = 0;
    SIM_CHECK_EQ(choose_talent(cat, c, "quick_hands"), std::string("no_talent_point"));
    const std::vector<Id> avail = available_talents(cat, c);
    SIM_CHECK(avail.empty());

    // Level cap 40.
    c.xp = 100000;
    apply_levels(c);
    SIM_CHECK_EQ(c.level, kLevelCap);
    return true;
}

// Attribute growth by exercise: skill points under an attribute raise it,
// never past 10; a talent adds on top, clamped.
static bool test_attribute_growth_by_exercise() {
    WorldState w;
    w.init("../db/canon", kSeed);
    CharacterState& c = w.character;
    // cooking is governed by wits: 12 x 6 = 72 points take wits 5 -> 6.
    SkillGain total;
    while (skill_value(c, "cooking") < 71) total.add(gain_skill_xp(w.progression, c, "cooking", 5));
    SIM_CHECK_EQ(c.attributes.at("wits"), 5);
    while (c.attributes.at("wits") == 5) total.add(gain_skill_xp(w.progression, c, "cooking", 5));
    SIM_CHECK_EQ(skill_value(c, "cooking"), 72);
    SIM_CHECK_EQ(total.attributes_raised, std::vector<Id>{"wits"});
    SIM_CHECK(growth_bp(w.progression, c, "cooking") == kBp + kAttrGrowthBpPerPoint);
    c.attributes["wits"] = kAttrMax;
    c.talents.push_back("sharp_wits");
    refresh_derived(w.progression, c);
    SIM_CHECK_EQ(attribute(c, "wits"), kAttrMax);  // clamped
    return true;
}

// No decay (the imported spec names none): time alone never lowers a skill.
static bool test_no_skill_decay() {
    WorldState w;
    w.init("../db/canon", kSeed);
    (void)gain_skill_xp(w.progression, w.character, "farming", 500);
    const int before = skill_value(w.character, "farming");
    SIM_CHECK(before > 0);
    w.advance_days(400);
    SIM_CHECK_EQ(skill_value(w.character, "farming"), before);
    return true;
}

// --- the world verbs ---------------------------------------------------------------

static bool test_practice_caps_and_tires() {
    WorldState w;
    w.init("../db/canon", kSeed);
    SIM_CHECK_EQ(practice_skill(w, "spear", 0).refusal, std::string("bad_hours"));
    SIM_CHECK_EQ(practice_skill(w, "spear", 13).refusal, std::string("bad_hours"));
    SIM_CHECK_EQ(practice_skill(w, "nope", 4).refusal, std::string("unknown_skill"));
    const ProgressResult r = practice_skill(w, "spear", 8);
    SIM_CHECK(r.ok);
    SIM_CHECK(r.gain.points > 0);
    SIM_CHECK_EQ(w.needs.by_actor.at("player").fatigue, 32);  // 8 waking hours
    (void)practice_skill(w, "spear", 8);
    (void)practice_skill(w, "spear", 8);  // 96 fatigue: exhausted
    SIM_CHECK_EQ(practice_skill(w, "spear", 1).refusal, std::string("exhausted"));
    advance_needs(w.needs, "player", 12, true);
    for (int i = 0; i < 200 && skill_value(w.character, "spear") < kPracticeCap; ++i) {
        advance_needs(w.needs, "player", 12, true);
        (void)practice_skill(w, "spear", 8);
    }
    SIM_CHECK_EQ(skill_value(w.character, "spear"), kPracticeCap);
    advance_needs(w.needs, "player", 12, true);
    const std::string before = save_world(w);
    SIM_CHECK_EQ(practice_skill(w, "spear", 4).refusal, std::string("practice_capped"));
    SIM_CHECK_EQ(save_world(w), before);  // a refusal changes nothing
    return true;
}

static bool test_teachers_charge_and_cap() {
    WorldState w;
    w.init("../db/canon", kSeed);
    const Id baker = npc_with_role(w, "baker");
    const Id watchman = npc_with_role(w, "watchman");
    SIM_CHECK(!baker.empty() && !watchman.empty());
    // The baker's light sheet: he knows what he teaches.
    SIM_CHECK_EQ(actor_effective_skill(w, baker, "cooking"), 60);
    SIM_CHECK_EQ(actor_attribute(w, baker, "strength"), kAttrStart);
    SIM_CHECK_EQ(actor_skill(w, "nobody_at_all", "cooking"), -1);

    SIM_CHECK_EQ(train_with_teacher(w, "cooking", "nobody", 4).refusal, std::string("unknown_teacher"));
    SIM_CHECK_EQ(train_with_teacher(w, "spear", baker, 4).refusal, std::string("not_taught_by_teacher"));
    SIM_CHECK_EQ(train_with_teacher(w, "cooking", baker, 4).refusal, std::string("cannot_afford"));
    const Silver fee = teacher_fee(w, "cooking", baker);
    SIM_CHECK(fee > 0);
    credit_purse(w.property, "player", 1000);
    const Silver teacher_before = purse(w.property, baker);
    const ProgressResult r = train_with_teacher(w, "cooking", baker, 8);
    SIM_CHECK(r.ok);
    SIM_CHECK_EQ(r.silver, fee * 8);
    SIM_CHECK_EQ(purse(w.property, "player"), 1000 - fee * 8);
    SIM_CHECK_EQ(purse(w.property, baker), teacher_before + fee * 8);  // the teacher is paid
    SIM_CHECK(skill_value(w.character, "cooking") > 0);

    // Up to the teacher's own skill, no further.
    w.character.skills["cooking"].value = 59;
    advance_needs(w.needs, "player", 12, true);
    for (int i = 0; i < 20; ++i) { advance_needs(w.needs, "player", 12, true); (void)train_with_teacher(w, "cooking", baker, 8); }
    SIM_CHECK_EQ(skill_value(w.character, "cooking"), 60);
    SIM_CHECK_EQ(train_with_teacher(w, "cooking", baker, 4).refusal, std::string("teacher_surpassed"));

    // The apt pupil pays less.
    w.character.talents.push_back("apt_pupil");
    refresh_derived(w.progression, w.character);
    SIM_CHECK(teacher_fee(w, "spear", watchman) < 2);
    SIM_CHECK(!teachers_of(w, "spear").empty());
    return true;
}

static bool test_texts_need_literacy() {
    WorldState w;
    w.init("../db/canon", kSeed);
    SIM_CHECK_EQ(study_text(w, "divination", "clay_liver_model", 4).refusal, std::string("text_not_held"));
    w.inventories["player"].counts["clay_liver_model"] = 1;
    SIM_CHECK_EQ(study_text(w, "rites", "clay_liver_model", 4).refusal, std::string("not_taught_in_text"));
    SIM_CHECK_EQ(study_text(w, "divination", "clay_liver_model", 4).refusal, std::string("cannot_read"));
    w.character.skills["scribal_arts"].value = kLiteracySkill;
    const ProgressResult r = study_text(w, "divination", "clay_liver_model", 8);
    SIM_CHECK(r.ok);
    SIM_CHECK(skill_value(w.character, "divination") > 0);
    SIM_CHECK(w.character.skills.at("scribal_arts").progress > 0);  // reading is practice at reading
    SIM_CHECK_EQ(w.inventories["player"].counts["clay_liver_model"], 1);  // read, not consumed
    w.character.skills["divination"].value = 60;
    SIM_CHECK_EQ(study_text(w, "divination", "clay_liver_model", 4).refusal, std::string("text_exhausted"));
    return true;
}

// Work: the prisoner's first silver. Rations as wages; skills grow by use.
static bool test_work_pays_wages_and_grows_skills() {
    WorldState w;
    w.init("../db/canon", kSeed);
    const Id field_hand = npc_with_role(w, "field hand");
    const Id dock = npc_with_role(w, "dockworker");
    const Id scribe = npc_with_role(w, "scribe");
    const Id paladin = npc_with_role(w, "paladin");
    SIM_CHECK_EQ(work_for(w, "nobody", 4).refusal, std::string("unknown_employer"));
    SIM_CHECK_EQ(work_for(w, paladin, 4).refusal, std::string("not_an_employer"));
    SIM_CHECK_EQ(work_for(w, scribe, 4).refusal, std::string("skill_too_low"));  // must already read

    const ProgressResult grain = work_for(w, field_hand, 8);
    SIM_CHECK(grain.ok);
    SIM_CHECK_EQ(grain.items, 8);
    SIM_CHECK_EQ(w.inventories["player"].counts["grain"], 8);
    SIM_CHECK(skill_value(w.character, "farming") > 0);

    const ProgressResult silver = work_for(w, dock, 4);
    SIM_CHECK(silver.ok);
    SIM_CHECK_EQ(silver.silver, 4);
    SIM_CHECK_EQ(purse(w.property, "player"), 4);

    w.character.talents.push_back("hard_worker");  // wage_bp +2500
    refresh_derived(w.progression, w.character);
    advance_needs(w.needs, "player", 12, true);
    SIM_CHECK_EQ(work_for(w, dock, 8).silver, 10);
    return true;
}

static bool test_trade_moves_silver_goods_and_skill() {
    WorldState w;
    w.init("../db/canon", kSeed);
    SIM_CHECK(!market_open(w.db, w.cal, w.day));  // day 1: New Waters closes the market
    SIM_CHECK_EQ(buy_from_market(w, "player", kCity, "grain", 1).refusal, std::string("market_closed"));
    w.advance_days(static_cast<int>(open_market_day(w) - w.day));
    SIM_CHECK_EQ(buy_from_market(w, "player", "nowhere", "grain", 1).refusal, std::string("unknown_market"));
    SIM_CHECK_EQ(buy_from_market(w, "player", kCity, "quern", 1).refusal, std::string("not_sold_here"));
    SIM_CHECK_EQ(buy_from_market(w, "player", kCity, "grain", 0).refusal, std::string("bad_quantity"));
    SIM_CHECK_EQ(buy_from_market(w, "player", kCity, "grain", 5).refusal, std::string("cannot_afford"));
    SIM_CHECK_EQ(buy_from_market(w, "player", kCity, "bread", 1).refusal, std::string("out_of_stock"));
    SIM_CHECK_EQ(sell_to_market(w, "player", kCity, "grain", 1).refusal, std::string("not_held"));

    credit_purse(w.property, "player", 500);
    const std::int64_t stock = w.economy.stock_by_city_item.at(std::string(kCity) + "/grain");
    const Silver quote = quote_buy(w, "player", kCity, "grain", 10);
    const ProgressResult b = buy_from_market(w, "player", kCity, "grain", 10);
    SIM_CHECK(b.ok);
    SIM_CHECK_EQ(b.silver, quote);
    SIM_CHECK_EQ(purse(w.property, "player"), 500 - quote);
    SIM_CHECK_EQ(w.inventories["player"].counts["grain"], 10);
    SIM_CHECK_EQ(w.economy.stock_by_city_item.at(std::string(kCity) + "/grain"), stock - 10);
    SIM_CHECK(w.character.skills.at("bargaining").progress > 0 || skill_value(w.character, "bargaining") > 0);

    const Silver sq = quote_sell(w, "player", kCity, "grain", 10);
    SIM_CHECK(sq <= quote);  // no arbitrage against the market
    const ProgressResult s = sell_to_market(w, "player", kCity, "grain", 10);
    SIM_CHECK(s.ok);
    SIM_CHECK_EQ(s.silver, sq);
    SIM_CHECK(w.character.skills.count("persuasion") == 1);  // selling is talking a buyer round

    // Bargaining and rank with the city's polity better the price.
    const Silver plain = quote_buy(w, "player", kCity, "lapis_lazuli", 10);
    w.character.skills["bargaining"].value = 50;
    const Silver haggled = quote_buy(w, "player", kCity, "lapis_lazuli", 10);
    SIM_CHECK(haggled < plain);
    w.character.rank_by_polity[city_faction(w.db, kCity)] = 3;
    SIM_CHECK(quote_buy(w, "player", kCity, "lapis_lazuli", 10) < haggled);
    return true;
}

static bool test_rank_by_deeds_and_patron_and_outlawry() {
    WorldState w;
    w.init("../db/canon", kSeed);
    const Id polity = city_faction(w.db, kCity);
    Id patron, stranger;
    for (const Npc& n : w.population.npcs) {
        if (patron.empty() && n.faction_id == polity) patron = n.id;
        if (stranger.empty() && !n.faction_id.empty() && n.faction_id != polity) stranger = n.id;
    }
    SIM_CHECK(!patron.empty());
    SIM_CHECK_EQ(raise_rank(w, "nowhere", patron).refusal, std::string("unknown_polity"));
    SIM_CHECK_EQ(raise_rank(w, polity, patron).refusal, std::string("standing_too_low"));
    add_standing(w.faction, polity, kRankStanding[2]);
    SIM_CHECK_EQ(raise_rank(w, polity, "nobody").refusal, std::string("unknown_patron"));
    if (!stranger.empty())
        SIM_CHECK_EQ(raise_rank(w, polity, stranger).refusal, std::string("patron_not_of_polity"));
    SIM_CHECK(raise_rank(w, polity, patron).ok);
    SIM_CHECK(raise_rank(w, polity, patron).ok);
    SIM_CHECK_EQ(rank(w.character, polity), 2);
    SIM_CHECK_EQ(raise_rank(w, polity, patron).refusal, std::string("standing_too_low"));
    add_standing(w.faction, polity, 100);
    for (int i = 0; i < 3; ++i) SIM_CHECK(raise_rank(w, polity, patron).ok);
    SIM_CHECK_EQ(rank(w.character, polity), 5);
    SIM_CHECK(raise_rank(w, polity, "").ok);  // Lugal: the right of kingship needs no patron
    SIM_CHECK_EQ(rank(w.character, polity), 6);
    SIM_CHECK_EQ(raise_rank(w, polity, patron).refusal, std::string("at_highest_rank"));

    // Outlawry: an exile/death verdict drops the rank to 0 (row 12).
    const Id crime = commit_crime(w, "player", "burglary", kCity, {patron});
    const Hearing h = hold_crime_hearing(w, crime, "", kCity);
    SIM_CHECK_EQ(h.verdict, std::string("death"));
    SIM_CHECK_EQ(rank(w.character, polity), 0);
    SIM_CHECK_EQ(raise_rank(w, polity, patron).refusal, std::string("outlawed"));
    return true;
}

// The use hooks in the caller-side layers: craft and eat (CApi), rites
// (Rites.cpp), an unseen crime (Actions.cpp).
static bool test_use_hooks() {
    SimWorld* sw = sim_world_create("../db/canon", kSeed);
    SIM_CHECK(sw != nullptr);
    sim_world_give_item(sw, "player", "grain", 4);
    sim_world_give_item(sw, "player", "water", 1);
    SIM_CHECK_EQ(sim_world_craft(sw, "player", "mill_flour", "quern", 2), 0);
    SIM_CHECK_EQ(sim_world_craft(sw, "player", "bake_bread", "oven", 1), 0);
    SIM_CHECK(sim_world_skill(sw, "player", "cooking") > 0);
    SIM_CHECK_EQ(sim_world_skill(sw, "player", "brewing"), 0);
    // A refused craft grows nothing.
    const int xp = sim_world_character_xp(sw);
    SIM_CHECK_EQ(sim_world_craft(sw, "player", "brew_beer", "brewing_vat", 1), -4);
    SIM_CHECK_EQ(sim_world_character_xp(sw), xp);
    // The new craft stations: pottery at the wheel.
    sim_world_give_item(sw, "player", "clay", 20);
    sim_world_give_item(sw, "player", "water", 10);
    SIM_CHECK_EQ(sim_world_craft(sw, "player", "throw_pot", "potters_wheel", 10), 0);
    SIM_CHECK(sim_world_skill(sw, "player", "pottery") > 0);
    // Eating when hungry grows survival (slowly); eating sated teaches nothing.
    sim_world_give_item(sw, "player", "fish", 20);
    for (int i = 0; i < 10; ++i) SIM_CHECK_EQ(sim_world_eat(sw, "player", "fish"), 0);
    SIM_CHECK_EQ(sim_world_skill(sw, "player", "survival"), 0);
    for (int i = 0; i < 10; ++i) {
        sim_world_advance_needs(sw, "player", 20, 0);  // hunger 40: hungry
        sim_world_advance_needs(sw, "player", 12, 1);
        SIM_CHECK_EQ(sim_world_eat(sw, "player", "fish"), 0);
    }
    SIM_CHECK(sim_world_skill(sw, "player", "survival") > 0);
    sim_world_destroy(sw);

    WorldState w;
    w.init("../db/canon", kSeed);
    // A rite performed (success or failure) grows its tradition's skill.
    w.inventories["player"].counts["clay_liver_model"] = 1;
    SIM_CHECK(learn_rite_from_text(w, "barutu_haruspicy", "clay_liver_model").learned);
    w.inventories["player"].counts["a_burned_goat's_liver"] = 1;
    w.magic.place = "house:diviner";
    const RiteOutcome o = perform_rite_in_world(w, "barutu_haruspicy", "inanna");
    SIM_CHECK(o.rite.performed);
    SIM_CHECK(w.character.skills.count("divination") == 1);
    SIM_CHECK(w.character.skills.at("divination").progress > 0 || skill_value(w.character, "divination") > 0);
    // A refused rite grows nothing.
    const int before = w.character.xp;
    const std::int64_t prog = w.character.skills.at("divination").progress;
    (void)perform_rite_in_world(w, "hymns_deity_names", "inanna");  // not known: refused
    SIM_CHECK_EQ(w.character.xp, before);
    SIM_CHECK_EQ(w.character.skills.at("divination").progress, prog);

    // A deed nobody saw grows stealth; a witnessed one does not.
    (void)commit_crime(w, "player", "theft", kCity, {});
    SIM_CHECK(w.character.skills.count("stealth") == 1);
    const std::int64_t st = w.character.skills.at("stealth").progress;
    (void)commit_crime(w, "player", "theft", kCity, {npc_with_role(w, "watchman")});
    SIM_CHECK_EQ(w.character.skills.at("stealth").progress, st);
    // An npc's deeds never touch the player's sheet.
    (void)commit_crime(w, npc_with_role(w, "baker"), "theft", kCity, {});
    SIM_CHECK_EQ(w.character.skills.at("stealth").progress, st);
    return true;
}

// Talents with rite_favour_bp add to a successful offering's favour.
static bool test_rite_favour_talent() {
    WorldState w;
    w.init("../db/canon", kSeed);
    const Id priest = npc_with_role(w, "priest of the moon");
    SIM_CHECK(learn_rite_from_teacher(w, "hymns_deity_names", priest).learned);
    w.magic.place = "temple:city_of_the_moon";
    w.character.talents.push_back("true_names");  // rite_favour_bp +5000
    refresh_derived(w.progression, w.character);
    for (int i = 0; i < 20; ++i) {
        w.advance_days(1);
        const RiteOutcome o = perform_rite_in_world(w, "hymns_deity_names", "nanna");
        if (o.rite.succeeded) {
            SIM_CHECK_EQ(o.effect, std::string("favour:nanna:+") +
                                       std::to_string(kHymnFavour + kHymnFavour * 5000 / kBp));
            return true;
        }
    }
    SIM_CHECK(false);  // twenty days without one successful hymn
    return true;
}

// --- save / determinism ---------------------------------------------------------------

static WorldState played_world() {
    WorldState w;
    w.init("../db/canon", kSeed);
    w.advance_days(3);
    credit_purse(w.property, "player", 300);
    const Id dock = npc_with_role(w, "dockworker");
    const Id baker = npc_with_role(w, "baker");
    (void)work_for(w, dock, 8);
    advance_needs(w.needs, "player", 12, true);
    (void)train_with_teacher(w, "cooking", baker, 8);
    advance_needs(w.needs, "player", 12, true);
    (void)practice_skill(w, "spear", 8);
    w.character.xp = xp_for_level(26);
    apply_levels(w.character);
    (void)choose_calling(w.progression, w.character, "merchant");
    (void)choose_specialisation(w.progression, w.character, "sea_trader");
    (void)choose_second_calling(w.progression, w.character, "warrior");
    (void)choose_talent(w.progression, w.character, "broad_shoulders");
    w.character.rank_by_polity["the_empire"] = 2;
    w.npc_sheets[baker].skills["cooking"] = 61;
    return w;
}

static bool test_snapshot_round_trip_and_old_saves() {
    const WorldState w = played_world();
    const std::string bytes = save_world(w);
    SIM_CHECK(bytes.find("CHAR_CORE") != std::string::npos);
    WorldState r;
    load_world(r, "../db/canon", bytes);
    SIM_CHECK_EQ(save_world(r), bytes);
    SIM_CHECK_EQ(r.character.level, w.character.level);
    SIM_CHECK_EQ(r.character.specialisation, std::string("sea_trader"));
    SIM_CHECK_EQ(r.character.derived.trade_bp, w.character.derived.trade_bp);  // rebuilt
    SIM_CHECK_EQ(attribute(r.character, "strength"), 6);
    SIM_CHECK_EQ(effective_skill(r.character, "sailing"), effective_skill(w.character, "sailing"));
    SIM_CHECK_EQ(r.npc_sheets.at(npc_with_role(w, "baker")).skills.at("cooking"), 61);

    // A pre-W4-A save (no CHAR_ sections) still loads: a fresh prisoner.
    const std::string old = bytes.substr(0, bytes.find("CHAR_CORE"));
    WorldState o;
    load_world(o, "../db/canon", old);
    SIM_CHECK_EQ(o.character.level, 1);
    SIM_CHECK(o.character.skills.empty());
    SIM_CHECK_EQ(o.npc_sheets.size(), o.population.npcs.size());

    // A truncated character section is a malformed save.
    const std::string cut = bytes.substr(0, bytes.find("CHAR_SKILLS"));
    WorldState bad;
    bool threw = false;
    try {
        load_world(bad, "../db/canon", cut);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    SIM_CHECK(threw);
    return true;
}

static bool test_determinism() {
    const std::string a = save_world(played_world());
    const std::string b = save_world(played_world());
    SIM_CHECK_EQ(a, b);
    return true;
}

// --- the C API --------------------------------------------------------------------------

static bool test_c_api_surface() {
    SimWorld* sw = sim_world_create("../db/canon", kSeed);
    SIM_CHECK(sw != nullptr);
    char buf[4096];
    SIM_CHECK_EQ(sim_world_attribute(sw, "player", "strength"), 5);
    SIM_CHECK_EQ(sim_world_attribute(sw, "player", "luck"), -2);
    SIM_CHECK_EQ(sim_world_attribute(sw, nullptr, "strength"), -1);
    SIM_CHECK_EQ(sim_world_skill(sw, "player", "nope"), -2);
    SIM_CHECK_EQ(sim_world_level(sw), 1);
    SIM_CHECK_EQ(sim_world_xp_for_next_level(sw), 6);
    SIM_CHECK_EQ(sim_world_choose_calling(sw, "priest"), -3);
    SIM_CHECK(sim_world_progression_refusal(sw, buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("level_too_low"));
    SIM_CHECK_EQ(sim_world_choose_talent(sw, "nope"), -2);
    SIM_CHECK_EQ(sim_world_practice_skill(sw, "spear", 99), -7);
    SIM_CHECK(sim_world_practice_skill(sw, "spear", 8) >= 0);
    SIM_CHECK_EQ(sim_world_progression_refusal(sw, buf, sizeof buf), 0);
    SIM_CHECK(sim_world_skill_ids(sw, buf, sizeof buf) > 0);
    SIM_CHECK(std::string(buf).find("incantation") != std::string::npos);
    SIM_CHECK(sim_world_skill_teachers(sw, "cooking", buf, sizeof buf) > 0);
    SIM_CHECK(std::string(buf).find(":60:") != std::string::npos);  // the baker, up to 60
    SIM_CHECK(sim_world_character_summary(sw, buf, sizeof buf) > 0);
    SIM_CHECK(std::string(buf).find("level 1 / 40") != std::string::npos);
    SIM_CHECK(std::string(buf).find("spear") != std::string::npos);
    SIM_CHECK_EQ(sim_world_calling(sw, 3, buf, sizeof buf), -1);
    SIM_CHECK_EQ(sim_world_calling(sw, 0, buf, sizeof buf), 0);
    SIM_CHECK_EQ(sim_world_purse(sw, "player"), 0);
    SIM_CHECK_EQ(sim_world_buy(sw, "player", kCity, "grain", 1), -7);  // day 1: market closed
    SIM_CHECK_EQ(sim_world_buy_quote(sw, "player", kCity, "quern", 1), -7);
    SIM_CHECK(sim_world_buy_quote(sw, "player", kCity, "grain", 1) > 0);
    SIM_CHECK_EQ(sim_world_rank(sw, "the_empire"), 0);
    SIM_CHECK_EQ(sim_world_raise_rank(sw, "the_empire", "law_giver"), -3);  // standing 0
    SIM_CHECK_EQ(sim_world_raise_rank(sw, "nowhere", "law_giver"), -2);
    // A refused verb leaves the save untouched.
    std::string before(1 << 20, '\0');
    before.resize(static_cast<std::size_t>(sim_world_save_to_buffer(sw, before.data(), 1 << 20)));
    SIM_CHECK_EQ(sim_world_work(sw, "nobody", 4), -2);
    SIM_CHECK_EQ(sim_world_train_skill(sw, "cooking", "nobody", 4), -2);
    SIM_CHECK_EQ(sim_world_study_skill(sw, "divination", "clay_liver_model", 4), -5);
    std::string after(1 << 20, '\0');
    after.resize(static_cast<std::size_t>(sim_world_save_to_buffer(sw, after.data(), 1 << 20)));
    SIM_CHECK_EQ(after, before);
    sim_world_destroy(sw);
    return true;
}

SIM_MAIN(test_catalog_is_whole, test_every_skill_has_a_use_path, test_fresh_prisoner_and_growth_maths,
         test_levels_callings_and_talents, test_attribute_growth_by_exercise, test_no_skill_decay,
         test_practice_caps_and_tires, test_teachers_charge_and_cap, test_texts_need_literacy,
         test_work_pays_wages_and_grows_skills, test_trade_moves_silver_goods_and_skill,
         test_rank_by_deeds_and_patron_and_outlawry, test_use_hooks, test_rite_favour_talent,
         test_snapshot_round_trip_and_old_saves, test_determinism, test_c_api_surface)
