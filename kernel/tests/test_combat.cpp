// test_combat.cpp — W4-B: the Combat module on its own (sim/Combat.hpp):
// arms from canon, equipment slots and layers, the integer resolver
// (determinism, basis-point bounds, armour vs damage type, zones, knockout,
// wear, broken shields), bleeding that kills unless bound, healing over days
// with scars and lasting limps, stance, surrender/prisoners/duels, styles and
// the group skirmish.
#include "sim/Combat.hpp"
#include "sim/Db.hpp"
#include "sim/Needs.hpp"

#include "sim/Test.hpp"

#include <algorithm>
#include <string>

using namespace sim;

namespace {

const Db& canon() {
    static const Db db = Db::load("../db/canon");
    return db;
}

CombatInputs skilled(int skill) {
    CombatInputs in;
    in.default_skill = skill;
    return in;
}

bool test_arms_from_canon() {
    const Db& db = canon();
    // Every material tier of mechanics row 29 is present, iron included.
    for (const char* id : {"flint_dagger", "copper_dagger", "arsenical_bronze_axe",
                           "tin_bronze_sickle_sword", "iron_dagger"})
        SIM_CHECK(arms_def(db, id).has_value());
    SIM_CHECK_EQ(arms_def(db, "flint_dagger")->material_tier, std::string("flint"));
    SIM_CHECK_EQ(arms_def(db, "iron_dagger")->material_tier, std::string("iron"));
    const ArmsDef bow = *arms_def(db, "composite_bow");
    SIM_CHECK_EQ(bow.slot, std::string("ranged"));
    SIM_CHECK_EQ(bow.ammo, std::string("arrow"));
    SIM_CHECK(bow.two_handed());
    SIM_CHECK_EQ(arms_def(db, "sling")->ammo, std::string("sling_bullet"));
    const ArmsDef coat = *arms_def(db, "copper_scale_coat");
    SIM_CHECK_EQ(coat.layer, std::string("over"));
    SIM_CHECK_EQ(coat.zones.size(), std::size_t(2));
    SIM_CHECK(coat.protection("cut") > coat.protection("blunt"));  // scales turn edges, not maces
    SIM_CHECK(!arms_def(db, "grain").has_value());                  // not an arms row
    SIM_CHECK(!arms_def(db, "").has_value());
    SIM_CHECK_EQ(fists().damage_type, std::string("blunt"));
    // Every arms row is also a tradeable item.
    for (const Row& r : db.rows("arms")) SIM_CHECK(db.has("items", r.at("id")));
    // Each combat style's kit is real arms.
    for (const Row& r : db.rows("combat_styles")) {
        const auto st = combat_style(db, r.at("id"));
        SIM_CHECK(st.has_value());
        if (!st->weapon.empty()) SIM_CHECK(arms_def(db, st->weapon).has_value());
        if (!st->shield.empty()) SIM_CHECK(arms_def(db, st->shield).has_value());
        for (const Id& a : st->armour) SIM_CHECK(arms_def(db, a).has_value());
    }
    return true;
}

bool test_equip_slots_and_layers() {
    const Db& db = canon();
    CombatState s;
    SIM_CHECK(equip(db, s, "a", "tower_shield") == EquipResult::Ok);
    SIM_CHECK(equip(db, s, "a", "copper_spear") == EquipResult::Ok);
    SIM_CHECK_EQ(s.by_actor["a"].shield, Id("tower_shield"));      // spear and shield: the phalanx
    SIM_CHECK(equip(db, s, "a", "composite_bow") == EquipResult::Ok);
    SIM_CHECK(s.by_actor["a"].shield.empty());                      // the bow needs both hands
    SIM_CHECK(equip(db, s, "a", "quilted_linen_tunic") == EquipResult::Ok);
    SIM_CHECK(equip(db, s, "a", "studded_leather_cloak") == EquipResult::Ok);
    SIM_CHECK_EQ(s.by_actor["a"].armour.size(), std::size_t(2));   // under + over on the torso
    SIM_CHECK(equip(db, s, "a", "copper_scale_coat") == EquipResult::Ok);
    // The coat (over: torso+arms) displaces the cloak (over: torso), keeps the tunic (under).
    const std::vector<Id> want = {"copper_scale_coat", "quilted_linen_tunic"};
    SIM_CHECK(s.by_actor["a"].armour == want);
    SIM_CHECK(equip(db, s, "a", "grain") == EquipResult::UnknownItem);
    SIM_CHECK(unequip(s, "a", "copper_scale_coat"));
    SIM_CHECK(!unequip(s, "a", "copper_scale_coat"));
    return true;
}

bool test_resolver_is_deterministic_and_bounded() {
    const Db& db = canon();
    bool bounded = true;
    auto run = [&](std::uint64_t seed) {
        CombatState s;
        (void)equip(db, s, "a", "copper_dagger");
        (void)equip(db, s, "b", "stone_mace");
        const Rng rng(seed);
        std::string all;
        for (int i = 0; i < 30; ++i) {
            const AttackResult r = resolve_attack(db, rng, s, Fighter{"a", skilled(40)},
                                                  Fighter{"b", skilled(30)}, -1, 1);
            all += r.text + "\n";
            if (r.outcome == AttackOutcome::Invalid) break;
            if (r.chance_bp < 500 || r.chance_bp > 10000 || r.roll_bp < 0 || r.roll_bp > 9999)
                bounded = false;
        }
        return all;
    };
    const std::string one = run(7), two = run(7), other = run(8);
    SIM_CHECK(!one.empty());
    SIM_CHECK(bounded);
    SIM_CHECK_EQ(one, two);    // same seed, same fight, byte for byte
    SIM_CHECK(one != other);   // a different world draws differently
    // Skill shows in the basis-point chance (same draw position: fresh states).
    CombatState s1, s2;
    const Rng rng(3);
    const auto hi = resolve_attack(db, rng, s1, Fighter{"a", skilled(90)}, Fighter{"b", skilled(10)}, -1, 1);
    const auto lo = resolve_attack(db, rng, s2, Fighter{"a", skilled(10)}, Fighter{"b", skilled(90)}, -1, 1);
    SIM_CHECK(hi.chance_bp > lo.chance_bp);
    SIM_CHECK_EQ(hi.chance_bp, 9500);  // capped
    SIM_CHECK(lo.chance_bp >= 500 && lo.chance_bp < 2500);  // floored at 500, never below
    // Refusals change nothing.
    CombatState s3;
    SIM_CHECK(resolve_attack(db, rng, s3, Fighter{"a"}, Fighter{"a"}, -1, 1).outcome == AttackOutcome::Invalid);
    s3.by_actor["dead"].dead = true;
    SIM_CHECK_EQ(resolve_attack(db, rng, s3, Fighter{"a"}, Fighter{"dead"}, -1, 1).refusal,
                 std::string("defender_dead"));
    return true;
}

// A knocked-out defender is struck unopposed and the blow lands where aimed,
// so the same draw can compare armour directly.
AttackResult helpless_blow(const Id& weapon, const std::vector<Id>& armour, int zone) {
    const Db& db = canon();
    CombatState s;
    (void)equip(db, s, "a", weapon);
    for (const Id& piece : armour) (void)equip(db, s, "d", piece);
    s.by_actor["d"].ko_hours = 5;
    return resolve_attack(db, Rng(11), s, Fighter{"a", skilled(40)}, Fighter{"d", skilled(20)}, zone, 1);
}

bool test_armour_zones_and_damage_types() {
    const AttackResult bare = helpless_blow("tin_bronze_sickle_sword", {}, kZoneHead);
    const AttackResult helmed = helpless_blow("tin_bronze_sickle_sword", {"copper_helmet"}, kZoneHead);
    SIM_CHECK(bare.aimed && helmed.aimed);
    SIM_CHECK_EQ(bare.zone, kZoneHead);
    SIM_CHECK(bare.damage > helmed.damage);  // the helmet turns the edge
    // The helmet protects the head only.
    const AttackResult legs = helpless_blow("tin_bronze_sickle_sword", {"copper_helmet"}, kZoneLegs);
    const AttackResult legs_bare = helpless_blow("tin_bronze_sickle_sword", {}, kZoneLegs);
    SIM_CHECK_EQ(legs.damage, legs_bare.damage);
    // The head takes more than a limb from the same blow (x150 vs x75).
    SIM_CHECK(bare.damage > legs_bare.damage);
    // Scales stop a cut better than a mace's blow.
    const AttackResult cut = helpless_blow("arsenical_bronze_axe", {"copper_scale_coat"}, kZoneTorso);
    const AttackResult cut_bare = helpless_blow("arsenical_bronze_axe", {}, kZoneTorso);
    const AttackResult bash = helpless_blow("copper_mace", {"copper_scale_coat"}, kZoneTorso);
    const AttackResult bash_bare = helpless_blow("copper_mace", {}, kZoneTorso);
    SIM_CHECK(cut_bare.damage - cut.damage > bash_bare.damage - bash.damage);
    // Blunt does not bleed; edges do.
    SIM_CHECK_EQ(bash_bare.bleed, 0);
    SIM_CHECK(cut_bare.bleed > 0);
    return true;
}

bool test_knockout_and_death() {
    const Db& db = canon();
    CombatState s;
    (void)equip(db, s, "a", "copper_mace");
    CombatInputs strong = skilled(60);
    strong.strength = 10;
    // Club at the head of a helpless man until something gives.
    s.by_actor["d"].ko_hours = 1;
    bool saw_death = false;
    for (int i = 0; i < 40 && !saw_death; ++i) {
        const AttackResult r = resolve_attack(db, Rng(5), s, Fighter{"a", strong}, Fighter{"d"}, kZoneHead, 1);
        if (r.outcome == AttackOutcome::Killed) saw_death = true;
    }
    SIM_CHECK(saw_death);
    SIM_CHECK(s.by_actor["d"].dead);
    SIM_CHECK_EQ(s.deaths.size(), std::size_t(1));
    SIM_CHECK_EQ(s.deaths[0].killer, Id("a"));
    SIM_CHECK_EQ(s.deaths[0].cause, std::string("slain"));
    // A blunt blow to the head of a standing man can knock him out.
    bool saw_ko = false;
    for (std::uint64_t seed = 1; seed < 60 && !saw_ko; ++seed) {
        CombatState t;
        (void)equip(db, t, "a", "copper_mace");
        const AttackResult r = resolve_attack(db, Rng(seed), t, Fighter{"a", strong}, Fighter{"d", skilled(5)},
                                              kZoneHead, 1);
        if (r.outcome == AttackOutcome::KnockedOut) {
            saw_ko = true;
            SIM_CHECK(t.by_actor["d"].ko_hours > 0);
            SIM_CHECK(!can_fight(t.by_actor["d"]));
        }
    }
    SIM_CHECK(saw_ko);
    return true;
}

bool test_bleeding_kills_unless_bound() {
    auto wounded = [](CombatState& s) {
        Combatant& c = combatant_of(s, "v");
        Wound w;
        w.zone = kZoneTorso;
        w.type = "cut";
        w.severity = kGrave;
        w.damage = w.remaining = 20;
        w.bleed = 4;
        c.wounds.push_back(w);
        c.health = 60;
        c.last_attacker = "a";
    };
    CombatState open, bound;
    wounded(open);
    wounded(bound);
    SIM_CHECK_EQ(bleeding(open.by_actor["v"]), 4);
    SIM_CHECK(advance_combat_hours(open, "v", 24, false, 3));   // bleeds out
    SIM_CHECK(open.by_actor["v"].dead);
    SIM_CHECK_EQ(open.deaths.back().cause, std::string("bled_out"));
    SIM_CHECK_EQ(open.deaths.back().killer, Id("a"));
    SIM_CHECK(treat_wounds(bound, "v", kTreatBound, /*with_linen=*/true) == 1);
    SIM_CHECK_EQ(bleeding(bound.by_actor["v"]), 0);
    SIM_CHECK(!advance_combat_hours(bound, "v", 24, false, 3));
    SIM_CHECK(!bound.by_actor["v"].dead);
    // Torn cloth only slows a grave wound; herbs stop it.
    CombatState torn;
    wounded(torn);
    (void)treat_wounds(torn, "v", kTreatBound, false);
    SIM_CHECK_EQ(bleeding(torn.by_actor["v"]), 1);
    (void)treat_wounds(torn, "v", kTreatHerbs, false);
    SIM_CHECK_EQ(bleeding(torn.by_actor["v"]), 0);
    // A light wound clots on its own within hours.
    CombatState light;
    Combatant& c = combatant_of(light, "v");
    Wound w;
    w.type = "cut";
    w.severity = kLight;
    w.damage = w.remaining = 6;
    w.bleed = 1;
    w.clot_hours = 3;
    c.wounds.push_back(w);
    SIM_CHECK(!advance_combat_hours(light, "v", 5, false, 1));
    SIM_CHECK_EQ(bleeding(light.by_actor["v"]), 0);
    SIM_CHECK_EQ(light.by_actor["v"].health, 97);
    return true;
}

bool test_healing_scars_and_lasting_effects() {
    auto hurt = [](CombatState& s, int zone, int severity, int dmg) {
        Combatant& c = combatant_of(s, "player");
        Wound w;
        w.zone = zone;
        w.type = "cut";
        w.severity = severity;
        w.damage = w.remaining = dmg;
        c.wounds.push_back(w);
        c.health = 100 - dmg;
    };
    CombatState plain, healed;
    hurt(plain, kZoneLegs, kGrave, 18);
    hurt(healed, kZoneLegs, kGrave, 18);
    (void)treat_wounds(healed, "player", kTreatHealer, true);
    int days_plain = 0, days_healed = 0;
    for (DayNumber d = 1; d <= 60; ++d) {
        if (!plain.by_actor["player"].wounds.empty()) { (void)tick_combat(plain, d); ++days_plain; }
        if (!healed.by_actor["player"].wounds.empty()) { (void)tick_combat(healed, d); ++days_healed; }
    }
    SIM_CHECK(plain.by_actor["player"].wounds.empty());
    SIM_CHECK(days_healed < days_plain);  // the asû's hands knit faster
    const auto& lp = plain.by_actor["player"].lasting;
    const auto& lh = healed.by_actor["player"].lasting;
    SIM_CHECK(std::find(lp.begin(), lp.end(), "scar:legs") != lp.end());
    SIM_CHECK(std::find(lp.begin(), lp.end(), "limp") != lp.end());      // never saw a healer
    SIM_CHECK(std::find(lh.begin(), lh.end(), "limp") == lh.end());      // the healer saved the leg
    SIM_CHECK_EQ(healed.by_actor["player"].health, 100);
    // Rest speeds the day's knitting.
    CombatState rested, active;
    hurt(rested, kZoneTorso, kSerious, 12);
    hurt(active, kZoneTorso, kSerious, 12);
    (void)advance_combat_hours(rested, "player", 8, true, 1);
    (void)tick_combat(rested, 1);
    (void)tick_combat(active, 1);
    SIM_CHECK(open_damage(rested.by_actor["player"], -1) < open_damage(active.by_actor["player"], -1));
    return true;
}

bool test_wear_and_broken_shield() {
    const Db& db = canon();
    CombatState s;
    (void)equip(db, s, "a", "arsenical_bronze_axe");
    (void)equip(db, s, "d", "tower_shield");
    s.by_actor["d"].durability["tower_shield"] = 2;
    CombatInputs wall = skilled(10);
    wall.skills["shields"] = 100;
    wall.strength = 10;
    bool broke = false;
    for (int i = 0; i < 60 && !broke; ++i) {
        s.by_actor["a"].stamina = 100;
        const AttackResult r = resolve_attack(db, Rng(21), s, Fighter{"a", skilled(10)}, Fighter{"d", wall}, -1, 1);
        if (r.shield_broke) broke = true;
        if (r.outcome == AttackOutcome::Killed) break;
    }
    SIM_CHECK(broke);
    SIM_CHECK_EQ(current_durability(db, s.by_actor["d"], "tower_shield"), 0);
    // The axe wore too.
    SIM_CHECK(current_durability(db, s.by_actor["a"], "arsenical_bronze_axe") <
              arms_def(db, "arsenical_bronze_axe")->durability);
    // A broken weapon hits softer.
    const AttackResult sharp = helpless_blow("copper_dagger", {}, kZoneTorso);
    CombatState t;
    (void)equip(db, t, "a", "copper_dagger");
    t.by_actor["a"].durability["copper_dagger"] = 0;
    t.by_actor["d"].ko_hours = 5;
    const AttackResult dull = resolve_attack(db, Rng(11), t, Fighter{"a", skilled(40)}, Fighter{"d", skilled(20)},
                                             kZoneTorso, 1);
    SIM_CHECK(dull.damage < sharp.damage);
    return true;
}

bool test_stamina_economy() {
    const Db& db = canon();
    CombatState s;
    (void)equip(db, s, "a", "arsenical_bronze_axe");
    (void)equip(db, s, "a", "copper_scale_coat");
    bool winded = false;
    for (int i = 0; i < 20 && !winded; ++i) {
        s.by_actor["d"].ko_hours = 0;
        const AttackResult r = resolve_attack(db, Rng(4), s, Fighter{"a"}, Fighter{"d"}, -1, 1);
        if (r.winded) winded = true;
        if (s.by_actor["d"].dead) s.by_actor["d"] = Combatant{};
    }
    SIM_CHECK(winded);  // heavy axe + scale coat: the arm tires within a few blows
    const int before = s.by_actor["a"].stamina;
    catch_breath(s, "a", 2);
    SIM_CHECK_EQ(s.by_actor["a"].stamina, before + 2 * kBreathStamina);
    // Exhaustion (Needs) weakens the blow's chance.
    Needs tired;
    tired.fatigue = 90;
    CombatState f1, f2;
    const auto fresh = resolve_attack(db, Rng(2), f1, Fighter{"a", skilled(40)}, Fighter{"d"}, -1, 1);
    const auto spent = resolve_attack(db, Rng(2), f2, Fighter{"a", skilled(40), &tired}, Fighter{"d"}, -1, 1);
    SIM_CHECK(spent.chance_bp < fresh.chance_bp);
    return true;
}

bool test_stance_surrender_and_duels() {
    const Db& db = canon();
    Combatant c;
    c.health = 90;
    c.morale = 60;
    CombatInputs in;
    SIM_CHECK(choose_stance(c, in, 1, 1) == Stance::Fight);
    c.health = 25;
    SIM_CHECK(choose_stance(c, in, 0, 2) == Stance::Flee);
    c.health = 10;
    c.morale = 10;
    SIM_CHECK(choose_stance(c, in, 0, 2) == Stance::Surrender);
    // A paladin never yields; lamed he cannot run, so he fights on.
    const CombatInputs paladin = style_inputs(*combat_style(db, "retributors_paladin"));
    Combatant p;
    p.health = 5;
    p.morale = 5;
    Wound leg;
    leg.zone = kZoneLegs;
    leg.remaining = 30;
    p.wounds.push_back(leg);
    SIM_CHECK(choose_stance(p, paladin, 0, 3) == Stance::Fight);
    // The same lamed man in a common style yields instead of fleeing.
    SIM_CHECK(choose_stance(p, in, 0, 3) == Stance::Surrender);
    Combatant down;
    down.ko_hours = 2;
    SIM_CHECK(choose_stance(down, in, 0, 1) == Stance::None);

    CombatState s;
    SIM_CHECK(!take_prisoner(s, "player", "bandit", 1));          // he has not yielded
    SIM_CHECK(surrender(s, "bandit", "player"));
    SIM_CHECK(take_prisoner(s, "player", "bandit", 1));
    SIM_CHECK(find_prisoner(s, "bandit") != nullptr);
    SIM_CHECK(!can_fight(s.by_actor["bandit"]));
    SIM_CHECK(release_prisoner(s, "bandit"));
    SIM_CHECK(can_fight(s.by_actor["bandit"]));
    agree_duel(s, "player", "champion", 4);
    SIM_CHECK(duel_agreed(s, "champion", "player", 4));
    SIM_CHECK(!duel_agreed(s, "champion", "player", 5));
    (void)resolve_attack(db, Rng(1), s, Fighter{"champion"}, Fighter{"player"}, -1, 4);
    (void)resolve_attack(db, Rng(1), s, Fighter{"player"}, Fighter{"champion"}, -1, 4);
    SIM_CHECK_EQ(first_aggressor(s, "player", "champion", 4), Id("champion"));
    SIM_CHECK(first_aggressor(s, "player", "champion", 5).empty());
    return true;
}

bool test_styles_and_skirmish() {
    const Db& db = canon();
    SIM_CHECK_EQ(style_for(db, "retributors_of_utu", ""), Id("retributors_paladin"));
    SIM_CHECK_EQ(style_for(db, "", "watchman"), Id("city_watchman"));
    SIM_CHECK_EQ(style_for(db, "", "Bandit"), Id("drought_bandit"));
    SIM_CHECK_EQ(style_for(db, "", "baker"), Id("commoner"));
    auto fight = [&](std::uint64_t seed, SkirmishResult* out) {
        CombatState s;
        std::map<Id, CombatInputs> in;
        const std::vector<Id> a = {"paladin_1", "paladin_2", "paladin_3"};
        const std::vector<Id> b = {"raider_1", "raider_2", "raider_3", "raider_4"};
        for (const Id& id : a) {
            SIM_CHECK(apply_style(db, s, id, "retributors_paladin"));
            in[id] = style_inputs(*combat_style(db, "retributors_paladin"));
        }
        for (const Id& id : b) {
            SIM_CHECK(apply_style(db, s, id, "eastern_raider"));
            in[id] = style_inputs(*combat_style(db, "eastern_raider"));
        }
        SIM_CHECK_EQ(s.by_actor["paladin_1"].weapon, Id("tin_bronze_sickle_sword"));
        SIM_CHECK_EQ(s.by_actor["raider_1"].shield, Id("round_hide_shield"));
        *out = resolve_skirmish(db, Rng(seed), s, a, b, in, {}, 1, 30);
        return true;
    };
    SkirmishResult r1, r2;
    SIM_CHECK(fight(99, &r1));
    SIM_CHECK(fight(99, &r2));
    SIM_CHECK_EQ(r1.log.size(), r2.log.size());
    for (std::size_t i = 0; i < r1.log.size(); ++i) SIM_CHECK_EQ(r1.log[i].text, r2.log[i].text);
    SIM_CHECK(r1.rounds > 0);
    // Disciplined, armoured paladins hold the field against raiders more
    // often than not across worlds.
    int paladin_wins = 0, raider_breaks = 0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        SkirmishResult r;
        SIM_CHECK(fight(seed, &r));
        if (r.winner == 0) ++paladin_wins;
        if (!r.fled.empty() || !r.surrendered.empty()) ++raider_breaks;
    }
    SIM_CHECK(paladin_wins >= 12);
    SIM_CHECK(raider_breaks >= 1);  // raiders run or yield — morale is real
    return true;
}

}  // namespace

SIM_MAIN(test_arms_from_canon, test_equip_slots_and_layers, test_resolver_is_deterministic_and_bounded,
         test_armour_zones_and_damage_types, test_knockout_and_death, test_bleeding_kills_unless_bound,
         test_healing_scars_and_lasting_effects, test_wear_and_broken_shield, test_stamina_economy,
         test_stance_surrender_and_duels, test_styles_and_skirmish)
