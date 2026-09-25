#pragma once
// Property.hpp — CONTRACT (implemented by a fleet agent; do not change API).
// What can be owned (rpg-systems §5): houses, fields, orchards, herds,
// workshops, ship shares, caravan partnerships — and loans at the traditional
// rates (20% silver / 33⅓% barley [C]; the kernel tracks silver loans).
// Ownership is written: an asset without its deed tablet is contestable.
//
// Invariants:
//  - income accrues per tick; loans accrue interest per tick and can be repaid
//  - deeds reference items by id (the item system is a later wave; the tablet
//    is a string id for now)
//  - deterministic; writes ONLY PropertyState
#include "sim/Types.hpp"

#include <map>
#include <string>
#include <vector>


namespace sim {

struct WorldContext;  // defined in sim/Context.hpp (the seam — never included by module headers)

struct Asset {
    Id id;               // "asset_<n>"
    Id kind;             // house|field|orchard|herd|workshop|ship_share|caravan_partnership
    Id owner;            // npc id or "player"
    Id deed_tablet_id;   // item id of the deed; "" = unwritten (contestable)
    Silver income_per_day = 0;
    Id city;
};

struct Loan {
    Id id;               // "loan_<n>"
    Id debtor;
    Id creditor;         // npc id or faction/market id
    Silver principal = 0;
    int rate_pct = 20;   // per year, applied per tick as principal * rate / (360 * 100)
    DayNumber issued = 0;
    DayNumber due = 0;
    bool repaid = false;
    Silver accrued = 0;
};

struct PropertyState {
    std::vector<Asset> assets;
    std::vector<Loan> loans;
    // The purse: income credited per tick, by owner (npc id or "player").
    // D-017 closes the D-011 deferral — income has a wallet to land in.
    std::map<Id, Silver> purse_by_owner;
    std::vector<std::string> steward_reports;  // diegetic tablet lines, newest last
    int next_id = 1;
};

void tick_property(const WorldContext& ctx, PropertyState& state, int days = 1);

Asset& buy_asset(PropertyState& state, const Id& kind, const Id& owner,
                 const Id& city, Silver income_per_day, bool with_deed);
Loan& issue_loan(PropertyState& state, const Id& debtor, const Id& creditor,
                 Silver principal, int rate_pct, DayNumber day, int term_days);
bool repay_loan(PropertyState& state, const Id& loan_id, Silver amount);

// Purse access: income lands here per tick; the engine reads it as the
// owner's silver-on-hand. take_from_purse is clamped (false when short).
Silver purse(const PropertyState& state, const Id& owner);
void credit_purse(PropertyState& state, const Id& owner, Silver amount);
bool take_from_purse(PropertyState& state, const Id& owner, Silver amount);

const Asset* find_asset(const PropertyState& state, const Id& asset_id);
const Loan* find_loan(const PropertyState& state, const Id& loan_id);

}  // namespace sim
