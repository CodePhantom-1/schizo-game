// test_property.cpp — assets, deeds, and silver loans at the traditional rate.
// Tests the contract in include/sim/Property.hpp: creation and the shared id
// counter, deed tablets (written vs contestable), the per-tick accrual of
// interest (the header's exact formula) and the reported daily income,
// repayment (interest first, overpayment clamped), overdue steward tablets,
// graceful absence, and byte determinism.
#include "sim/Context.hpp"

#include "sim/Test.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace sim;

namespace {

// A small deterministic world to tick against, mirroring the coordinator's
// construction (test_contracts.cpp). ctx holds const refs to the members.
struct Rig {
    Db db;
    Rng rng{7};
    Calendar cal{};
    WorldFacts facts{};
    EconomyState economy;
    PopulationState population;
    FactionState faction;
    MagicState magic;
    JusticeState justice;
    EventsState events;
    PropertyState property;
    QuestState quests;
    NeedsState needs;
    std::map<Id, Inventory> inventories;
    WorldContext ctx;

    explicit Rig(std::uint64_t seed = 7, DayNumber day = 1)
        : db(Db::load("../db/canon")), rng(seed),
          ctx{db,       rng,      day,      cal,      facts,    economy, population,
              faction,  magic,    justice,  events,   property, quests, needs, inventories} {}
};

// A deterministic serialization of the state: same bytes => same state.
std::string fingerprint(const PropertyState& s) {
    std::string out = "next=" + std::to_string(s.next_id) + ";";
    for (const Asset& a : s.assets) {
        out += a.id + "|" + a.kind + "|" + a.owner + "|" + a.deed_tablet_id + "|" +
               std::to_string(a.income_per_day) + "|" + a.city + "#";
    }
    for (const Loan& l : s.loans) {
        out += l.id + "|" + l.debtor + "|" + l.creditor + "|" +
               std::to_string(l.principal) + "|" + std::to_string(l.rate_pct) + "|" +
               std::to_string(l.issued) + "|" + std::to_string(l.due) + "|" +
               (l.repaid ? "R" : "O") + "|" + std::to_string(l.accrued) + "#";
    }
    for (const std::string& line : s.steward_reports) out += line + "~";
    return out;
}

// --- creation ---------------------------------------------------------------

static bool test_buy_asset_ids_and_deeds() {
    Rig rig;
    Asset& a1 = buy_asset(rig.property, "field", "player", "city_of_jewels", 5, true);
    SIM_CHECK_EQ(a1.id, "asset_1");
    SIM_CHECK_EQ(a1.kind, "field");
    SIM_CHECK_EQ(a1.owner, "player");
    SIM_CHECK_EQ(a1.city, "city_of_jewels");
    SIM_CHECK_EQ(a1.income_per_day, Silver{5});
    SIM_CHECK_EQ(a1.deed_tablet_id, "deed_asset_1");  // ownership is written
    SIM_CHECK(&a1 == &rig.property.assets[0]);  // the reference is the stored asset

    Asset& a2 = buy_asset(rig.property, "herd", "npc_herder", "city_of_the_sky", 3, false);
    SIM_CHECK_EQ(a2.id, "asset_2");
    SIM_CHECK(a2.deed_tablet_id.empty());  // unwritten: contestable
    SIM_CHECK(&a2 == &rig.property.assets[1]);

    SIM_CHECK_EQ(rig.property.assets.size(), std::size_t{2});
    SIM_CHECK_EQ(rig.property.next_id, 3);
    return true;
}

static bool test_issue_loan_records_terms_and_shares_the_counter() {
    Rig rig;
    const Asset& a = buy_asset(rig.property, "workshop", "player", "city_of_kings", 10, true);
    SIM_CHECK_EQ(a.id, "asset_1");

    Loan& l = issue_loan(rig.property, "npc_merchant", "market_of_ur", 36000, 20, 100, 30);
    SIM_CHECK_EQ(l.id, "loan_2");  // one PropertyState::next_id serves both prefixes
    SIM_CHECK_EQ(l.debtor, "npc_merchant");
    SIM_CHECK_EQ(l.creditor, "market_of_ur");
    SIM_CHECK_EQ(l.principal, Silver{36000});
    SIM_CHECK_EQ(l.rate_pct, 20);
    SIM_CHECK_EQ(l.issued, DayNumber{100});
    SIM_CHECK_EQ(l.due, DayNumber{130});
    SIM_CHECK(!l.repaid);
    SIM_CHECK_EQ(l.accrued, Silver{0});

    Loan& l2 = issue_loan(rig.property, "player", "npc_usurer", 1800, 33, 100, 0);
    SIM_CHECK_EQ(l2.id, "loan_3");
    SIM_CHECK_EQ(l2.due, DayNumber{100});  // term 0: due the day it is issued
    SIM_CHECK_EQ(rig.property.next_id, 4);
    return true;
}

static bool test_find_asset_and_find_loan() {
    Rig rig;
    buy_asset(rig.property, "field", "player", "city_of_jewels", 5, true);
    issue_loan(rig.property, "player", "market_of_ur", 3600, 20, 1, 30);

    const Asset* a = find_asset(rig.property, "asset_1");
    SIM_CHECK(a != nullptr);
    SIM_CHECK_EQ(a->kind, "field");
    SIM_CHECK(find_asset(rig.property, "asset_2") == nullptr);
    SIM_CHECK(find_asset(rig.property, "") == nullptr);

    const Loan* l = find_loan(rig.property, "loan_2");
    SIM_CHECK(l != nullptr);
    SIM_CHECK_EQ(l->principal, Silver{3600});
    SIM_CHECK(find_loan(rig.property, "loan_1") == nullptr);
    return true;
}

// --- the tick: accrual and tablets -------------------------------------------

static bool test_tick_accrues_interest_exactly_and_reports_income() {
    Rig rig;
    buy_asset(rig.property, "field", "player", "city_of_jewels", 5, true);
    Loan& l = issue_loan(rig.property, "npc_merchant", "market_of_ur", 36000, 20, 1, 360);
    // 36000 * 20 / (360 * 100) = 20 silver/day — the traditional 20%/yr, exact.
    tick_property(rig.ctx, rig.property, 10);
    SIM_CHECK_EQ(l.accrued, Silver{200});
    SIM_CHECK(!l.repaid);

    // The steward tablet: one line per ticked day, newest last.
    SIM_CHECK_EQ(rig.property.steward_reports.size(), std::size_t{10});
    SIM_CHECK_EQ(rig.property.steward_reports.front(),
                 std::string("day 1: income 5 silver from 1 assets; "
                             "interest 20 silver on 1 loans; overdue 0"));
    SIM_CHECK(rig.property.steward_reports.back().rfind("day 10: ", 0) == 0);

    // Small principals truncate honestly: 100 * 20 / 36000 = 0 per day.
    Loan& small = issue_loan(rig.property, "player", "npc_usurer", 100, 20, 1, 360);
    tick_property(rig.ctx, rig.property, 5);
    SIM_CHECK_EQ(small.accrued, Silver{0});

    // Simple interest on the principal only: 10 + 5 + 10 ticked days, one
    // 20-silver dose each, on the principal alone. (The second issue may have
    // reallocated the loans vector, so the big loan is re-fetched by id
    // instead of through the stale reference.)
    tick_property(rig.ctx, rig.property, 10);
    const Loan* big = find_loan(rig.property, "loan_2");
    SIM_CHECK(big != nullptr);
    SIM_CHECK_EQ(big->accrued, Silver{500});
    SIM_CHECK_EQ(small.accrued, Silver{0});
    return true;
}

static bool test_multi_day_tick_stamps_every_day() {
    Rig rig(7, 100);  // ctx.day = 100
    buy_asset(rig.property, "workshop", "player", "city_of_kings", 1, true);
    tick_property(rig.ctx, rig.property, 5);
    const std::vector<std::string>& reports = rig.property.steward_reports;
    SIM_CHECK_EQ(reports.size(), std::size_t{5});
    for (int i = 0; i < 5; ++i) {
        const std::string prefix = "day " + std::to_string(100 + i) + ": ";
        SIM_CHECK(reports[i].rfind(prefix, 0) == 0);  // newest last, in order
    }
    return true;
}

static bool test_steward_reports_overdue_loans() {
    Rig rig;
    issue_loan(rig.property, "npc_merchant", "market_of_ur", 36000, 20, 1, 10);  // due day 11
    for (DayNumber d = 1; d <= 13; ++d) {
        rig.ctx.day = d;
        tick_property(rig.ctx, rig.property, 1);
    }
    const std::vector<std::string>& reports = rig.property.steward_reports;
    SIM_CHECK_EQ(reports.size(), std::size_t{13});
    // The due day itself is not overdue; the morning after it is.
    SIM_CHECK(reports[10].find("overdue 0") != std::string::npos);  // day 11
    SIM_CHECK(reports[11].find("overdue 1") != std::string::npos);  // day 12
    SIM_CHECK(reports[12].find("overdue 1") != std::string::npos);  // day 13

    // Repaying clears the flag on the next morning's tablet.
    SIM_CHECK(repay_loan(rig.property, "loan_1", 1000000));
    rig.ctx.day = 14;
    tick_property(rig.ctx, rig.property, 1);
    SIM_CHECK(reports.back().find("overdue 0") != std::string::npos);

    // A loan without a due date (due <= 0, the zero default) is never overdue,
    // though it still accrues.
    Loan hand;
    hand.id = "loan_hand";
    hand.debtor = "player";
    hand.creditor = "market_of_ur";
    hand.principal = 36000;
    hand.rate_pct = 20;
    rig.property.loans.push_back(hand);
    rig.ctx.day = 15;
    tick_property(rig.ctx, rig.property, 1);
    SIM_CHECK(rig.property.steward_reports.back().find("overdue 0") != std::string::npos);
    const Loan* hand_after = find_loan(rig.property, "loan_hand");
    SIM_CHECK(hand_after != nullptr);
    SIM_CHECK_EQ(hand_after->accrued, Silver{20});
    return true;
}

// --- repayment ----------------------------------------------------------------

static bool test_repay_loan_clears_interest_first() {
    Rig rig;
    Loan& l = issue_loan(rig.property, "npc_merchant", "market_of_ur", 36000, 20, 1, 360);
    tick_property(rig.ctx, rig.property, 2);
    SIM_CHECK_EQ(l.accrued, Silver{40});

    // Junk payments are refused without touching the tablet.
    SIM_CHECK(!repay_loan(rig.property, "no_such_loan", 40));
    SIM_CHECK(!repay_loan(rig.property, "loan_1", 0));
    SIM_CHECK(!repay_loan(rig.property, "loan_1", -5));
    SIM_CHECK_EQ(l.accrued, Silver{40});

    // A payment clears the accrued interest first, then the principal.
    SIM_CHECK(repay_loan(rig.property, "loan_1", 40));
    SIM_CHECK_EQ(l.accrued, Silver{0});
    SIM_CHECK_EQ(l.principal, Silver{36000});
    SIM_CHECK(!l.repaid);

    SIM_CHECK(repay_loan(rig.property, "loan_1", 50));  // partial on the principal
    SIM_CHECK_EQ(l.principal, Silver{35950});

    // Overpayment is clamped to what is owed; the loan is settled.
    SIM_CHECK(repay_loan(rig.property, "loan_1", 1000000));
    SIM_CHECK_EQ(l.principal, Silver{0});
    SIM_CHECK_EQ(l.accrued, Silver{0});
    SIM_CHECK(l.repaid);

    // Settled paper: no further payments, no further accrual.
    SIM_CHECK(!repay_loan(rig.property, "loan_1", 1));
    tick_property(rig.ctx, rig.property, 3);
    SIM_CHECK_EQ(l.accrued, Silver{0});
    SIM_CHECK_EQ(l.principal, Silver{0});
    SIM_CHECK(l.repaid);

    // A zero tablet (nothing owed at all) settles on any positive payment.
    Loan& z = issue_loan(rig.property, "player", "market_of_ur", 0, 20, 1, 360);
    SIM_CHECK(repay_loan(rig.property, "loan_2", 1));
    SIM_CHECK(z.repaid);
    return true;
}

// --- junk days and absence ------------------------------------------------------

static bool test_zero_and_negative_days_change_nothing() {
    Rig rig;
    buy_asset(rig.property, "orchard", "player", "city_of_jewels", 7, true);
    issue_loan(rig.property, "player", "market_of_ur", 3600, 20, 1, 30);
    const std::string before = fingerprint(rig.property);
    tick_property(rig.ctx, rig.property, 0);
    tick_property(rig.ctx, rig.property, -3);
    SIM_CHECK_EQ(fingerprint(rig.property), before);
    return true;
}

static bool test_absence_is_graceful() {
    Rig rig;
    const PropertyState fresh;
    SIM_CHECK(rig.property.assets.empty());
    SIM_CHECK(rig.property.loans.empty());
    SIM_CHECK(rig.property.steward_reports.empty());

    // Nothing stewarded: no tablets, no state change.
    tick_property(rig.ctx, rig.property, 10);
    SIM_CHECK_EQ(fingerprint(rig.property), fingerprint(fresh));

    SIM_CHECK(find_asset(rig.property, "asset_1") == nullptr);
    SIM_CHECK(find_loan(rig.property, "loan_1") == nullptr);
    SIM_CHECK(!repay_loan(rig.property, "loan_1", 10));
    return true;
}

// --- determinism ----------------------------------------------------------------

static bool test_tick_is_byte_deterministic() {
    // The module draws no randomness: two rigs with DIFFERENT seeds run the
    // same op sequence and must land on identical state bytes.
    auto run = [](Rig& rig) {
        buy_asset(rig.property, "field", "player", "city_of_jewels", 5, true);
        buy_asset(rig.property, "herd", "npc_herder", "city_of_the_sky", 3, false);
        issue_loan(rig.property, "npc_merchant", "market_of_ur", 36000, 20, 1, 200);
        issue_loan(rig.property, "player", "npc_usurer", 1800, 33, 10, 60);
        for (DayNumber d = 1; d <= 300; ++d) {
            rig.ctx.day = d;
            if (d == 100) (void)repay_loan(rig.property, "loan_3", 500);
            if (d == 200) (void)repay_loan(rig.property, "loan_4", 100000);
            tick_property(rig.ctx, rig.property, 1);
        }
        return fingerprint(rig.property);
    };
    Rig a(1);
    Rig b(999);
    SIM_CHECK_EQ(run(a), run(b));
    return true;
}

// --- invariants under a long soak -------------------------------------------------

static bool test_soak_invariants_hold() {
    Rig rig;
    DayNumber day = 1;
    for (int i = 0; i < 400; ++i, ++day) {
        rig.ctx.day = day;
        if (i % 50 == 0)
            buy_asset(rig.property, i % 100 == 0 ? "field" : "ship_share", "player",
                      "city_of_jewels", 2, i % 100 != 0);
        if (i % 40 == 0)
            issue_loan(rig.property, "npc_merchant", "market_of_ur", 3600 + i, 20, day, 30);
        if (i % 30 == 0) {
            for (const Loan& l : rig.property.loans)
                if (!l.repaid) {
                    (void)repay_loan(rig.property, l.id, 10);
                    break;
                }
        }
        tick_property(rig.ctx, rig.property, 1);

        // Invariants: unique ids, honest books, non-empty tablets.
        std::set<Id> ids;
        for (const Asset& a : rig.property.assets) SIM_CHECK(ids.insert(a.id).second);
        for (const Loan& l : rig.property.loans) {
            SIM_CHECK(ids.insert(l.id).second);
            SIM_CHECK(l.accrued >= 0);
            if (l.repaid) SIM_CHECK_EQ(l.accrued + l.principal, Silver{0});
        }
        for (const std::string& line : rig.property.steward_reports) SIM_CHECK(!line.empty());
        SIM_CHECK(rig.property.next_id > 0);
    }
    SIM_CHECK_EQ(rig.property.assets.size(), std::size_t{8});    // bought every 50 days
    SIM_CHECK_EQ(rig.property.loans.size(), std::size_t{10});    // issued every 40 days
    SIM_CHECK_EQ(rig.property.steward_reports.size(), std::size_t{400});  // one per ticked day
    return true;
}

}  // namespace


static bool test_purse_credits_income_per_owner_and_takes_clamp() {
    Rig rig;
    (void)buy_asset(rig.property, Id{"workshop"}, Id{"player"}, Id{"city_of_the_moon"}, 5, true);
    (void)buy_asset(rig.property, Id{"field"}, Id{"npc_steward"}, Id{"city_of_the_moon"}, 2, false);
    SIM_CHECK_EQ(purse(rig.property, Id{"player"}), Silver{0});
    tick_property(rig.ctx, rig.property, 3);
    // income credits per owner: the player 5/day, the npc 2/day
    SIM_CHECK_EQ(purse(rig.property, Id{"player"}), Silver{15});
    SIM_CHECK_EQ(purse(rig.property, Id{"npc_steward"}), Silver{6});
    credit_purse(rig.property, Id{"player"}, 10);
    SIM_CHECK_EQ(purse(rig.property, Id{"player"}), Silver{25});
    SIM_CHECK(!take_from_purse(rig.property, Id{"player"}, 30));  // clamped when short
    SIM_CHECK_EQ(purse(rig.property, Id{"player"}), Silver{25});
    SIM_CHECK(take_from_purse(rig.property, Id{"player"}, 25));
    SIM_CHECK_EQ(purse(rig.property, Id{"player"}), Silver{0});
    return true;
}

SIM_MAIN(test_buy_asset_ids_and_deeds,
         test_issue_loan_records_terms_and_shares_the_counter,
         test_find_asset_and_find_loan,
         test_tick_accrues_interest_exactly_and_reports_income,
         test_multi_day_tick_stamps_every_day,
         test_steward_reports_overdue_loans,
         test_repay_loan_clears_interest_first,
         test_zero_and_negative_days_change_nothing,
         test_absence_is_graceful,
         test_tick_is_byte_deterministic,
         test_soak_invariants_hold)
