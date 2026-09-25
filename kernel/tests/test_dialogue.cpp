// test_dialogue.cpp — the dialogue runner: speaker matching + act-gated
// eligibility (see sim/Dialogue.hpp for the INVENTED rule this exercises).
// A synthetic dialogues.csv + quests.csv fixture drives the gating paths
// (real dialogues.csv has no OPEN rows to test against); the real canon is
// also swept for reachability and determinism.
#include "sim/Dialogue.hpp"

#include "sim/Test.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

using namespace sim;

namespace {

Db load_fixture_db() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "schizo_game_test_dialogue_fixture";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "dialogues.csv", std::ios::trunc);
        out << "id,speaker,context,text,tag,source_ref\n"
            << "d_open,a water-carrier,river steps,\"the ration is thin\",INVENTED,"
               "serving story.csv:opening; test fixture\n"
            << "d_act1,a water-carrier,river steps,\"still thinner\",INVENTED,"
               "serving story.csv:act_i; test fixture\n"
            << "d_act2,a water-carrier,river steps,\"the well is dry\",INVENTED,"
               "serving story.csv:act_ii; test fixture\n"
            << "d_prophet,the Prophet,temple court,\"four promises\",INVENTED,"
               "serving story.csv:act_ii; test fixture\n"
            << "d_no_act,a stranger,dusk,\"no act token at all\",INVENTED,test fixture\n"
            << "d_unwritten,a water-carrier,river steps,\"unwritten line\",OPEN,"
               "serving story.csv:opening; test fixture\n";
    }
    {
        std::ofstream out(dir / "quests.csv", std::ios::trunc);
        out << "id,name,giver,act,kind,deadline_days,tag,source_ref\n"
            << "q_act1,Test Act I Quest,tester,act_i,systemic,3,CANON,test fixture\n"
            << "q_act2,Test Act II Quest,tester,act_ii,systemic,3,CANON,test fixture\n";
    }
    return Db::load(dir.string());
}

}  // namespace

static bool test_speaker_matches_role_ignoring_article_and_case() {
    const Db db = load_fixture_db();
    PlayerContext none;
    const auto lines = eligible_lines(db, "WATER-carrier", none);
    // Only the opening-act line is eligible with no quests held (opening default).
    SIM_CHECK_EQ(lines.size(), std::size_t{1});
    SIM_CHECK_EQ(lines[0].id, std::string("d_open"));
    return true;
}

static bool test_speaker_matches_person_id_with_underscores_as_spaces() {
    const Db db = load_fixture_db();
    PlayerContext ctx;
    ctx.active_quests = {"q_act2"};  // reaches act_ii
    const auto lines = eligible_lines(db, "the_prophet", ctx);
    SIM_CHECK_EQ(lines.size(), std::size_t{1});
    SIM_CHECK_EQ(lines[0].id, std::string("d_prophet"));
    return true;
}

static bool test_act_gating_widens_as_quests_progress() {
    const Db db = load_fixture_db();

    PlayerContext none;
    SIM_CHECK_EQ(eligible_lines(db, "water-carrier", none).size(), std::size_t{1});  // opening only

    PlayerContext act1;
    act1.active_quests = {"q_act1"};
    const auto lines1 = eligible_lines(db, "water-carrier", act1);
    SIM_CHECK_EQ(lines1.size(), std::size_t{2});  // opening + act_i

    PlayerContext act2;
    act2.completed_quests = {"q_act2"};  // completed counts too
    const auto lines2 = eligible_lines(db, "water-carrier", act2);
    SIM_CHECK_EQ(lines2.size(), std::size_t{3});  // opening + act_i + act_ii
    return true;
}

static bool test_open_row_never_surfaces() {
    const Db db = load_fixture_db();
    PlayerContext ctx;
    ctx.active_quests = {"q_act2"};
    const auto lines = eligible_lines(db, "water-carrier", ctx);
    for (const DialogueLine& l : lines) SIM_CHECK(l.id != std::string("d_unwritten"));
    return true;
}

static bool test_line_with_no_act_token_is_always_eligible() {
    const Db db = load_fixture_db();
    PlayerContext none;
    const auto lines = eligible_lines(db, "a stranger", none);
    SIM_CHECK_EQ(lines.size(), std::size_t{1});
    SIM_CHECK_EQ(lines[0].id, std::string("d_no_act"));
    return true;
}

static bool test_unknown_speaker_and_unknown_quest_ids_are_graceful() {
    const Db db = load_fixture_db();
    PlayerContext ctx;
    ctx.active_quests = {"no_such_quest"};  // unresolved id: ignored, not a crash
    SIM_CHECK(eligible_lines(db, "a dragon", ctx).empty());
    return true;
}

static bool test_determinism_same_inputs_same_outputs() {
    const Db db = load_fixture_db();
    PlayerContext ctx;
    ctx.active_quests = {"q_act1"};
    const auto a = eligible_lines(db, "water-carrier", ctx);
    const auto b = eligible_lines(db, "water-carrier", ctx);
    SIM_CHECK_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) SIM_CHECK_EQ(a[i].id, b[i].id);
    return true;
}

// --- real canon sweep --------------------------------------------------

static bool test_every_real_dialogue_speaker_is_reachable_at_act_iv() {
    const Db db = Db::load("../db/canon");
    PlayerContext endgame;
    for (const Row& r : db.rows("quests"))
        if (r.get("act") == "act_iv") endgame.completed_quests.push_back(r.get("id"));

    std::set<Id> reachable;
    std::set<std::string> speakers;
    for (const Row& row : db.rows("dialogues")) {
        if (row.get("tag") == "OPEN") continue;
        speakers.insert(row.get("speaker"));
    }
    for (const std::string& speaker : speakers)
        for (const DialogueLine& l : eligible_lines(db, speaker, endgame)) reachable.insert(l.id);

    for (const Row& row : db.rows("dialogues")) {
        if (row.get("tag") == "OPEN") continue;
        SIM_CHECK(reachable.count(row.get("id")) == 1);
    }
    return true;
}

SIM_MAIN(test_speaker_matches_role_ignoring_article_and_case,
         test_speaker_matches_person_id_with_underscores_as_spaces,
         test_act_gating_widens_as_quests_progress, test_open_row_never_surfaces,
         test_line_with_no_act_token_is_always_eligible,
         test_unknown_speaker_and_unknown_quest_ids_are_graceful,
         test_determinism_same_inputs_same_outputs,
         test_every_real_dialogue_speaker_is_reachable_at_act_iv)
