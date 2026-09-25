// test_wild_capi.cpp — W4-C: the wild lands through the C boundary alone
// (sim/CApiWild.h, included by sim/CApi.h): places and links, travel,
// encounters, camps, raids, the verbs, save/load and null-safety.
#include "sim/CApi.h"
#include "sim/Test.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector<std::string> fields(const char* s) {
    std::vector<std::string> out;
    std::string cur;
    for (const char* p = s; *p; ++p) {
        if (*p == ';') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += *p;
        }
    }
    out.push_back(cur);
    return out;
}

}  // namespace

static bool test_places_links_and_travel_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK(w != nullptr);
    char buf[512];
    const int places = sim_world_wild_place_count(w);
    SIM_CHECK(places >= 15);
    bool saw_gate = false;
    for (int k = 0; k < places; ++k) {
        SIM_CHECK(sim_world_wild_place(w, k, buf, sizeof(buf)) > 0);
        const auto f = fields(buf);
        SIM_CHECK(f.size() >= 9);
        saw_gate = saw_gate || f[0] == "moon_gate_place";
    }
    SIM_CHECK(saw_gate);
    SIM_CHECK_EQ(sim_world_wild_place(w, places, buf, sizeof(buf)), -1);
    const int links = sim_world_wild_link_count(w);
    SIM_CHECK(links >= 20);
    SIM_CHECK(sim_world_wild_link(w, 0, buf, sizeof(buf)) > 0);
    SIM_CHECK_EQ(fields(buf).size(), std::size_t{6});

    SIM_CHECK(sim_world_player_place(w, buf, sizeof(buf)) > 0);
    SIM_CHECK_EQ(std::string(buf), std::string("moon_gate_place"));
    SIM_CHECK(sim_world_weather(w, buf, sizeof(buf)) > 0);
    const int preview = sim_world_route_minutes(w, "date_grove_place", "foot");
    SIM_CHECK(preview > 0);
    SIM_CHECK_EQ(sim_world_route_minutes(w, "riverbank_reeds_place", "chariot"), -5);

    const int code = sim_world_travel(w, "date_grove_place", "foot", 9, "flee", 10, buf, sizeof(buf));
    SIM_CHECK(code == 0 || code == 1);
    SIM_CHECK(std::strstr(buf, "minutes=") == buf);
    if (code == 0) {
        SIM_CHECK(std::string(buf).find("minutes=" + std::to_string(preview)) == 0);  // no shortcut
        SIM_CHECK(sim_world_player_place(w, buf, sizeof(buf)) > 0);
        SIM_CHECK_EQ(std::string(buf), std::string("date_grove_place"));
    }
    SIM_CHECK_EQ(sim_world_travel(w, "nowhere", "foot", 9, nullptr, 0, buf, sizeof(buf)), -2);
    SIM_CHECK_EQ(sim_world_travel(w, "moon_gate_place", "horse", 9, nullptr, 0, nullptr, 0), -4);
    SIM_CHECK_EQ(sim_world_set_player_place(w, "nowhere"), -2);
    SIM_CHECK_EQ(sim_world_set_player_place(w, "moon_gate_place"), 0);
    sim_world_destroy(w);
    return true;
}

static bool test_camps_raids_and_verbs_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 2026);
    SIM_CHECK(w != nullptr);
    sim_world_set_drought(w, 3);
    sim_world_advance_days(w, 150);
    char buf[1024];
    const int camps = sim_world_camp_count(w);
    SIM_CHECK(camps >= 1);
    std::string occupied;
    for (int k = 0; k < camps; ++k) {
        SIM_CHECK(sim_world_camp(w, k, buf, sizeof(buf)) > 0);
        const auto f = fields(buf);
        SIM_CHECK_EQ(f.size(), std::size_t{12});
        if (f[2] == "occupied" && occupied.empty()) occupied = f[0];
    }
    SIM_CHECK(!occupied.empty());
    SIM_CHECK(sim_world_group_count(w) >= 5);
    SIM_CHECK(sim_world_group(w, 0, buf, sizeof(buf)) > 0);
    SIM_CHECK_EQ(fields(buf).size(), std::size_t{13});
    const int raids = sim_world_raid_count(w);
    SIM_CHECK(raids > 0);
    SIM_CHECK(sim_world_raid(w, raids - 1, buf, sizeof(buf)) > 0);
    SIM_CHECK(fields(buf).size() >= 11);
    SIM_CHECK(sim_world_raid_chance(w, "eastern_warband") >= 0);
    SIM_CHECK(sim_world_herd_head(w) >= 0);
    SIM_CHECK(sim_world_caravan_count(w) >= 0);
    // Rumours point at the camps.
    char rumours[8192];
    int heard = 0;
    for (int k = 0; k < 40 && heard <= 0; ++k) {
        char id[128];
        if (sim_world_npc_id(w, k, id, sizeof(id)) < 0) break;
        heard = sim_world_hear_rumours(w, id, rumours, sizeof(rumours));
    }
    SIM_CHECK(heard > 0);

    // Scout from the camp itself, then clear it with hired spears.
    SIM_CHECK_EQ(sim_world_set_player_place(w, occupied.c_str()), 0);
    SIM_CHECK(sim_world_scout_camp(w, occupied.c_str(), 2, buf, sizeof(buf)) >= 0);
    int attack = -99;
    for (int k = 0; k < 6 && attack != 1; ++k)
        attack = sim_world_attack_camp(w, occupied.c_str(), 120, 3, buf, sizeof(buf));
    SIM_CHECK_EQ(attack, 1);
    SIM_CHECK(std::strstr(buf, "cleared") != nullptr);
    SIM_CHECK_EQ(sim_world_join_group(w, "no_such_group", nullptr, 0), -2);
    SIM_CHECK_EQ(sim_world_leave_group(w, buf, sizeof(buf)), -4);
    SIM_CHECK_EQ(sim_world_lead_raid(w, "moon", 2, buf, sizeof(buf)), -1);
    SIM_CHECK(sim_world_found_band(w, occupied.c_str(), 3, buf, sizeof(buf)) == 0);
    SIM_CHECK(sim_world_player_group(w, buf, sizeof(buf)) > 0);
    SIM_CHECK(sim_world_infamy(w) == 0);
    SIM_CHECK(sim_world_player_wounds(w) >= 0);

    // Save / load through the buffer keeps the wild.
    std::vector<char> save(1 << 21);
    const int len = sim_world_save_to_buffer(w, save.data(), static_cast<int>(save.size()));
    SIM_CHECK(len > 0 && len < static_cast<int>(save.size()));
    SimWorld* b = sim_world_load_from_buffer("../db/canon", save.data());
    SIM_CHECK(b != nullptr);
    SIM_CHECK_EQ(sim_world_raid_count(b), sim_world_raid_count(w));
    SIM_CHECK_EQ(sim_world_camp_count(b), sim_world_camp_count(w));
    char pa[128], pb[128];
    sim_world_player_group(w, pa, sizeof(pa));
    sim_world_player_group(b, pb, sizeof(pb));
    SIM_CHECK_EQ(std::string(pa), std::string(pb));
    sim_world_destroy(b);
    sim_world_destroy(w);
    return true;
}

static bool test_null_safety() {
    char buf[16];
    SIM_CHECK_EQ(sim_world_wild_place_count(nullptr), -1);
    SIM_CHECK_EQ(sim_world_wild_place(nullptr, 0, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_wild_link_count(nullptr), -1);
    SIM_CHECK_EQ(sim_world_player_place(nullptr, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_set_player_place(nullptr, "x"), -1);
    SIM_CHECK_EQ(sim_world_route_minutes(nullptr, "x", "foot"), -1);
    SIM_CHECK_EQ(sim_world_travel(nullptr, "x", "foot", 0, nullptr, 0, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_encounter_count(nullptr), -1);
    SIM_CHECK_EQ(sim_world_camp_count(nullptr), -1);
    SIM_CHECK_EQ(sim_world_raid_count(nullptr), -1);
    SIM_CHECK_EQ(sim_world_raid_chance(nullptr, "x"), -1);
    SIM_CHECK_EQ(sim_world_npc_fate(nullptr, "x", buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_hear_rumours(nullptr, "x", buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_attack_camp(nullptr, "x", 0, 0, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_search_site(nullptr, "x", 0, buf, sizeof(buf)), -1);
    SimWorld* w = sim_world_create("../db/canon", 1);
    SIM_CHECK(w != nullptr);
    SIM_CHECK_EQ(sim_world_player_place(w, nullptr, 0), -1);
    SIM_CHECK_EQ(sim_world_travel(w, nullptr, "foot", 0, nullptr, 0, nullptr, 0), -1);
    SIM_CHECK_EQ(sim_world_attack_camp(w, nullptr, 0, 0, nullptr, 0), -1);
    SIM_CHECK_EQ(sim_world_camp(w, 999, buf, sizeof(buf)), -1);
    SIM_CHECK_EQ(sim_world_npc_fate(w, "ur_utu_gatekeeper_dawn", buf, sizeof(buf)), 0);  // alive
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_places_links_and_travel_through_c, test_camps_raids_and_verbs_through_c,
         test_null_safety)
