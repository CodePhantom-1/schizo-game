// test_scenario_debts.cpp — engine-binding hardening scenario: silver loans at
// the traditional 20% and the quest deadline machine, driven the way the engine
// will drive them — through a seeded WorldState (sim/World.hpp) and its daily
// advance_days, not bare module rigs.
//
// Proven here, end to end:
//  - a loan at 20% accrues exactly principal * rate / (360 * 100) per ticked
//    day (include/sim/Property.hpp), honestly truncating for small principals
//  - a part payment clears accrued interest first, then principal; the balance
//    and the following days' accrual follow the reduced principal
//  - a full repayment closes the loan: settled paper accrues nothing and
//    refuses further payment
//  - a quest accepted with a deadline fails exactly once, the morning after
//    its deadline day, into failed_list; a quest without a deadline never does
//  - steward tablet lines appear as assets earn (one per ticked day, newest last)
//  - the whole scenario is byte-deterministic under a fixed seed
//
// db/canon/quests.csv is header-only at Wave 1, so a deadline quest is accepted
// through the documented per-accept path (src/Quests.cpp reads the deadline
// from the defs the state carries): a fixture QuestDef stands in for the canon
// row a later wave will write. Fixture ids below are test data, not canon.
#include "sim/World.hpp"

#include "sim/Test.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace sim;

namespace {

const char* kCanon = "../db/canon";  // ctest runs from kernel/ (CMakeLists sets WORKING_DIRECTORY)
const Id kCity = "city_of_jewels";   // a CANON row of db/canon/cities.csv
const Id kCreditor = "the_empire";   // a CANON row of db/canon/factions.csv

// A deterministic serialization of everything this scenario touches: same
// bytes => same state (mirrors the fingerprints of test_property.cpp).
std::string fingerprint(const WorldState& w) {
    std::string out = "day=" + std::to_string(w.day) + ";";
    const PropertyState& p = w.property;
    out += "next=" + std::to_string(p.next_id) + ";";
    for (const Asset& a : p.assets)
        out += a.id + "|" + a.kind + "|" + a.owner + "|" + a.deed_tablet_id + "|" +
               std::to_string(a.income_per_day) + "|" + a.city + "#";
    for (const Loan& l : p.loans)
        out += l.id + "|" + l.debtor + "|" + l.creditor + "|" +
               std::to_string(l.principal) + "|" + std::to_string(l.rate_pct) + "|" +
               std::to_string(l.issued) + "|" + std::to_string(l.due) + "|" +
               (l.repaid ? "R" : "O") + "|" + std::to_string(l.accrued) + "#";
    for (const std::string& line : p.steward_reports) out += line + "~";
    const QuestState& q = w.quests;
    for (const QuestDef& d : q.defs)
        out += d.id + "/" + d.kind + "/" + std::to_string(d.deadline_days) + ";";
    for (const Quest& a : q.active)
        out += a.def_id + "@" + std::to_string(a.accepted) + "d" +
               std::to_string(a.deadline) + ":" + a.stage + ";";
    for (const Id& c : q.completed) out += "C" + c + ";";
    for (const Id& f : q.failed_list) out += "X" + f + ";";
    return out;
}

// --- the loan: accrual follows the documented formula ------------------------

static bool test_loan_at_twenty_percent_accrues_the_documented_formula() {
    WorldState w;
    w.init(kCanon, 4242);
    SIM_CHECK_EQ(w.day, DayNumber{1});

    // Four principals at the traditional 20%: the documented daily dose is
    // principal * rate / (360 * 100), truncating (include/sim/Property.hpp):
    //   36000 -> 20/day; 1800 -> 1/day; 1000 -> 0/day; 35900 -> 19/day.
    issue_loan(w.property, "player", kCreditor, 36000, 20, w.day, 360);      // loan_1
    issue_loan(w.property, "npc_scribe", kCreditor, 1800, 20, w.day, 360);   // loan_2
    issue_loan(w.property, "npc_herder", kCreditor, 1000, 20, w.day, 360);   // loan_3
    issue_loan(w.property, "npc_merchant", kCreditor, 35900, 20, w.day, 360);  // loan_4

    w.advance_days(10);  // ten ticked mornings, one dose each
    SIM_CHECK_EQ(w.day, DayNumber{11});

    const Loan* big = find_loan(w.property, "loan_1");
    const Loan* mid = find_loan(w.property, "loan_2");
    const Loan* tiny = find_loan(w.property, "loan_3");
    const Loan* odd = find_loan(w.property, "loan_4");
    SIM_CHECK(big != nullptr && mid != nullptr && tiny != nullptr && odd != nullptr);
    SIM_CHECK_EQ(big->accrued, Silver{200});  // 10 x (36000 * 20 / 36000)
    SIM_CHECK_EQ(mid->accrued, Silver{10});   // 10 x (1800 * 20 / 36000)
    SIM_CHECK_EQ(tiny->accrued, Silver{0});   // 1000 * 20 / 36000 truncates to 0/day
    SIM_CHECK_EQ(odd->accrued, Silver{190});  // 10 x (35900 * 20 / 36000) = 10 x 19
    for (const Loan* l : {big, mid, tiny, odd}) {
        SIM_CHECK(!l->repaid);
        SIM_CHECK_EQ(l->rate_pct, 20);
    }

    // The steward summed the same doses on every morning's tablet.
    SIM_CHECK_EQ(w.property.steward_reports.size(), std::size_t{10});
    SIM_CHECK_EQ(w.property.steward_reports.front(),
                 std::string("day 1: income 0 silver from 0 assets; "
                             "interest 40 silver on 4 loans; overdue 0"));
    SIM_CHECK(w.property.steward_reports.back().rfind("day 10: ", 0) == 0);
    return true;
}

// --- the loan storyline: part payment, balance, full settlement ---------------

static bool test_loan_storyline_part_payment_then_full_repayment() {
    WorldState w;
    w.init(kCanon, 4242);
    const Id id = issue_loan(w.property, "player", kCreditor, 36000, 20, w.day, 360).id;
    SIM_CHECK_EQ(id, Id("loan_1"));

    w.advance_days(10);
    const Loan* l = find_loan(w.property, id);
    SIM_CHECK_EQ(l->accrued, Silver{200});  // 20/day, exact per the documented formula
    SIM_CHECK_EQ(w.property.steward_reports.size(), std::size_t{10});
    SIM_CHECK_EQ(w.property.steward_reports.front(),
                 std::string("day 1: income 0 silver from 0 assets; "
                             "interest 20 silver on 1 loans; overdue 0"));

    // A part payment clears the accrued interest first, then the principal:
    // 700 = 200 interest + 500 principal. The balance is 35500, nothing repaid.
    SIM_CHECK(repay_loan(w.property, id, 700));
    l = find_loan(w.property, id);
    SIM_CHECK_EQ(l->accrued, Silver{0});
    SIM_CHECK_EQ(l->principal, Silver{35500});
    SIM_CHECK(!l->repaid);

    // Interest now follows the reduced principal, truncating honestly:
    // 35500 * 20 / 36000 = 19/day.
    w.advance_days(10);
    l = find_loan(w.property, id);
    SIM_CHECK_EQ(l->accrued, Silver{190});
    SIM_CHECK(w.property.steward_reports.back().rfind("day 20: ", 0) == 0);
    SIM_CHECK(w.property.steward_reports.back().find("interest 19 silver on 1 loans") !=
              std::string::npos);

    // The balance: 35500 principal + 190 accrued = 35690. Paying exactly that
    // closes the loan.
    SIM_CHECK_EQ(l->accrued + l->principal, Silver{35690});
    SIM_CHECK(repay_loan(w.property, id, 35690));
    l = find_loan(w.property, id);
    SIM_CHECK_EQ(l->principal, Silver{0});
    SIM_CHECK_EQ(l->accrued, Silver{0});
    SIM_CHECK(l->repaid);

    // Settled paper: further payment is refused, nothing accrues, and the
    // steward's tablet says the books are clear.
    SIM_CHECK(!repay_loan(w.property, id, 1));
    w.advance_days(5);
    l = find_loan(w.property, id);
    SIM_CHECK_EQ(l->accrued, Silver{0});
    SIM_CHECK(l->repaid);
    SIM_CHECK_EQ(w.property.steward_reports.size(), std::size_t{25});  // one per ticked day
    SIM_CHECK(w.property.steward_reports.back().rfind("day 25: ", 0) == 0);
    SIM_CHECK(w.property.steward_reports.back().find("interest 0 silver on 0 loans") !=
              std::string::npos);
    SIM_CHECK_EQ(w.day, DayNumber{26});
    return true;
}

// --- the quest: a deadline fails exactly once ---------------------------------

static bool test_deadline_quest_fails_exactly_once_into_the_failed_list() {
    WorldState w;
    w.init(kCanon, 99);
    // db/canon/quests.csv is live now (Act I data): its defs load in init and
    // ride alongside the fixture below — accept() resolves by id, so the
    // fixture (q_dredge_the_canal) is unaffected by canon defs.
    SIM_CHECK(w.quests.defs.size() >= 12);  // the Act I canon defs loaded

    // A deadline quest: the machine takes deadline_days per-accept from the
    // defs the state carries, so a fixture def stands in for the canon row a
    // later wave will write. Accepted day 1 + 3 days -> deadline day 4.
    QuestDef dredge;
    dredge.id = "q_dredge_the_canal";
    dredge.kind = "systemic";
    dredge.deadline_days = 3;
    w.quests.defs.push_back(dredge);

    Quest& q = accept(w.quests, Id{"q_dredge_the_canal"}, w.day);
    SIM_CHECK_EQ(q.accepted, DayNumber{1});
    SIM_CHECK_EQ(q.deadline, DayNumber{4});
    SIM_CHECK_EQ(q.stage, std::string("accepted"));
    SIM_CHECK(!q.failed);

    // Three days of mornings: days 1..3 ticked, still before the deadline.
    w.advance_days(3);
    SIM_CHECK_EQ(w.day, DayNumber{4});
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK(w.quests.failed_list.empty());

    // The deadline day itself is still time to hand the work in.
    w.advance_days(1);  // ticks day 4
    SIM_CHECK_EQ(w.day, DayNumber{5});
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK(w.quests.failed_list.empty());

    // The morning after: tick_quests fails it, into failed_list.
    w.advance_days(1);  // ticks day 5
    SIM_CHECK_EQ(w.day, DayNumber{6});
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.failed_list.front(), std::string("q_dredge_the_canal"));

    // Exactly once: ten more mornings and an explicit multi-day tick_quests
    // both find nothing left to fail.
    w.advance_days(10);
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{1});
    tick_quests(w.context(), w.quests, 7);
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.failed_list.front(), std::string("q_dredge_the_canal"));
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK(w.quests.completed.empty());
    return true;
}

// --- the quest: no deadline, no failure ----------------------------------------

static bool test_quest_without_deadline_never_fails() {
    WorldState w;
    w.init(kCanon, 99);

    // Two flavours of "no deadline": a def whose deadline_days is 0, and an id
    // the canon does not represent at all. Both accept with deadline 0.
    QuestDef stars;
    stars.id = "q_read_the_stars";
    stars.kind = "background";
    stars.deadline_days = 0;
    w.quests.defs.push_back(stars);
    accept(w.quests, Id{"q_read_the_stars"}, w.day);
    accept(w.quests, Id{"q_never_written"}, w.day);
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{2});
    for (const Quest& quest : w.quests.active) SIM_CHECK_EQ(quest.deadline, DayNumber{0});

    // Well over a year of mornings: the world doesn't wait, but nothing was
    // ever due — so nothing fails.
    w.advance_days(500);
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{2});
    SIM_CHECK(w.quests.failed_list.empty());
    SIM_CHECK(w.quests.completed.empty());
    return true;
}

// --- the steward: tablets appear as assets earn ---------------------------------

static bool test_steward_lines_appear_as_assets_earn() {
    WorldState w;
    w.init(kCanon, 4242);

    // Nothing to steward: the steward writes nothing.
    w.advance_days(5);
    SIM_CHECK(w.property.steward_reports.empty());

    // An earning asset with its deed tablet (ownership is written): one line
    // per ticked day, newest last, reporting the day's income.
    const Asset& field = buy_asset(w.property, "field", "player", kCity, 5, true);
    SIM_CHECK_EQ(field.id, Id("asset_1"));
    SIM_CHECK_EQ(field.deed_tablet_id, Id("deed_asset_1"));

    w.advance_days(3);  // ticks days 6..8
    SIM_CHECK_EQ(w.property.steward_reports.size(), std::size_t{3});
    SIM_CHECK_EQ(w.property.steward_reports[0],
                 std::string("day 6: income 5 silver from 1 assets; "
                             "interest 0 silver on 0 loans; overdue 0"));
    SIM_CHECK(w.property.steward_reports[1].rfind("day 7: ", 0) == 0);
    SIM_CHECK(w.property.steward_reports[1].find("income 5 silver from 1 assets") !=
              std::string::npos);
    SIM_CHECK(w.property.steward_reports[2].rfind("day 8: ", 0) == 0);

    // A second asset earns too: the tablet reports the day's total, unwritten
    // deeds marked contestable.
    const Asset& orchard = buy_asset(w.property, "orchard", "npc_gardener", kCity, 2, false);
    SIM_CHECK_EQ(orchard.id, Id("asset_2"));
    SIM_CHECK(orchard.deed_tablet_id.empty());

    w.advance_days(2);  // ticks days 9..10
    SIM_CHECK_EQ(w.property.steward_reports.size(), std::size_t{5});
    SIM_CHECK_EQ(w.property.steward_reports[3],
                 std::string("day 9: income 7 silver from 2 assets; "
                             "interest 0 silver on 0 loans; overdue 0"));
    SIM_CHECK(w.property.steward_reports[4].rfind("day 10: ", 0) == 0);
    SIM_CHECK(w.property.steward_reports[4].find("income 7 silver from 2 assets") !=
              std::string::npos);
    SIM_CHECK_EQ(w.day, DayNumber{11});
    return true;
}

// --- the whole scenario is byte-deterministic ------------------------------------

static bool test_scenario_is_byte_deterministic_under_a_fixed_seed() {
    // The engine path, end to end: an asset, a 20% loan, a part payment, a
    // deadline quest and a deadline-less one, ten mornings in two chunks.
    auto run = [](WorldState& w) {
        w.init(kCanon, 4242);
        buy_asset(w.property, "field", "player", kCity, 5, true);                // asset_1
        issue_loan(w.property, "player", kCreditor, 36000, 20, w.day, 360);      // loan_2
        QuestDef dredge;
        dredge.id = "q_dredge_the_canal";
        dredge.kind = "systemic";
        dredge.deadline_days = 3;
        w.quests.defs.push_back(dredge);
        accept(w.quests, Id{"q_dredge_the_canal"}, w.day);
        accept(w.quests, Id{"q_never_written"}, w.day);
        w.advance_days(6);  // days 1..6: the dredge quest times out on day 5
        (void)repay_loan(w.property, "loan_2", 700);  // 120 accrued + 580 principal
        w.advance_days(4);  // days 7..10
        return fingerprint(w);
    };

    WorldState a;
    WorldState b;
    const std::string fa = run(a);
    const std::string fb = run(b);
    SIM_CHECK_EQ(fa, fb);

    // Non-vacuous: the run actually did what the scenario describes. The loan
    // is loan_2 (the asset took asset_1's slot in the shared id counter); its
    // accrued 120 was cleared first, then 580 off the principal; 35420 * 20 /
    // 36000 truncates to 19/day, so four more mornings accrue 76.
    const Loan* loan = find_loan(a.property, "loan_2");
    SIM_CHECK(loan != nullptr);
    SIM_CHECK_EQ(a.day, DayNumber{11});
    SIM_CHECK_EQ(loan->principal, Silver{35420});
    SIM_CHECK_EQ(loan->accrued, Silver{76});
    SIM_CHECK(!loan->repaid);
    SIM_CHECK_EQ(a.property.steward_reports.size(), std::size_t{10});
    SIM_CHECK_EQ(a.quests.failed_list.size(), std::size_t{1});
    SIM_CHECK_EQ(a.quests.failed_list.front(), std::string("q_dredge_the_canal"));
    SIM_CHECK_EQ(a.quests.active.size(), std::size_t{1});
    SIM_CHECK_EQ(a.quests.active.front().def_id, std::string("q_never_written"));
    return true;
}

}  // namespace

SIM_MAIN(test_loan_at_twenty_percent_accrues_the_documented_formula,
         test_loan_storyline_part_payment_then_full_repayment,
         test_deadline_quest_fails_exactly_once_into_the_failed_list,
         test_quest_without_deadline_never_fails,
         test_steward_lines_appear_as_assets_earn,
         test_scenario_is_byte_deterministic_under_a_fixed_seed)
