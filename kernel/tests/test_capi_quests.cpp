// test_capi_quests.cpp — A7: quests and the journal through the C boundary.
#include "sim/CApi.h"
#include "sim/CApiQuests.h"
#include "sim/Test.hpp"

#include "sim/Quests.hpp"
#include "../src/CApiInternal.hpp"  // to plant a world-driven def the way WildWorld does

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>

static std::string at(const SimWorld* w, const char* list, int i) {
    char b[128];
    return sim_world_quest_at(w, list, i, b, sizeof b) >= 0 ? std::string(b) : std::string("<none>");
}

static bool test_quest_lifecycle() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const int available = sim_world_quest_count(w, "available");
    SIM_CHECK(available > 30);  // quests.csv has 41 rows
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 0);
    char b[256];
    SIM_CHECK(sim_world_quest_title(w, "the_outsiders_first_days", b, sizeof b) > 0);
    SIM_CHECK_EQ(std::string(b), "The Outsider's First Days");
    SIM_CHECK(sim_world_quest_giver(w, "the_outsiders_first_days", b, sizeof b) > 0);

    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), 0);
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 1);
    SIM_CHECK_EQ(sim_world_quest_count(w, "available"), available - 1);
    SIM_CHECK_EQ(at(w, "active", 0), "the_outsiders_first_days");
    SIM_CHECK(sim_world_quest_deadline(w, "the_outsiders_first_days") == 1 + 3);  // accepted day 1, 3-day deadline
    SIM_CHECK_EQ(sim_world_quest_advance(w, "the_outsiders_first_days", "found_bread", "A baker's boy shared his loaf."), 1);
    SIM_CHECK_EQ(sim_world_quest_advance(w, "the_outsiders_first_days", "found_bread", ""), 0);
    sim_world_quest_stage(w, "the_outsiders_first_days", b, sizeof b);
    SIM_CHECK_EQ(std::string(b), "found_bread");

    SIM_CHECK_EQ(sim_world_journal_count(w, "the_outsiders_first_days"), 2);  // accepted, found_bread
    int64_t day = 0;
    char stage[64], text[256];
    SIM_CHECK(sim_world_journal_at(w, "the_outsiders_first_days", 1, &day, stage, sizeof stage, text, sizeof text) > 0);
    SIM_CHECK(day == 1 && std::string(stage) == "found_bread");
    SIM_CHECK(std::strstr(text, "loaf") != nullptr);

    SIM_CHECK_EQ(sim_world_quest_complete(w, "the_outsiders_first_days"), 0);
    SIM_CHECK_EQ(sim_world_quest_count(w, "completed"), 1);
    SIM_CHECK_EQ(sim_world_journal_count(w, "the_outsiders_first_days"), 3);  // + completed
    sim_world_destroy(w);
    return true;
}

static bool test_deadline_fails_and_lists_it() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), 0);
    sim_world_advance_days(w, 5);  // past its 3-day deadline
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 0);
    SIM_CHECK_EQ(sim_world_quest_count(w, "failed"), 1);
    SIM_CHECK_EQ(at(w, "failed", 0), "the_outsiders_first_days");
    sim_world_destroy(w);
    return true;
}

static bool test_quest_verbs_refuse_nonsense() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "no_such_quest"), -2);
    SIM_CHECK_EQ(sim_world_quest_complete(w, "the_outsiders_first_days"), -3);  // never accepted
    SIM_CHECK_EQ(sim_world_quest_abandon(w, "the_outsiders_first_days"), -3);
    SIM_CHECK_EQ(sim_world_quest_advance(w, "the_outsiders_first_days", "x", ""), -3);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), 0);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), -3);    // twice
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 1);
    SIM_CHECK_EQ(sim_world_quest_abandon(w, "the_outsiders_first_days"), 0);
    SIM_CHECK_EQ(sim_world_journal_count(w, "the_outsiders_first_days"), 2);   // accepted, abandoned
    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), -3);    // failed stays failed
    SIM_CHECK_EQ(sim_world_quest_count(w, "bogus_list"), -1);
    SIM_CHECK_EQ(sim_world_quest_count(nullptr, "active"), -1);
    SIM_CHECK_EQ(sim_world_quest_accept(w, nullptr), -1);
    char b[8];
    SIM_CHECK_EQ(sim_world_quest_at(w, "active", 0, b, sizeof b), -1);  // index past end
    SIM_CHECK_EQ(sim_world_journal_at(w, "no_such_quest", 0, nullptr, nullptr, 0, nullptr, 0), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_journal_survives_save_load() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    sim_world_quest_accept(w, "the_outsiders_first_days");
    const char* tricky = "He said, \"bread\"; then left,\n\tquickly";
    SIM_CHECK_EQ(sim_world_quest_advance(w, "the_outsiders_first_days", "met_baker", tricky), 1);
    const std::string path = (std::filesystem::temp_directory_path() / "sim_journal_save.txt").string();
    SIM_CHECK_EQ(sim_world_save(w, path.c_str()), 0);
    SimWorld* l = sim_world_load("../db/canon", path.c_str());
    SIM_CHECK(l != nullptr);
    int64_t day;
    char stage[64], text[256];
    SIM_CHECK(sim_world_journal_at(l, "the_outsiders_first_days", 1, &day, stage, sizeof stage, text, sizeof text) > 0);
    SIM_CHECK_EQ(std::string(text), tricky);
    sim_world_destroy(l);
    sim_world_destroy(w);
    std::filesystem::remove(path);
    return true;
}

static bool test_dialogue_lines_for_a_speaker() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const char* SPEAKER = "the captain of the prisoner transport";  // 2 opening-act lines
    const int n = sim_world_dialogue_count(w, SPEAKER);
    SIM_CHECK(n >= 1);
    char id[128], text[1024];
    SIM_CHECK(sim_world_dialogue_at(w, SPEAKER, 0, id, sizeof id, text, sizeof text) > 0);
    SIM_CHECK(std::strlen(id) > 0);
    SIM_CHECK_EQ(sim_world_dialogue_at(w, SPEAKER, n, id, sizeof id, text, sizeof text), -1);
    SIM_CHECK_EQ(sim_world_dialogue_count(w, "nobody at all"), 0);
    sim_world_destroy(w);
    return true;
}

static bool test_dialogue_speaker_keys() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const char* SPEAKER = "the captain of the prisoner transport";
    std::string upper = SPEAKER;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    SIM_CHECK_EQ(sim_world_dialogue_count(w, upper.c_str()), sim_world_dialogue_count(w, SPEAKER));
    SIM_CHECK_EQ(sim_world_dialogue_count(w, nullptr), -1);
    SIM_CHECK_EQ(sim_world_dialogue_count(nullptr, SPEAKER), -1);
    sim_world_destroy(w);
    return true;
}

// A world-driven quest: a def with no quests.csv row, created and advanced by
// the kernel itself (WildWorld's rescue_<npc>). Returns the npc id used.
static std::string plant_rescue(SimWorld* w) {
    char npc[128];
    sim_world_npc_id(w, 0, npc, sizeof npc);
    sim::QuestDef def;
    def.id = std::string("rescue_") + npc;
    def.kind = "emergent";
    def.reward_silver = 30;
    w->world.quests.defs.push_back(def);
    sim::accept(w->world.quests, def.id, w->world.day);
    return npc;
}

static bool test_world_driven_quests_are_not_player_verbs() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const int available = sim_world_quest_count(w, "available");
    const std::string npc = plant_rescue(w);
    const std::string qid = "rescue_" + npc;
    SIM_CHECK_EQ(sim_world_quest_count(w, "available"), available);  // never offered
    SIM_CHECK_EQ(sim_world_quest_complete(w, qid.c_str()), -4);        // no reward without a rescue
    SIM_CHECK_EQ(sim_world_quest_advance(w, qid.c_str(), "x", ""), -4);
    SIM_CHECK_EQ(sim_world_quest_abandon(w, qid.c_str()), -4);
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 1);               // untouched
    // Canon quests of every kind stay the player's (D-021: open play).
    SIM_CHECK_EQ(sim_world_quest_accept(w, "a_roof_for_the_outsider"), 0);  // kind emergent, but a canon row
    sim_world_destroy(w);
    return true;
}

static bool test_world_driven_quests_have_a_title() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const std::string npc = plant_rescue(w);
    const std::string qid = "rescue_" + npc;
    char name[128], title[256], kind[64];
    sim_world_npc_name(w, npc.c_str(), name, sizeof name);
    SIM_CHECK(sim_world_quest_title(w, qid.c_str(), title, sizeof title) > 0);
    SIM_CHECK_EQ(std::string(title), std::string("Rescue ") + name);
    SIM_CHECK(sim_world_quest_kind(w, qid.c_str(), kind, sizeof kind) > 0);
    SIM_CHECK_EQ(std::string(kind), "emergent");
    sim_world_destroy(w);
    return true;
}

static bool test_quest_act_and_kind() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    char b[64];
    SIM_CHECK(sim_world_quest_act(w, "the_outsiders_first_days", b, sizeof b) > 0);
    SIM_CHECK_EQ(std::string(b), "opening");
    SIM_CHECK(sim_world_quest_kind(w, "the_outsiders_first_days", b, sizeof b) > 0);
    SIM_CHECK_EQ(std::string(b), "systemic");
    SIM_CHECK_EQ(sim_world_quest_act(w, "no_such_quest", b, sizeof b), -1);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_quest_lifecycle, test_deadline_fails_and_lists_it, test_quest_verbs_refuse_nonsense,
         test_journal_survives_save_load,
         test_dialogue_lines_for_a_speaker, test_dialogue_speaker_keys,
         test_world_driven_quests_are_not_player_verbs, test_world_driven_quests_have_a_title,
         test_quest_act_and_kind)
