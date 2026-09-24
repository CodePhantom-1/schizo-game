// Events.cpp — the city's pulse (city-life §7): a trigger engine over the
// WorldFacts and the calendar, with rules from db/canon/events.csv. At Wave 1
// the table is EMPTY: the engine exists, loads zero rules, and fires nothing.
//
// Determinism: the only randomness is ctx.rng.fork(day + rule salt); no wall
// clock, no global or static mutable state, no threads. Writes ONLY EventsState.
#include "sim/Events.hpp"

#include "sim/Context.hpp"

#include <cstdint>
#include <string>

namespace sim {
namespace {

// FNV-1a 64 over the rule id — this is the "rule salt" of Events.hpp. Deriving
// it from the id keeps every rule's chance stream independent of every other
// rule's (different ids -> different forks on the same day) and stable when a
// later canon edit merely reorders rows.
std::uint64_t rule_salt(const std::string& id) {
    std::uint64_t h = 14695981039346656037ull;  // FNV offset basis
    for (const char c : id) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;  // FNV prime
    }
    return h;
}

std::string trim(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

// "<kind>:<arg>" — arg is empty when the condition carries none.
void split_condition(const std::string& condition, std::string& kind, std::string& arg) {
    const std::size_t colon = condition.find(':');
    if (colon == std::string::npos) {
        kind = trim(condition);
        arg.clear();
    } else {
        kind = trim(condition.substr(0, colon));
        arg = trim(condition.substr(colon + 1));
    }
}

// Strict decimal read: a malformed arg is a condition that never matches,
// never a guess. Bounds the value so later arithmetic cannot overflow.
bool parse_int(const std::string& s, long long& out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    bool negative = false;
    if (s[0] == '-' || s[0] == '+') {
        negative = s[0] == '-';
        i = 1;
    }
    if (i >= s.size()) return false;
    long long v = 0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
        if (v > 4611686018427387903LL) return false;
    }
    out = negative ? -v : v;
    return true;
}

// The rule's row says it may fire again via its `repeat` column; the column is
// absent from the Wave 1 header, so every loaded rule defaults to fire-once.
bool is_truthy(const std::string& s) {
    return s == "1" || s == "true" || s == "yes" || s == "y";
}

// One condition of one rule, evaluated for `day`. Unknown kinds and malformed
// args never match: the engine invents no behaviour for shapes it was not given.
bool condition_matches(const EventRule& rule, const std::string& condition,
                       const WorldContext& ctx, DayNumber day) {
    std::string kind;
    std::string arg;
    split_condition(condition, kind, arg);

    if (kind == "drought_gte") {
        long long stage = 0;
        return parse_int(arg, stage) && ctx.facts.drought_stage >= stage;
    }
    if (kind == "war_gte") {
        long long stage = 0;
        return parse_int(arg, stage) && ctx.facts.war_stage >= stage;
    }
    if (kind == "season") {
        return !arg.empty() && ctx.cal.season_id(day) == arg;
    }
    if (kind == "festival") {
        // The Calendar exposes no per-festival id at Wave 1, so the condition
        // can only mean "the day is one of the calendar's festival days".
        return ctx.cal.is_festival(day);
    }
    if (kind == "chance") {
        long long n = 0;
        if (!parse_int(arg, n) || n <= 0) return false;
        if (n > 2147483647LL) return false;  // 1-in-more-than-int: no daily roll resolves it
        // Deterministic 1-in-N: the fork mixes the day and the rule salt and
        // does NOT advance the parent stream, so the roll depends only on
        // (world rng state, day, rule id).
        Rng roll = ctx.rng.fork(static_cast<std::uint64_t>(day) + rule_salt(rule.id));
        return roll.int_in(1, static_cast<int>(n)) == 1;
    }
    return false;
}

// A rule fires when ALL its conditions hold for the day (the documented
// "drought_gte:2;chance:10" example is exactly such an AND). A rule carrying
// no conditions at all is a vacuous AND: it fires once (then the fire-once
// invariant stops it) — no special case is invented for the malformed shape.
bool rule_matches(const EventRule& rule, const WorldContext& ctx, DayNumber day) {
    for (const std::string& condition : rule.triggers)
        if (!condition_matches(rule, condition, ctx, day)) return false;
    return true;
}

}  // namespace

void load_rules(const WorldContext& ctx, EventsState& state) {
    state.rules.clear();  // reload is idempotent: the same db yields the same rules
    for (const Row& row : ctx.db.rows("events")) {
        // An OPEN row is an unwritten event — its content does not exist yet —
        // so it must never become a rule the world can fire.
        if (row.get("tag") == "OPEN") continue;

        EventRule rule;
        rule.id = row.at("id");
        rule.category = row.get("category");

        // `triggers` is a ';'-separated condition list, e.g. "drought_gte:2;chance:10".
        const std::string cell = row.get("triggers");
        std::size_t pos = 0;
        while (pos <= cell.size()) {
            const std::size_t semi = cell.find(';', pos);
            const std::string part =
                trim(cell.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos));
            if (!part.empty()) rule.triggers.push_back(part);
            if (semi == std::string::npos) break;
            pos = semi + 1;
        }

        rule.repeatable = is_truthy(row.get("repeat"));
        state.rules.push_back(std::move(rule));
    }
}

void tick_events(const WorldContext& ctx, EventsState& state, int days) {
    // The context is the morning of ctx.day; evaluate that day and, when asked,
    // the days after it. Days ascend, so `fired` stays ordered by day; within
    // one day, rules fire in load order.
    for (DayNumber day = ctx.day; day < ctx.day + days; ++day) {
        for (const EventRule& rule : state.rules) {
            if (!rule.repeatable) {
                // Fires at most once per rule unless the rule is repeatable.
                const auto it = state.fire_count_by_rule.find(rule.id);
                if (it != state.fire_count_by_rule.end() && it->second > 0) continue;
            }
            if (!rule_matches(rule, ctx, day)) continue;

            ++state.fire_count_by_rule[rule.id];
            // The frozen EventRule carries only id/category/triggers, so the
            // summary can state nothing beyond the rule's own id.
            state.fired.push_back(TriggeredEvent{rule.id, day, rule.id});
        }
    }
}

}  // namespace sim
