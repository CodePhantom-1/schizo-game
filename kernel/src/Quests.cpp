// Quests.cpp — the quest state machine (living-world §8): accept, complete,
// fail-on-deadline. At Wave 1 only the MACHINERY exists: db/canon/quests.csv is
// empty (header only), the seven quest sources (history arc, background,
// companion, sourced side, systemic, faction contract, emergent) arrive as
// content in later phases, and the machine runs on whatever written rows the
// canon eventually gives it — no quest names in code.
//
// Determinism: the machine needs no randomness at all (the deadline arithmetic
// is exact; no ctx.rng draws), uses no wall clock, no global or static mutable
// state, no threads. Writes ONLY QuestState.
#include "sim/Quests.hpp"

#include "sim/Context.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace sim {
namespace {

std::string trim(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string ascii_lower(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}

// OPEN rows mark missing canon (db/schema rule quoted at Justice.cpp: "OPEN
// rows mark missing canon; they cannot ship") — plain "OPEN" or "OPEN (...)".
// An OPEN quest row is an unwritten quest: loading it would resolve a row the
// canon has not written, so it is treated as absent.
bool row_is_open(const Row& row) {
    return ascii_lower(trim(row.get("tag"))).rfind("open", 0) == 0;
}

// Strict decimal read of deadline_days. A cell that is absent (the Wave 1
// quests.csv header carries no deadline_days column at all), empty, negative,
// or not plain digits — or a number the int field cannot hold — is a deadline
// the machine cannot honor: 0 = none. Never a guessed one.
int parse_deadline_days(const std::string& s) {
    const std::string t = trim(s);
    if (t.empty()) return 0;
    long long v = 0;
    for (const char c : t) {
        if (c < '0' || c > '9') return 0;
        v = v * 10 + (c - '0');
        if (v > 2147483647LL) return 0;  // not representable in QuestDef.deadline_days
    }
    return static_cast<int>(v);
}

const QuestDef* find_def(const QuestState& state, const Id& def_id) {
    for (const QuestDef& def : state.defs)
        if (def.id == def_id) return &def;
    return nullptr;
}

}  // namespace

void load_defs(const WorldContext& ctx, QuestState& state) {
    state.defs.clear();  // reload is idempotent: the same db yields the same defs
    for (const Row& row : ctx.db.rows("quests")) {
        const std::string& id = row.get("id");
        if (id.empty()) continue;
        if (row_is_open(row)) continue;  // canon content that does not exist yet

        QuestDef def;
        def.id = id;
        def.kind = row.get("kind");  // carried as data; the machine never branches on it
        def.deadline_days = parse_deadline_days(row.get("deadline_days"));
        state.defs.push_back(std::move(def));
    }
}

void tick_quests(const WorldContext& ctx, QuestState& state, int days) {
    // Days ascend (the same convention as the other modules' ticks); days <= 0
    // advance nothing. A quest is "past its deadline" the day AFTER its
    // deadline day — the deadline day itself is still time to hand the work in
    // ("quests fail and time out; the world doesn't wait", living-world §8).
    // Expiry is checked on each evaluated day, so a quest fails exactly once:
    // the move out of `active` (only the id survives, into failed_list) removes
    // it from every later day's scan.
    for (DayNumber day = ctx.day; day < ctx.day + days; ++day) {
        std::vector<Quest> still_active;
        still_active.reserve(state.active.size());
        for (Quest& quest : state.active) {
            if (quest.deadline > 0 && day > quest.deadline)
                state.failed_list.push_back(quest.def_id);
            else
                still_active.push_back(std::move(quest));
        }
        state.active = std::move(still_active);
    }
}

Quest& accept(QuestState& state, const Id& def_id, DayNumber day) {
    Quest quest;
    quest.def_id = def_id;
    quest.accepted = day;
    // The deadline comes from the def's canon row when one exists (accepted
    // day + deadline_days). A def the canon does not represent (QuestState.defs
    // is empty at Wave 1) gets 0 = no deadline rather than an invented one.
    if (const QuestDef* def = find_def(state, def_id);
        def != nullptr && def->deadline_days > 0)
        quest.deadline = day + def->deadline_days;
    quest.stage = "accepted";
    state.active.push_back(std::move(quest));
    return state.active.back();
}

void complete(QuestState& state, const Id& def_id, DayNumber day) {
    (void)day;  // the frozen Quest record has no completed-day field to keep it in
    const auto it = std::find_if(state.active.begin(), state.active.end(),
                                 [&](const Quest& q) { return q.def_id == def_id; });
    if (it == state.active.end()) return;  // nothing active under this def: nothing to complete
    state.completed.push_back(it->def_id);  // recorded in completion order
    state.active.erase(it);
}

}  // namespace sim
