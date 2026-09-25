// test_scenario_duel.cpp — W4-B scenario: the player duels a bandit on the
// road by the Moon Gate, wins, is wounded, binds his wounds, loots the body,
// heals over the following days, and the killing is judged — as
// self-defence when the bandit struck first (dismissed), as murder when the
// player kills a townsman unprovoked (death verdict, outlawry), as the lawful
// slaying of a robber when the victim is an outlaw, and as a duel killing
// when both men agreed to fight. Also: the dead leave the population and
// every schedule, the town mourns, and the whole state survives a save.
#include "sim/CombatActions.hpp"
#include "sim/Actions.hpp"
#include "sim/Population.hpp"
#include "sim/Progression.hpp"
#include "sim/Snapshot.hpp"

#include "sim/Test.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

using namespace sim;

namespace {

const char* kCity = "city_of_the_moon";
const char* kWatchman = "sin_eribam_watchman";
const char* kBandit = "bandit_of_the_river_road";  // a stranger from the wild lands (W4-C seam)

int held(const WorldState& w, const Id& actor, const Id& item) {
    const auto inv = w.inventories.find(actor);
    if (inv == w.inventories.end()) return 0;
    const auto it = inv->second.counts.find(item);
    return it == inv->second.counts.end() ? 0 : it->second;
}

void arm_player(WorldState& w) {
    for (const char* item : {"copper_spear", "tower_shield", "leather_cap"}) {
        w.inventories["player"].counts[item] = 1;
        (void)equip_in_world(w, "player", item);
    }
    w.inventories["player"].counts["linen_bandage"] = 2;
}

// The bandit strikes first; they trade blows until one cannot go on.
// Returns true when the player won, alive and wounded.
bool duel(WorldState& w) {
    arm_player(w);
    (void)apply_style_in_world(w, kBandit, "drought_bandit");
    for (int i = 0; i < 200; ++i) {
        const Combatant& b = combatant_of(w.combat, kBandit);
        const Combatant& p = combatant_of(w.combat, "player");
        if (b.dead || p.dead || !can_fight(p)) break;
        if (can_fight(b)) (void)attack_in_world(w, kBandit, "player", kZoneTorso);
        if (!can_fight(combatant_of(w.combat, "player"))) break;
        (void)attack_in_world(w, "player", kBandit, -1);
    }
    const Combatant& p = combatant_of(w.combat, "player");
    return combatant_of(w.combat, kBandit).dead && !p.dead && open_damage(p, -1) > 0;
}

// The first world seed whose duel ends the scripted way (robust to retuning).
std::uint64_t winning_seed() {
    for (std::uint64_t seed = 1; seed < 200; ++seed) {
        WorldState w;
        w.init("../db/canon", seed);
        if (duel(w)) return seed;
    }
    return 0;
}

const Hearing* verdict_for(const WorldState& w, const Id& crime_id) {
    for (const Hearing& h : w.justice.verdicts)
        if (h.crime_id == crime_id) return &h;
    return nullptr;
}

bool test_duel_self_defence_and_healing() {
    const std::uint64_t seed = winning_seed();
    SIM_CHECK(seed != 0);
    WorldState w;
    w.init("../db/canon", seed);
    SIM_CHECK(duel(w));

    // The bandit is dead, the world knows it.
    SIM_CHECK_EQ(w.combat.deaths.size(), std::size_t(1));
    SIM_CHECK_EQ(w.combat.deaths[0].id, Id(kBandit));
    SIM_CHECK_EQ(w.combat.deaths[0].killer, Id("player"));
    SIM_CHECK_EQ(w.events.fired.back().rule_id, Id("combat_death"));
    SIM_CHECK_EQ(first_aggressor(w.combat, "player", kBandit, w.day), Id(kBandit));

    // W4-A wired: the fight taught him the spear (skills grow by use), and
    // the inputs he fights with are his sheet's.
    SIM_CHECK(actor_skill(w, "player", "spear") > 0);
    const CombatInputs in = combat_inputs_for(w, "player");
    SIM_CHECK_EQ(in.strength, actor_attribute(w, "player", "strength"));
    SIM_CHECK(in.skill("spears") >= actor_effective_skill(w, "player", "spear"));

    // Wounded: the pain and the bleeding cost the body (Needs).
    const int fatigue_before = needs_of(w.needs, "player").fatigue;
    const int wounds = open_damage(combatant_of(w.combat, "player"), -1);
    SIM_CHECK(wounds > 0);
    (void)advance_body_hours(w, "player", 1, false);
    if (wounds >= 25) SIM_CHECK(needs_of(w.needs, "player").fatigue > fatigue_before);

    // Bind the wounds (a linen bandage is used if there was bleeding to stop).
    SIM_CHECK(treat_in_world(w, "player", "bind", "") >= 0);
    SIM_CHECK_EQ(bleeding(combatant_of(w.combat, "player")), 0);

    // Strip the body.
    SIM_CHECK(loot_body(w, "player", kBandit) > 0);
    SIM_CHECK_EQ(held(w, "player", "flint_dagger"), 1);
    SIM_CHECK(combatant_of(w.combat, kBandit).weapon.empty());

    // The watchman saw it; the killing goes before the judges.
    Id crime;
    SIM_CHECK_EQ(file_combat_crime(w, "player", kBandit, kCity, {kWatchman}, &crime), Id("self_defence"));
    SIM_CHECK(!crime.empty());
    const Npc* watch = find_npc(w.population, kWatchman);
    SIM_CHECK(watch != nullptr && !watch->memory.empty());

    // A save mid-story restores byte for byte.
    const std::string saved = save_world(w);
    WorldState r;
    load_world(r, "../db/canon", saved);
    SIM_CHECK_EQ(save_world(r), saved);
    SIM_CHECK_EQ(open_damage(combatant_of(r.combat, "player"), -1), wounds);

    // Days pass: he sleeps eight hours a night; the hearing sits on day +3.
    const Id court = city_faction(w.db, kCity);
    const int standing_before = standing(w.faction, court);
    const int health_after_fight = combatant_of(w.combat, "player").health;
    for (int d = 0; d < 40; ++d) {
        (void)rest_in_world(w, "player", 8);
        w.advance_days(1);
    }
    const Hearing* h = verdict_for(w, crime);
    SIM_CHECK(h != nullptr);
    SIM_CHECK_EQ(h->verdict, std::string("dismissed"));        // self-defence: no penalty
    SIM_CHECK_EQ(standing(w.faction, court), standing_before);
    SIM_CHECK(!is_outlawed(w.faction, court));
    const Combatant& healed = combatant_of(w.combat, "player");
    SIM_CHECK(!healed.dead);
    SIM_CHECK_EQ(open_damage(healed, -1), 0);                  // the wounds have closed
    SIM_CHECK_EQ(healed.health, 100);
    SIM_CHECK(healed.health > health_after_fight);
    return true;
}

bool test_murder_of_a_townsman() {
    WorldState w;
    w.init("../db/canon", 17);
    arm_player(w);
    const Id victim = "ur_shara_baker";
    SIM_CHECK(find_npc(w.population, victim) != nullptr);
    SIM_CHECK(npc_task_at(w.db, w.cal, w.population, victim, w.day, 9).has_value());
    std::vector<Id> mourners = w.population.knows[victim];
    SIM_CHECK(!mourners.empty());
    for (int i = 0; i < 300 && !combatant_of(w.combat, victim).dead; ++i)
        (void)attack_in_world(w, "player", victim, kZoneTorso);
    SIM_CHECK(combatant_of(w.combat, victim).dead);

    // Out of the population and every schedule; the town remembers him.
    SIM_CHECK(find_npc(w.population, victim) == nullptr);
    SIM_CHECK(!npc_task_at(w.db, w.cal, w.population, victim, w.day, 9).has_value());
    SIM_CHECK(w.population.knows.find(victim) == w.population.knows.end());
    for (const auto& [id, edges] : w.population.knows)
        SIM_CHECK(std::find(edges.begin(), edges.end(), victim) == edges.end());
    const Npc* mourner = find_npc(w.population, mourners.front());
    SIM_CHECK(mourner != nullptr);
    bool mourns = false;
    for (const MemoryEntry& m : mourner->memory)
        if (m.subject == victim && m.fact.rfind("mourns", 0) == 0) mourns = true;
    SIM_CHECK(mourns);
    SIM_CHECK(w.needs.by_actor.find(victim) == w.needs.by_actor.end());

    // Unprovoked: murder, life for life (laws.csv murder: death;exile).
    Id crime;
    SIM_CHECK_EQ(file_combat_crime(w, "player", victim, kCity, {kWatchman}, &crime), Id("murder"));
    w.advance_days(kHearingDelayDays + 1);
    const Hearing* h = verdict_for(w, crime);
    SIM_CHECK(h != nullptr);
    SIM_CHECK_EQ(h->verdict, std::string("death"));
    SIM_CHECK(is_outlawed(w.faction, city_faction(w.db, kCity)));

    // An outlawed player is himself fair game: the watch may slay him lawfully.
    (void)attack_in_world(w, kWatchman, "player", -1);
    SIM_CHECK_EQ(file_combat_crime(w, kWatchman, "player", kCity, {}), Id("slaying_a_robber"));
    return true;
}

bool test_outlaw_prisoner_and_duel() {
    WorldState w;
    w.init("../db/canon", 23);
    arm_player(w);

    // An outlaw may be slain lawfully even when the player strikes first.
    const Id robber = "robber_of_the_marsh";
    (void)apply_style_in_world(w, robber, "drought_bandit");
    set_outlaw(w.combat, robber, true);
    for (int i = 0; i < 300 && !combatant_of(w.combat, robber).dead; ++i)
        (void)attack_in_world(w, "player", robber, -1);
    SIM_CHECK(combatant_of(w.combat, robber).dead);
    Id crime;
    SIM_CHECK_EQ(file_combat_crime(w, "player", robber, kCity, {kWatchman}, &crime), Id("slaying_a_robber"));
    w.advance_days(kHearingDelayDays + 1);
    SIM_CHECK_EQ(verdict_for(w, crime)->verdict, std::string("dismissed"));

    // A second robber yields and is ransomed; he cannot pay, so he owes.
    const Id yielder = "robber_who_yields";
    (void)apply_style_in_world(w, yielder, "drought_bandit");
    SIM_CHECK(surrender(w.combat, yielder, "player"));
    SIM_CHECK(take_prisoner(w.combat, "player", yielder, w.day));
    SIM_CHECK(loot_body(w, "player", yielder) > 0);           // the captor takes his arms
    Id loan;
    SIM_CHECK_EQ(ransom_prisoner(w, yielder, &loan), Silver(0));
    SIM_CHECK(!loan.empty());
    const Loan* debt = find_loan(w.property, loan);
    SIM_CHECK(debt != nullptr && debt->creditor == Id("player") && debt->principal == kRansomSilver);
    SIM_CHECK(find_prisoner(w.combat, yielder) == nullptr);

    // A man who yielded is under protection: striking him is assault.
    const Id captive = "captive_of_the_road";
    SIM_CHECK(surrender(w.combat, captive, "player"));
    (void)attack_in_world(w, "player", captive, -1);
    SIM_CHECK_EQ(file_combat_crime(w, "player", captive, kCity, {}), Id("assault"));

    // An agreed duel: wounds are no crime; a death is paid in blood-price.
    const Id champion = "taribum_fisherman";
    agree_duel(w.combat, "player", champion, w.day);
    AttackResult first;
    for (int i = 0; i < 50; ++i) {
        first = attack_in_world(w, "player", champion, kZoneArms);
        if (first.outcome == AttackOutcome::Wounded) break;
    }
    SIM_CHECK(first.outcome == AttackOutcome::Wounded);
    SIM_CHECK(file_combat_crime(w, "player", champion, kCity, {kWatchman}).empty());
    for (int i = 0; i < 300 && !combatant_of(w.combat, champion).dead; ++i)
        (void)attack_in_world(w, "player", champion, -1);
    SIM_CHECK_EQ(file_combat_crime(w, "player", champion, kCity, {kWatchman}), Id("duel_killing"));
    return true;
}

bool test_healer_smith_and_ammo() {
    WorldState w;
    w.init("../db/canon", 5);
    credit_purse(w.property, "player", 200);

    // Arrows run out.
    w.inventories["player"].counts["composite_bow"] = 1;
    w.inventories["player"].counts["arrow"] = 2;
    SIM_CHECK_EQ(equip_in_world(w, "player", "composite_bow"), 0);
    SIM_CHECK(attack_in_world(w, "player", "target_post", -1).outcome != AttackOutcome::Invalid);
    SIM_CHECK(attack_in_world(w, "player", "target_post", -1).outcome != AttackOutcome::Invalid);
    SIM_CHECK_EQ(held(w, "player", "arrow"), 0);
    SIM_CHECK_EQ(attack_in_world(w, "player", "target_post", -1).refusal, std::string("no_ammunition"));

    // The asû treats a grave wound for a fee.
    Combatant& p = combatant_of(w.combat, "player");
    Wound deep;
    deep.zone = kZoneLegs;
    deep.type = "pierce";
    deep.severity = kGrave;
    deep.damage = deep.remaining = 20;
    deep.bleed = 5;
    p.wounds.push_back(deep);
    p.health = 70;
    SIM_CHECK_EQ(treat_in_world(w, "player", "healer", "ur_shara_baker"), -4);  // a baker is no healer
    SIM_CHECK_EQ(treat_in_world(w, "player", "herbs", ""), -3);                  // no herbs held
    const Silver before = purse(w.property, "player");
    SIM_CHECK_EQ(treat_in_world(w, "player", "healer", "urlugaledina_physician"), 1);
    SIM_CHECK_EQ(purse(w.property, "player"), before - kHealerFee);
    SIM_CHECK_EQ(purse(w.property, "urlugaledina_physician"), kHealerFee);
    SIM_CHECK_EQ(bleeding(combatant_of(w.combat, "player")), 0);

    // The smith mends a notched blade and recasts a broken one.
    w.inventories["player"].counts["copper_dagger"] = 1;
    combatant_of(w.combat, "player").durability["copper_dagger"] = 30;
    SIM_CHECK_EQ(repair_at_smith(w, "player", "copper_dagger", "ur_shara_baker"), -4);
    SIM_CHECK_EQ(repair_at_smith(w, "player", "copper_dagger", "nur_ea_smith"), 0);
    SIM_CHECK_EQ(current_durability(w.db, combatant_of(w.combat, "player"), "copper_dagger"), 120);
    SIM_CHECK_EQ(repair_at_smith(w, "player", "copper_dagger", "nur_ea_smith"), -6);  // nothing to mend
    combatant_of(w.combat, "player").durability["copper_dagger"] = 0;
    SIM_CHECK_EQ(repair_at_smith(w, "player", "copper_dagger", "nur_ea_smith"), -7);  // must be recast
    SIM_CHECK_EQ(recast_at_smith(w, "player", "copper_dagger", "tin_bronze_sickle_sword", "nur_ea_smith"), 0);
    SIM_CHECK_EQ(held(w, "player", "copper_dagger"), 0);
    SIM_CHECK_EQ(held(w, "player", "tin_bronze_sickle_sword"), 1);
    SIM_CHECK(purse(w.property, "nur_ea_smith") > 0);
    // Iron is beyond the river smiths.
    w.inventories["player"].counts["iron_dagger"] = 1;
    SIM_CHECK_EQ(recast_at_smith(w, "player", "iron_dagger", "copper_spear", "nur_ea_smith"), -7);
    return true;
}

bool test_bleeding_out_in_the_daily_tick() {
    WorldState w;
    w.init("../db/canon", 8);
    // A townsman left senseless and bleeding after a fight dies in the night.
    const Id victim = "taribum_fisherman";
    Combatant& c = combatant_of(w.combat, victim);
    Wound gash;
    gash.type = "cut";
    gash.severity = kMortal;
    gash.damage = gash.remaining = 26;
    gash.bleed = 7;
    c.wounds.push_back(gash);
    c.health = 40;
    c.ko_hours = 6;
    c.last_attacker = "player";
    w.advance_days(1);
    SIM_CHECK(combatant_of(w.combat, victim).dead);
    SIM_CHECK_EQ(w.combat.deaths.back().cause, std::string("bled_out"));
    SIM_CHECK(find_npc(w.population, victim) == nullptr);
    // A conscious one binds his own wound and lives.
    const Id lucky = "ur_dumuzida_dockworker";
    Combatant& l = combatant_of(w.combat, lucky);
    Wound cut = gash;
    cut.severity = kSerious;
    cut.bleed = 2;
    cut.damage = cut.remaining = 12;
    l.wounds.push_back(cut);
    l.health = 80;
    w.advance_days(1);
    SIM_CHECK(!combatant_of(w.combat, lucky).dead);
    SIM_CHECK(find_npc(w.population, lucky) != nullptr);
    return true;
}

bool test_saves_before_and_after_combat() {
    // A save written before W4-B (no COMBAT sections) still loads: nobody hurt.
    WorldState w;
    w.init("../db/canon", 2);
    arm_player(w);
    (void)attack_in_world(w, "player", "taribum_fisherman", -1);
    const std::string full = save_world(w);
    const std::size_t at = full.find("COMBAT_ACTORS\t");
    SIM_CHECK(at != std::string::npos);
    WorldState old;
    load_world(old, "../db/canon", full.substr(0, at));
    SIM_CHECK(old.combat.by_actor.empty());
    // Two identical scripts give identical saves (combat draws are forks).
    WorldState a, b;
    a.init("../db/canon", 77);
    b.init("../db/canon", 77);
    SIM_CHECK(duel(a) == duel(b));
    SIM_CHECK_EQ(save_world(a), save_world(b));
    SIM_CHECK_EQ(a.rng.state(), b.rng.state());
    // A corrupt combat row fails loudly and leaves the live world untouched.
    std::string bad = full;
    bad.replace(bad.find("COMBAT_SEQ"), 10, "COMBAT_SEX");
    WorldState keep = w;
    bool threw = false;
    try {
        load_world(keep, "../db/canon", bad);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    SIM_CHECK(threw);
    SIM_CHECK_EQ(save_world(keep), full);
    return true;
}

}  // namespace

SIM_MAIN(test_duel_self_defence_and_healing, test_murder_of_a_townsman, test_outlaw_prisoner_and_duel,
         test_healer_smith_and_ammo, test_bleeding_out_in_the_daily_tick,
         test_saves_before_and_after_combat)
