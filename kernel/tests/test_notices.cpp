// test_notices.cpp — FND-09: the player notice feed. MECH:FND-09 MECH:UI-03
#include "sim/Notices.hpp"
#include "sim/Actions.hpp"
#include "sim/Quests.hpp"
#include "sim/Snapshot.hpp"
#include "sim/World.hpp"
#include "sim/Test.hpp"

#include <string>

using namespace sim;

static bool test_feed_keeps_the_newest_and_evicts_the_rest() {
    NoticeState s;
    SIM_CHECK(notice_at(s, 0) == nullptr);  // nothing written yet
    for (int i = 0; i < 300; ++i) push_notice(s, 1, "notice.raid", "raid " + std::to_string(i));
    SIM_CHECK_EQ(s.next_seq, 300);
    SIM_CHECK_EQ(s.recent.size(), kNoticesKept);
    SIM_CHECK(notice_at(s, 10) == nullptr);   // evicted
    SIM_CHECK(notice_at(s, 300) == nullptr);  // not yet written
    SIM_CHECK(notice_at(s, -1) == nullptr);
    SIM_CHECK_EQ(notice_at(s, 299)->text, std::string("raid 299"));
    const std::int64_t oldest = 300 - static_cast<std::int64_t>(kNoticesKept);
    SIM_CHECK_EQ(notice_at(s, oldest)->seq, oldest);
    SIM_CHECK(notice_at(s, oldest - 1) == nullptr);
    return true;
}

static bool test_raids_reach_the_feed() {
    WorldState w;
    w.init("../db/canon", 21);
    w.facts.drought_stage = 4;
    w.facts.war_stage = 3;
    const int raids_before = w.wild.next_raid;
    w.advance_days(120);
    SIM_CHECK(w.wild.next_raid > raids_before);  // the drought brings raids
    int raid_notices = 0;
    for (const Notice& n : w.notices.recent) raid_notices += n.key == "notice.raid";
    SIM_CHECK(raid_notices > 0);
    SIM_CHECK(raid_notices <= w.wild.next_raid - raids_before);
    return true;
}

static bool test_the_players_verdicts_reach_the_feed() {
    WorldState w;
    w.init("../db/canon", 21);
    (void)commit_crime(w, "player", "theft", "city_of_the_moon", {"ur_utu_gatekeeper_dawn"}, "ea_nasir_grain_trader");
    w.advance_days(kHearingDelayDays + 1);
    bool saw = false;
    for (const Notice& n : w.notices.recent) saw = saw || n.key == "notice.verdict";
    SIM_CHECK(saw);
    return true;
}

static bool test_a_failed_quest_reaches_the_feed() {
    WorldState w;
    w.init("../db/canon", 21);
    accept(w.quests, "water_in_a_breaking_world", w.day);  // deadline 2 days
    w.advance_days(5);
    bool saw = false;
    for (const Notice& n : w.notices.recent)
        saw = saw || (n.key == "notice.quest_failed" && n.text == "water_in_a_breaking_world");
    SIM_CHECK(saw);
    return true;
}

static bool test_notices_survive_a_save() {
    WorldState w;
    w.init("../db/canon", 21);
    push_notice(w.notices, 3, "notice.raid", "the reed men took sheep");
    const std::string a = save_world(w);
    WorldState back;
    load_world(back, "../db/canon", a);
    SIM_CHECK_EQ(back.notices.next_seq, 1);
    SIM_CHECK_EQ(notice_at(back.notices, 0)->text, std::string("the reed men took sheep"));
    SIM_CHECK_EQ(save_world(back), a);
    return true;
}

SIM_MAIN(test_feed_keeps_the_newest_and_evicts_the_rest, test_raids_reach_the_feed,
         test_the_players_verdicts_reach_the_feed, test_a_failed_quest_reaches_the_feed,
         test_notices_survive_a_save)
