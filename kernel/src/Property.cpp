// Property.cpp — assets, deeds, and silver loans at the traditional rate
// (implements the frozen API in include/sim/Property.hpp; mechanics row 27 via
// rpg-systems §5: houses, fields, orchards, herds, workshops, ship shares,
// caravan partnerships, loans at the traditional rates, deeds as tablets,
// steward reports).
//
// Determinism notes:
//  - pure integer arithmetic over PropertyState plus ctx.day: interest and
//    the steward tablet are exact functions of the state and the ticked day,
//    so no draws are needed and ctx.rng is left untouched on purpose
//    (identical (state, inputs) => identical bytes; no wall clock, no global
//    or static mutable state, no threads).
//  - writes ONLY PropertyState; the world is read through the const
//    WorldContext. No canon is consulted: db/canon has no property table and
//    kinds, cities, debtors and creditors arrive as string ids, so there is
//    nothing to resolve — OPEN rows are untouched by construction.
//
// INVENTED machinery (no canon exists for it yet — items.csv is empty, so a
// deed tablet cannot be a canon item): the "deed_<asset id>" tablet id, the
// shared id counter (the one PropertyState::next_id serves both the
// "asset_<n>" and "loan_<n>" prefixes in creation order), the allocation of a
// repayment (accrued interest first, then principal — mechanical bookkeeping
// only: interest accrues on the principal alone, so the order never changes
// future accrual), and the daily steward tablet (see tick_property).
//
// Open seam, not invented around: PropertyState has no purse. Income
// "accrues" observably on the steward tablet, but crediting silver to an
// owner would mean writing another module's state (or a field the frozen
// header does not carry) — crediting is a later-wave integration.
#include "sim/Property.hpp"

#include "sim/Context.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace sim {
namespace {

// The header's prescribed daily interest: principal * rate / (360 * 100),
// integral (truncating) on purpose — silver is integer grains, so a small
// principal honestly truncates to 0 on most days (100 silver at 20%/yr is
// 5.5/yr). The guard only keeps an absurd tablet (a principal whose product
// with the rate would overflow Silver) from undefined behavior; it accrues 0.
Silver daily_interest(const Loan& loan) {
    if (loan.principal == 0 || loan.rate_pct == 0) return 0;
    if (loan.principal == std::numeric_limits<Silver>::min()) return 0;
    const Silver magnitude = loan.principal < 0 ? -loan.principal : loan.principal;
    const int rate = loan.rate_pct < 0 ? -loan.rate_pct : loan.rate_pct;
    if (magnitude > std::numeric_limits<Silver>::max() / rate) return 0;
    return loan.principal * loan.rate_pct / (360 * 100);
}

Loan* find_loan_mut(PropertyState& state, const Id& loan_id) {
    const auto it = std::find_if(state.loans.begin(), state.loans.end(),
                                 [&](const Loan& l) { return l.id == loan_id; });
    return it == state.loans.end() ? nullptr : &*it;
}

}  // namespace

// --- the contract -----------------------------------------------------------

void tick_property(const WorldContext& ctx, PropertyState& state, int days) {
    if (days <= 0) return;
    for (int i = 0; i < days; ++i) {
        const DayNumber day = ctx.day + i;  // the i-th of the requested days

        // Interest accrues per tick, exactly as the header prescribes. Settled
        // paper (repaid) no longer accrues; a loan accrues past its due date
        // too — the formula is unconditional, and what overdue means for the
        // debtor is a later wave's decision.
        Silver interest_today = 0;
        int accruing = 0;
        int overdue = 0;
        for (Loan& loan : state.loans) {
            if (loan.repaid) continue;
            const Silver interest = daily_interest(loan);
            loan.accrued += interest;
            interest_today += interest;
            ++accruing;
            // due <= 0 is "no due date" (the struct's zero default); the due
            // day itself is not overdue — the morning after it is.
            if (loan.due > 0 && day > loan.due) ++overdue;
        }

        // The day's income: one dose per asset, credited to the owner's purse
        // (D-017 — income has a wallet to land in), and reported on the tablet.
        std::map<Id, Silver> income_by_owner;
        for (const Asset& asset : state.assets) income_by_owner[asset.owner] += asset.income_per_day;
        Silver income_today = 0;
        for (const auto& [owner, income] : income_by_owner) {
            state.purse_by_owner[owner] += income;
            income_today += income;
        }

        // One steward tablet per ticked day, newest last, while there is
        // anything to steward: a steward with no assets and no loans writes
        // nothing.
        if (!state.assets.empty() || !state.loans.empty()) {
            state.steward_reports.push_back(
                "day " + std::to_string(day) + ": income " +
                std::to_string(income_today) + " silver from " +
                std::to_string(state.assets.size()) + " assets; interest " +
                std::to_string(interest_today) + " silver on " +
                std::to_string(accruing) + " loans; overdue " +
                std::to_string(overdue));
        }
    }
}

Asset& buy_asset(PropertyState& state, const Id& kind, const Id& owner,
                 const Id& city, Silver income_per_day, bool with_deed) {
    Asset asset;
    asset.id = "asset_" + std::to_string(state.next_id);
    asset.kind = kind;
    asset.owner = owner;
    asset.city = city;
    asset.income_per_day = income_per_day;
    // Ownership is written: the deed tablet is an item id. Items are a later
    // wave, so the tablet is a mechanical string id derived from the asset;
    // "" = unwritten (contestable).
    if (with_deed) asset.deed_tablet_id = "deed_" + asset.id;
    ++state.next_id;
    state.assets.push_back(std::move(asset));
    return state.assets.back();
}

Loan& issue_loan(PropertyState& state, const Id& debtor, const Id& creditor,
                 Silver principal, int rate_pct, DayNumber day, int term_days) {
    Loan loan;
    loan.id = "loan_" + std::to_string(state.next_id);
    loan.debtor = debtor;
    loan.creditor = creditor;
    loan.principal = principal;
    loan.rate_pct = rate_pct;
    loan.issued = day;
    loan.due = day + term_days;
    ++state.next_id;
    state.loans.push_back(std::move(loan));
    return state.loans.back();
}

bool repay_loan(PropertyState& state, const Id& loan_id, Silver amount) {
    if (amount <= 0) return false;
    Loan* loan = find_loan_mut(state, loan_id);
    if (loan == nullptr || loan->repaid) return false;

    const Silver owed = loan->accrued + loan->principal;
    if (owed <= 0) {  // nothing owed (a zero tablet, or a nonsense rate): settled
        loan->accrued = 0;
        loan->principal = 0;
        loan->repaid = true;
        return true;
    }

    // The payment clears the accrued interest first, then the principal; an
    // overpayment is clamped to what is owed — the creditor takes no more.
    const Silver paid = amount < owed ? amount : owed;
    const Silver on_interest = loan->accrued < paid ? loan->accrued : paid;
    loan->accrued -= on_interest;
    loan->principal -= paid - on_interest;
    loan->repaid = loan->accrued + loan->principal == 0;
    return true;
}

const Asset* find_asset(const PropertyState& state, const Id& asset_id) {
    const auto it = std::find_if(state.assets.begin(), state.assets.end(),
                                 [&](const Asset& a) { return a.id == asset_id; });
    return it == state.assets.end() ? nullptr : &*it;
}

const Loan* find_loan(const PropertyState& state, const Id& loan_id) {
    const auto it = std::find_if(state.loans.begin(), state.loans.end(),
                                 [&](const Loan& l) { return l.id == loan_id; });
    return it == state.loans.end() ? nullptr : &*it;
}


// Purse access (D-017): clamped at zero; credit never negative.
Silver purse(const PropertyState& state, const Id& owner) {
    const auto it = state.purse_by_owner.find(owner);
    return it == state.purse_by_owner.end() ? Silver{0} : it->second;
}

void credit_purse(PropertyState& state, const Id& owner, Silver amount) {
    if (amount > 0) state.purse_by_owner[owner] += amount;
}

bool take_from_purse(PropertyState& state, const Id& owner, Silver amount) {
    if (amount < 0) return false;
    const Silver current = purse(state, owner);
    if (amount > current) return false;
    state.purse_by_owner[owner] = current - amount;
    return true;
}
}  // namespace sim
