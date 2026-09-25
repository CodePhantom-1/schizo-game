// test_scenario_quest.cpp — T7 vertical-slice scenario: one quest from a
// person with a problem, in the City of the Moon, end to end on a real
// WorldState (sim/World.hpp), plus a data-integrity sweep of the whole
// quests.csv + dialogues.csv canon.
//
// The chosen quest: "the_priestess_debt" (db/canon/quests.csv, act_i,
// emergent, deadline_days=4) — "a debtor priestess... hires the man without
// a name to collect from a grain merchant who laughs at temple marks". This
// is the best Act I fit for "a person with a problem" in the City of the
// Moon (D-005 is the arrival/slice city):
//   - a named person (a debtor priestess of the temple of sun and moon,
//     dialogues.csv:texture_debtor_priestess_ledger — "the offerings have
//     thinned since the river fell... the whole city is borrowed") with a
//     concrete problem (a debt she cannot be seen chasing herself) and a
//     concrete ask (collect from the grain merchant) — unlike the
//     survive-and-craft systemic quests (water_in_a_breaking_world, the
//     gate toll) which are chores, not a person's problem told in a voice;
//   - it deepens forward into act_ii's loan content (twenty_grains_on_the_
//     hundred is the same priestess, now as creditor herself), so the slice
//     touches a real content thread, not an orphan row;
//   - it has a deadline_days (4), giving the fail/timeout path something
//     real to expire.
//
// The Quests module (kernel/include/sim/Quests.hpp) writes ONLY QuestState:
// accept/complete/tick_quests never touch PropertyState or FactionState.
// quests.csv itself carries no reward or standing columns. So "rewards/
// standing effects as the data and module define" is, honestly, none —
// this scenario proves that boundary rather than inventing a reward the
// canon does not state (see kernel/contracts/scenario_quest.md for the
// missing-surface writeup).
#include "sim/World.hpp"

#include "sim/Test.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

using namespace sim;

namespace {

const char* kCanon = "../db/canon";  // ctest runs from kernel/
const Id kQuestId = "the_priestess_debt";

// A deterministic serialization of everything the offer/accept/progress/
// complete arc touches (mirrors test_scenario_debts.cpp's fingerprint()).
std::string fingerprint(const QuestState& q) {
    std::string out;
    for (const Quest& a : q.active)
        out += a.def_id + "@" + std::to_string(a.accepted) + "d" +
               std::to_string(a.deadline) + ":" + a.stage + ";";
    for (const Id& c : q.completed) out += "C" + c + ";";
    for (const Id& f : q.failed_list) out += "X" + f + ";";
    return out;
}

const QuestDef* find_def(const QuestState& state, const Id& def_id) {
    for (const QuestDef& def : state.defs)
        if (def.id == def_id) return &def;
    return nullptr;
}

// --- offer: the def is real Act I canon, giver is a person with a problem --

static bool test_offer_the_priestess_debt_is_act_i_canon_in_the_city_of_the_moon() {
    WorldState w;
    w.init(kCanon, 7);

    const QuestDef* def = find_def(w.quests, kQuestId);
    SIM_CHECK(def != nullptr);
    SIM_CHECK_EQ(def->kind, std::string("emergent"));
    SIM_CHECK_EQ(def->deadline_days, 4);

    // The raw canon row: act_i, the giver is a person (not a faction or a
    // board), the story text names her problem and her ask.
    const auto row = w.db.find("quests", kQuestId);
    SIM_CHECK(row.has_value());
    SIM_CHECK_EQ(row->get("act"), std::string("act_i"));
    SIM_CHECK_EQ(row->get("giver"), std::string("a debtor priestess"));
    SIM_CHECK(row->get("source_ref").find("owes what she cannot be seen chasing") !=
              std::string::npos);

    // The giver's own voice exists in dialogues.csv (the offer the player
    // would actually hear), citing the temple's thinned offerings — the same
    // drought pressure act_i's horizon names (game-design §6).
    bool found_voice = false;
    for (const Row& d : w.db.rows("dialogues")) {
        if (d.get("id") == "texture_debtor_priestess_ledger") {
            found_voice = true;
            SIM_CHECK_EQ(d.get("speaker"), std::string("a debtor priestess"));
            SIM_CHECK(d.get("text").find("owe") != std::string::npos ||
                      d.get("text").find("borrowed") != std::string::npos);
        }
    }
    SIM_CHECK(found_voice);
    return true;
}

// --- accept -> progress -> complete, before the deadline ---------------------

static bool test_accept_progress_complete_before_deadline() {
    WorldState w;
    w.init(kCanon, 7);
    SIM_CHECK_EQ(w.day, DayNumber{1});

    Quest& q = accept(w.quests, kQuestId, w.day);
    SIM_CHECK_EQ(q.def_id, kQuestId);
    SIM_CHECK_EQ(q.accepted, DayNumber{1});
    SIM_CHECK_EQ(q.deadline, DayNumber{5});  // accepted 1 + deadline_days 4
    SIM_CHECK_EQ(q.stage, std::string("accepted"));
    SIM_CHECK(!q.failed);

    // Progress: the free-form stage label is the only in-kernel trace of a
    // quest's beats (Quests.hpp: "stage; free-form stage label"); nothing in
    // the module validates it, so an engine-side dialogue/quest-journal
    // layer would drive it. Two mornings pass while the player works the
    // wharf and the lane before confronting the grain merchant.
    w.advance_days(2);
    SIM_CHECK_EQ(w.day, DayNumber{3});
    // accept()'s returned reference (q) does not survive advance_days(): the
    // tick_quests machinery move-assigns a new `active` vector each day
    // (src/Quests.cpp), invalidating references into the old one. Reach the
    // live quest back through the state, never through `q`, past a tick.
    w.quests.active.front().stage = "confronted_the_grain_merchant";
    SIM_CHECK_EQ(w.quests.active.front().stage, std::string("confronted_the_grain_merchant"));
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK(w.quests.failed_list.empty());

    // Complete on day 4, before the day-5 deadline.
    w.advance_days(1);
    SIM_CHECK_EQ(w.day, DayNumber{4});
    complete(w.quests, kQuestId, w.day);
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.completed.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.completed.front(), kQuestId);
    SIM_CHECK(w.quests.failed_list.empty());

    // Ticking on past the deadline changes nothing: it already left `active`.
    w.advance_days(5);
    SIM_CHECK(w.quests.failed_list.empty());
    SIM_CHECK_EQ(w.quests.completed.size(), std::size_t{1});

    // The boundary this scenario is here to prove: quests.csv carries no
    // reward/standing columns and the Quests module writes ONLY QuestState
    // (Quests.hpp), so completing this quest, by itself, credits no purse
    // and moves no faction standing. See scenario_quest.md.
    SIM_CHECK(w.property.purse_by_owner.empty());
    SIM_CHECK(w.faction.standing_by_faction.empty());
    return true;
}

// --- the fail/timeout path: ignored, the world doesn't wait ------------------

static bool test_fails_on_deadline_when_ignored() {
    WorldState w;
    w.init(kCanon, 7);

    Quest& q = accept(w.quests, kQuestId, w.day);
    SIM_CHECK_EQ(q.deadline, DayNumber{5});

    w.advance_days(4);  // ticks days 1..4
    SIM_CHECK_EQ(w.day, DayNumber{5});
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK(w.quests.failed_list.empty());

    // tick_quests evaluates the CURRENT (pre-increment) day, so this call
    // ticks day 5 — the deadline day itself, still live (Quests.cpp: "the
    // deadline day itself is still time to hand the work in") — and only
    // then advances the clock past it.
    w.advance_days(1);  // ticks day 5
    SIM_CHECK_EQ(w.day, DayNumber{6});
    SIM_CHECK_EQ(w.quests.active.size(), std::size_t{1});
    SIM_CHECK(w.quests.failed_list.empty());

    // The morning after: never collected from the grain merchant, the debt
    // (and the priestess's patience) runs out. This tick evaluates day 6,
    // the first day strictly past the deadline.
    w.advance_days(1);  // ticks day 6
    SIM_CHECK_EQ(w.day, DayNumber{7});
    SIM_CHECK(w.quests.active.empty());
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{1});
    SIM_CHECK_EQ(w.quests.failed_list.front(), kQuestId);

    // Exactly once, no matter how much more time passes.
    w.advance_days(20);
    SIM_CHECK_EQ(w.quests.failed_list.size(), std::size_t{1});
    SIM_CHECK(w.quests.completed.empty());
    return true;
}

// --- determinism --------------------------------------------------------------

static bool test_scenario_is_byte_deterministic_under_a_fixed_seed() {
    auto run = [](std::uint64_t seed) {
        WorldState w;
        w.init(kCanon, seed);
        accept(w.quests, kQuestId, w.day);
        w.advance_days(2);
        w.quests.active.front().stage = "confronted_the_grain_merchant";
        w.advance_days(1);
        complete(w.quests, kQuestId, w.day);
        w.advance_days(5);
        return fingerprint(w.quests);
    };
    const std::string fa = run(7);
    const std::string fb = run(7);
    SIM_CHECK_EQ(fa, fb);
    SIM_CHECK(fa.find("Cthe_priestess_debt;") != std::string::npos);
    return true;
}

// === data-integrity sweep: every row of quests.csv + dialogues.csv ==========

int act_rank(const std::string& act) {
    static const std::map<std::string, int> kOrder = {
        {"opening", 0}, {"act_i", 1}, {"act_ii", 2}, {"act_iii", 3}, {"act_iv", 4}};
    const auto it = kOrder.find(act);
    return it == kOrder.end() ? -1 : it->second;
}

bool row_is_open(const std::string& tag) {
    std::string t = tag;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::tolower(c); });
    return t.rfind("open", 0) == 0;
}

// Extracts the "deepens X[; Y[; Z]]" quest-id list out of a source_ref cell:
// stops at the first token that is not a bare snake_case identifier (the
// citation prose that follows "deepens" in some rows is not part of the
// list — e.g. "deepens X; Y (act_iii); items:z ..." stops after Y).
std::vector<std::string> parse_deepens(const std::string& source_ref) {
    static const std::regex kActPrefix(R"(^(opening|act_i{1,3}|act_iv)\s+)");
    static const std::regex kIdPattern(R"(^[a-z][a-z0-9_]*$)");
    std::vector<std::string> out;
    const std::string key = "deepens ";
    const auto pos = source_ref.find(key);
    if (pos == std::string::npos) return out;
    std::string rest = source_ref.substr(pos + key.size());
    // Cut at the first em dash (u2014, UTF-8 bytes E2 80 94): the prose
    // resumes there in every authored row.
    const auto dash = rest.find("\xE2\x80\x94");
    if (dash != std::string::npos) rest = rest.substr(0, dash);
    // Drop parenthetical annotations, e.g. "(act_iii)".
    rest = std::regex_replace(rest, std::regex(R"(\([^)]*\))"), "");
    std::size_t start = 0;
    while (start <= rest.size()) {
        auto semi = rest.find(';', start);
        std::string tok = rest.substr(start, semi == std::string::npos ? std::string::npos
                                                                        : semi - start);
        // trim
        const auto a = tok.find_first_not_of(" \t");
        const auto b = tok.find_last_not_of(" \t");
        tok = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
        tok = std::regex_replace(tok, kActPrefix, "");
        if (!std::regex_match(tok, kIdPattern)) break;  // citation prose starts here
        out.push_back(tok);
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    return out;
}

static bool test_data_integrity_sweep_quests_and_dialogues() {
    WorldState w;
    w.init(kCanon, 1);

    const std::set<std::string> kTables = {"items",  "events",  "customs",  "people", "deities",
                                            "factions", "rites", "ranks",   "endings",
                                            "dialogues", "quests", "cities"};
    std::map<std::string, std::set<std::string>> ids_by_table;
    for (const std::string& t : kTables)
        for (const Row& r : w.db.rows(t)) ids_by_table[t].insert(r.get("id"));

    // 1. quests.csv: unique ids, a known act, a well-formed (or absent)
    //    deadline_days, and a giver that reads as a person/role (non-empty).
    std::set<std::string> quest_ids;
    std::map<std::string, std::string> quest_act;
    for (const Row& r : w.db.rows("quests")) {
        const std::string id = r.get("id");
        SIM_CHECK(!id.empty());
        SIM_CHECK(quest_ids.insert(id).second);  // no duplicate ids
        const std::string act = r.get("act");
        SIM_CHECK(act_rank(act) >= 0);
        quest_act[id] = act;
        SIM_CHECK(!r.get("giver").empty());
        const std::string dd = r.get("deadline_days");
        if (!dd.empty()) {
            SIM_CHECK(std::all_of(dd.begin(), dd.end(), [](unsigned char c) { return std::isdigit(c); }));
        }
    }
    SIM_CHECK_EQ(quest_ids.size(), std::size_t{41});

    // 2. dialogues.csv: unique ids, non-empty speaker/text.
    std::set<std::string> dialogue_ids;
    for (const Row& r : w.db.rows("dialogues")) {
        const std::string id = r.get("id");
        SIM_CHECK(!id.empty());
        SIM_CHECK(dialogue_ids.insert(id).second);
        SIM_CHECK(!r.get("speaker").empty());
        SIM_CHECK(!r.get("text").empty());
    }

    // 3. Cross-references embedded in source_ref, "table:id" / "table.csv:id"
    //    — every one must resolve to a real row in that table. Tables that
    //    stand for something other than a canon table id list (dialogues.csv
    //    plain mentions, D-0xx decisions, game-design/wb section numbers) are
    //    excluded by only matching table names this schema actually has.
    static const std::regex kRef(R"(\b([a-z]+)(\.csv)?:([a-z][a-z0-9_]*))");
    int broken_refs = 0;
    for (const char* file : {"quests", "dialogues"}) {
        for (const Row& r : w.db.rows(file)) {
            const std::string& sref = r.get("source_ref");
            for (auto it = std::sregex_iterator(sref.begin(), sref.end(), kRef);
                 it != std::sregex_iterator(); ++it) {
                const std::string table = (*it)[1].str();
                const std::string rid = (*it)[3].str();
                if (kTables.find(table) == kTables.end()) continue;  // not a canon table name
                if (ids_by_table[table].find(rid) == ids_by_table[table].end()) {
                    std::fprintf(stderr, "BROKEN REF: %s row %s -> %s:%s not found\n", file,
                                 r.get("id").c_str(), table.c_str(), rid.c_str());
                    ++broken_refs;
                }
            }
        }
    }
    SIM_CHECK_EQ(broken_refs, 0);

    // 4. "deepens X" chains: every referenced quest id exists, the chain is
    //    acyclic, and a quest never deepens one from a LATER act (a
    //    prerequisite must precede or share the deepened quest's act).
    std::map<std::string, std::vector<std::string>> deepens;
    int order_violations = 0, missing_deepens = 0;
    for (const Row& r : w.db.rows("quests")) {
        const std::string id = r.get("id");
        std::vector<std::string> targets = parse_deepens(r.get("source_ref"));
        for (const std::string& t : targets) {
            if (quest_act.find(t) == quest_act.end()) {
                std::fprintf(stderr, "MISSING DEEPENS TARGET: %s -> %s\n", id.c_str(), t.c_str());
                ++missing_deepens;
                continue;
            }
            if (act_rank(quest_act[t]) > act_rank(quest_act[id])) {
                std::fprintf(stderr, "DEEPENS ORDER VIOLATION: %s (%s) -> %s (%s)\n", id.c_str(),
                             quest_act[id].c_str(), t.c_str(), quest_act[t].c_str());
                ++order_violations;
            }
        }
        deepens[id] = std::move(targets);
    }
    SIM_CHECK_EQ(missing_deepens, 0);
    SIM_CHECK_EQ(order_violations, 0);

    // Acyclic: DFS with a recursion stack over the deepens graph.
    std::map<std::string, int> color;  // 0 white, 1 gray, 2 black
    std::vector<std::string> stack;
    bool cycle_found = false;
    std::function<void(const std::string&)> dfs = [&](const std::string& n) {
        if (cycle_found) return;
        color[n] = 1;
        for (const std::string& m : deepens[n]) {
            if (color[m] == 1) {
                std::fprintf(stderr, "DEEPENS CYCLE at %s -> %s\n", n.c_str(), m.c_str());
                cycle_found = true;
                return;
            }
            if (color[m] == 0) dfs(m);
        }
        color[n] = 2;
    };
    for (const std::string& id : quest_ids)
        if (color[id] == 0) dfs(id);
    SIM_CHECK(!cycle_found);

    return true;
}

}  // namespace

SIM_MAIN(test_offer_the_priestess_debt_is_act_i_canon_in_the_city_of_the_moon,
         test_accept_progress_complete_before_deadline,
         test_fails_on_deadline_when_ignored,
         test_scenario_is_byte_deterministic_under_a_fixed_seed,
         test_data_integrity_sweep_quests_and_dialogues)
