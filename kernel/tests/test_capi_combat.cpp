// test_capi_combat.cpp — W4-B: the combat C API (sim/CApi.h). Null
// safety, return codes, the attack text buffer, body queries, rest and
// treatment, the justice hook, skirmish, and a save/load round trip of the
// combat state through the buffer API.
#include "sim/CApi.h"

#include "sim/Test.hpp"

#include <cstring>
#include <string>

namespace {

const char* kCanon = "../db/canon";

std::string text(int (*fn)(const SimWorld*, const char*, char*, int), const SimWorld* w, const char* a) {
    char buf[512];
    return fn(w, a, buf, sizeof buf) >= 0 ? std::string(buf) : std::string("<err>");
}

bool test_nulls() {
    SIM_CHECK_EQ(sim_world_equip(nullptr, "player", "copper_spear"), -1);
    SIM_CHECK_EQ(sim_world_attack(nullptr, "a", "b", 0, nullptr, 0), -1);
    SIM_CHECK_EQ(sim_world_health(nullptr, "player"), -1);
    SIM_CHECK_EQ(sim_world_wound(nullptr, "player", 0), -1);
    SIM_CHECK_EQ(sim_world_rest(nullptr, "player", 8), -1);
    SIM_CHECK_EQ(sim_world_treat_wounds(nullptr, "player", "bind", nullptr), -1);
    SIM_CHECK_EQ(sim_world_combat_crime(nullptr, "a", "b", "c", "", nullptr, 0), -1);
    SIM_CHECK_EQ(sim_world_skirmish(nullptr, "a", "b", 5, nullptr, 0), -1);
    SIM_CHECK_EQ(sim_world_death_count(nullptr), -1);
    SIM_CHECK_EQ(sim_world_ransom(nullptr, "a", nullptr, 0), -1);
    SimWorld* w = sim_world_create(kCanon, 3);
    SIM_CHECK(w != nullptr);
    SIM_CHECK_EQ(sim_world_equip(w, nullptr, "copper_spear"), -1);
    SIM_CHECK_EQ(sim_world_wound(w, "player", 7), -1);
    SIM_CHECK_EQ(sim_world_health(w, "player"), 100);   // never hurt
    SIM_CHECK_EQ(sim_world_stamina(w, "player"), 100);
    SIM_CHECK_EQ(sim_world_morale(w, "player"), 50);
    SIM_CHECK_EQ(sim_world_is_dead(w, "player"), 0);
    sim_world_destroy(w);
    return true;
}

bool test_equip_attack_and_body() {
    SimWorld* w = sim_world_create(kCanon, 11);
    SIM_CHECK_EQ(sim_world_equip(w, "player", "grain"), -2);          // not arms
    SIM_CHECK_EQ(sim_world_equip(w, "player", "copper_spear"), -3);   // not held
    SIM_CHECK_EQ(sim_world_give_item(w, "player", "copper_spear", 1), 1);
    SIM_CHECK_EQ(sim_world_give_item(w, "player", "tower_shield", 1), 1);
    SIM_CHECK_EQ(sim_world_equip(w, "player", "copper_spear"), 0);
    SIM_CHECK_EQ(sim_world_equip(w, "player", "tower_shield"), 0);
    SIM_CHECK_EQ(text(sim_world_equipped, w, "player"), std::string("copper_spear;tower_shield"));
    SIM_CHECK_EQ(sim_world_unequip(w, "player", "tower_shield"), 0);
    SIM_CHECK_EQ(sim_world_unequip(w, "player", "tower_shield"), 1);

    // Attack a townsman until he falls; every code is in range and the text matches.
    char buf[512];
    int code = 0;
    bool saw_wound = false;
    for (int i = 0; i < 300; ++i) {
        code = sim_world_attack(w, "player", "iltani_cloth_trader", 1, buf, sizeof buf);
        SIM_CHECK(code >= 0 && code <= 6);
        SIM_CHECK(std::strncmp(buf, "outcome=", 8) == 0);
        if (code >= 4) saw_wound = true;
        if (code == 6) break;
    }
    SIM_CHECK(saw_wound);
    SIM_CHECK_EQ(code, 6);
    SIM_CHECK(std::strstr(buf, "outcome=killed") != nullptr);
    SIM_CHECK_EQ(sim_world_is_dead(w, "iltani_cloth_trader"), 1);
    SIM_CHECK_EQ(sim_world_death_count(w), 1);
    SIM_CHECK_EQ(sim_world_attack(w, "player", "iltani_cloth_trader", 1, buf, sizeof buf), -2);
    SIM_CHECK(std::strstr(buf, "refusal=defender_dead") != nullptr);
    SIM_CHECK(std::string(text(sim_world_combat_status, w, "iltani_cloth_trader")) == "dead");
    // Gone from the people list: no index answers with her id.
    const int n = sim_world_npc_count(w);
    for (int i = 0; i < n; ++i) {
        SIM_CHECK(sim_world_npc_id(w, i, buf, sizeof buf) > 0);
        SIM_CHECK(std::string(buf) != "iltani_cloth_trader");
    }
    SIM_CHECK_EQ(sim_world_npc_task_at(w, "iltani_cloth_trader", 9, buf, sizeof buf), -1);
    // Murder: the watchman saw it.
    SIM_CHECK_EQ(sim_world_combat_crime(w, "player", "iltani_cloth_trader", "city_of_the_moon",
                                        "sin_eribam_watchman", buf, sizeof buf), 1);
    SIM_CHECK(std::strncmp(buf, "murder;crime_", 13) == 0);
    SIM_CHECK_EQ(sim_world_combat_crime(w, "player", "nobody_i_fought", "city_of_the_moon", "", buf, sizeof buf), 0);
    sim_world_destroy(w);
    return true;
}

bool test_wounds_rest_treat_and_save() {
    SimWorld* w = sim_world_create(kCanon, 19);
    // The watchman beats the player (unarmed) until he is hurt and bleeding.
    char buf[512];
    for (int i = 0; i < 200 && sim_world_bleeding(w, "player") <= 0; ++i) {
        if (sim_world_is_dead(w, "player") == 1) break;
        (void)sim_world_attack(w, "sin_eribam_watchman", "player", 1, buf, sizeof buf);
    }
    SIM_CHECK_EQ(sim_world_is_dead(w, "player"), 0);
    SIM_CHECK(sim_world_bleeding(w, "player") > 0);
    SIM_CHECK(sim_world_wound(w, "player", 0) + sim_world_wound(w, "player", 1) +
                  sim_world_wound(w, "player", 2) + sim_world_wound(w, "player", 3) > 0);
    SIM_CHECK(text(sim_world_combat_status, w, "player").find("bleeding") != std::string::npos);
    SIM_CHECK(text(sim_world_wounds, w, "player").find(":none") != std::string::npos);
    SIM_CHECK_EQ(text(sim_world_combat_style_of, w, "sin_eribam_watchman"), std::string("city_watchman"));
    SIM_CHECK(sim_world_catch_breath(w, "player", 1) >= 5);
    SIM_CHECK_EQ(sim_world_treat_wounds(w, "player", "prayer", nullptr), -2);
    const int bleed_before = sim_world_bleeding(w, "player");
    SIM_CHECK(sim_world_treat_wounds(w, "player", "bind", nullptr) >= 1);
    SIM_CHECK(sim_world_bleeding(w, "player") < bleed_before || sim_world_bleeding(w, "player") <= 1);
    SIM_CHECK_EQ(sim_world_give_item(w, "player", "healing_herbs", 1), 1);
    SIM_CHECK(sim_world_treat_wounds(w, "player", "herbs", nullptr) >= 0);
    SIM_CHECK(sim_world_bleeding(w, "player") <= 1);   // only a mortal wound still seeps
    SIM_CHECK(text(sim_world_wounds, w, "player").find(":herbs") != std::string::npos);

    // Save with wounds open; the loaded world answers the same.
    const int len = sim_world_save_to_buffer(w, nullptr, 0);
    (void)len;
    std::string save(1 << 20, '\0');
    const int wrote = sim_world_save_to_buffer(w, save.data(), static_cast<int>(save.size()));
    SIM_CHECK(wrote > 0 && wrote < static_cast<int>(save.size()));
    SimWorld* r = sim_world_load_from_buffer(kCanon, save.c_str());
    SIM_CHECK(r != nullptr);
    SIM_CHECK_EQ(sim_world_health(r, "player"), sim_world_health(w, "player"));
    SIM_CHECK_EQ(text(sim_world_wounds, r, "player"), text(sim_world_wounds, w, "player"));
    SIM_CHECK_EQ(text(sim_world_equipped, r, "sin_eribam_watchman"),
                 text(sim_world_equipped, w, "sin_eribam_watchman"));

    // Rest and days heal.
    const int hurt = sim_world_health(r, "player");
    for (int d = 0; d < 30; ++d) {
        SIM_CHECK_EQ(sim_world_rest(r, "player", 8), 0);
        sim_world_advance_days(r, 1);
    }
    SIM_CHECK(sim_world_health(r, "player") > hurt);
    SIM_CHECK_EQ(sim_world_wound(r, "player", 1), 0);
    sim_world_destroy(r);
    sim_world_destroy(w);
    return true;
}

bool test_stance_prisoners_skirmish() {
    SimWorld* w = sim_world_create(kCanon, 29);
    char buf[256];
    SIM_CHECK_EQ(sim_world_stance(w, "player", 1, 1, buf, sizeof buf), 0);
    SIM_CHECK_EQ(std::string(buf), std::string("fight"));
    SIM_CHECK_EQ(sim_world_apply_combat_style(w, "raider_a", "eastern_raider"), 0);
    SIM_CHECK_EQ(sim_world_apply_combat_style(w, "raider_a", "no_such_style"), -2);
    SIM_CHECK_EQ(sim_world_item_count(w, "raider_a", "arsenical_bronze_axe"), 1);
    SIM_CHECK_EQ(sim_world_take_prisoner(w, "player", "raider_a"), -2);   // he has not yielded
    SIM_CHECK_EQ(sim_world_surrender(w, "raider_a", "player"), 0);
    SIM_CHECK_EQ(sim_world_take_prisoner(w, "player", "raider_a"), 0);
    SIM_CHECK(text(sim_world_combat_status, w, "raider_a").find("prisoner") != std::string::npos);
    SIM_CHECK_EQ(sim_world_loot(w, "player", "raider_a"), 4);             // axe, shield, cap, leg wraps
    SIM_CHECK_EQ(sim_world_ransom(w, "raider_a", buf, sizeof buf), 0);    // penniless: all owed
    SIM_CHECK(std::strncmp(buf, "loan_", 5) == 0);
    SIM_CHECK_EQ(sim_world_ransom(w, "raider_a", buf, sizeof buf), -1);   // already free
    SIM_CHECK_EQ(sim_world_set_outlaw(w, "raider_b", 1), 0);
    SIM_CHECK(text(sim_world_combat_status, w, "raider_b").find("outlaw") != std::string::npos);
    SIM_CHECK_EQ(sim_world_agree_duel(w, "player", "raider_b"), 0);

    // Paladins against raiders, deterministic across two identical worlds.
    SimWorld* v = sim_world_create(kCanon, 29);
    for (SimWorld* x : {w, v}) {
        for (const char* p : {"pal_1", "pal_2", "pal_3"}) SIM_CHECK_EQ(sim_world_apply_combat_style(x, p, "retributors_paladin"), 0);
        for (const char* r : {"rd_1", "rd_2", "rd_3"}) SIM_CHECK_EQ(sim_world_apply_combat_style(x, r, "eastern_raider"), 0);
    }
    char out1[512], out2[512];
    const int win1 = sim_world_skirmish(w, "pal_1;pal_2;pal_3", "rd_1;rd_2;rd_3", 30, out1, sizeof out1);
    const int win2 = sim_world_skirmish(v, "pal_1;pal_2;pal_3", "rd_1;rd_2;rd_3", 30, out2, sizeof out2);
    SIM_CHECK(win1 >= 0 && win1 <= 2);
    SIM_CHECK_EQ(win1, win2);
    SIM_CHECK_EQ(std::string(out1), std::string(out2));
    SIM_CHECK(std::strncmp(out1, "rounds=", 7) == 0);
    sim_world_destroy(v);
    sim_world_destroy(w);
    return true;
}

bool test_smith_and_healer() {
    SimWorld* w = sim_world_create(kCanon, 31);
    SIM_CHECK_EQ(sim_world_give_item(w, "player", "copper_dagger", 1), 1);
    SIM_CHECK_EQ(sim_world_repair(w, "player", "copper_dagger", "nur_ea_smith"), -6);  // not worn
    SIM_CHECK_EQ(sim_world_repair(w, "player", "grain", "nur_ea_smith"), -2);
    SIM_CHECK_EQ(sim_world_recast(w, "player", "copper_dagger", "copper_spear", "nur_ea_smith"), -5);  // no silver
    SIM_CHECK_EQ(sim_world_treat_wounds(w, "player", "healer", "urlugaledina_physician"), -5);        // no silver
    SIM_CHECK_EQ(sim_world_treat_wounds(w, "player", "healer", "nur_ea_smith"), -4);
    sim_world_destroy(w);
    return true;
}

}  // namespace

SIM_MAIN(test_nulls, test_equip_attack_and_body, test_wounds_rest_treat_and_save, test_stance_prisoners_skirmish,
         test_smith_and_healer)
