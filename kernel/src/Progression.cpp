// Progression.cpp — implements sim/Progression.hpp (W4-A: the character
// verbs in the world). Like Actions.cpp and Rites.cpp, a WorldState& verb
// file: it writes the character sheet (WorldState::character) and, through
// each module's own free functions only, the purse (Property), market stock
// (Economy), needs (Needs) and inventories. Integer maths only (D-022).
#include "sim/Progression.hpp"

#include "sim/Actions.hpp"
#include "sim/Crafting.hpp"
#include "sim/Schedule.hpp"

#include <algorithm>
#include <optional>

namespace sim {
namespace {

std::string trim(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::vector<std::string> split_semi(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s + ";") {
        if (c == ';') {
            const std::string t = trim(cur);
            if (!t.empty()) out.push_back(t);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    return out;
}

int to_int(const std::string& s, int fallback = 0) {
    const std::string t = trim(s);
    if (t.empty()) return fallback;
    long long v = 0;
    for (const char c : t) {
        if (c < '0' || c > '9') return fallback;
        v = v * 10 + (c - '0');
        if (v > 1000000) return fallback;
    }
    return static_cast<int>(v);
}

bool live(const Row& r) { return r.get("tag") != "OPEN"; }

bool is_player(const Id& actor) { return actor == kPlayerActor; }

std::string role_of(const WorldState& w, const Id& npc) {
    const Npc* n = find_npc(w.population, npc);
    return n == nullptr ? std::string() : lower(trim(n->role));
}

bool exhausted(WorldState& w) {
    const auto it = w.needs.by_actor.find(kPlayerActor);
    if (it == w.needs.by_actor.end()) return false;
    for (const std::string& e : need_effects(it->second))
        if (e == "exhausted") return true;
    return false;
}

bool bad_hours(int hours) { return hours < 1 || hours > kMaxSessionHours; }

// x * (10000 + bp) / 10000, never negative.
std::int64_t scale_bp(std::int64_t x, int bp) {
    const std::int64_t f = std::max<std::int64_t>(0, kBp + static_cast<std::int64_t>(bp));
    return x * f / kBp;
}

int held(const WorldState& w, const Id& actor, const Id& item) {
    const auto inv = w.inventories.find(actor);
    if (inv == w.inventories.end()) return 0;
    const auto it = inv->second.counts.find(item);
    return it == inv->second.counts.end() ? 0 : it->second;
}

std::optional<Row> work_row(const WorldState& w, const std::string& role) {
    if (role.empty()) return std::nullopt;
    for (const Row& r : w.db.rows("work_roles"))
        if (live(r) && lower(trim(r.get("role"))) == role) return r;
    return std::nullopt;
}

std::optional<Row> teaching_row(const Db& db, const Id& skill, const std::string& via,
                                const std::string& source) {
    for (const Row& r : db.rows("skill_teachings")) {
        if (!live(r) || trim(r.get("skill_id")) != skill) continue;
        if (lower(trim(r.get("via"))) != via) continue;
        const std::string src = trim(r.get("source"));
        if ((via == "teacher" ? lower(src) : src) == source) return r;
    }
    return std::nullopt;
}

const NpcSheet* sheet_of(const WorldState& w, const Id& npc) {
    const auto it = w.npc_sheets.find(npc);
    return it == w.npc_sheets.end() ? nullptr : &it->second;
}

int trade_bp_of(const WorldState& w, const Id& actor, const Id& city) {
    if (!is_player(actor)) return 0;
    return w.character.derived.trade_bp +
           rank(w.character, city_faction(w.db, city)) * kRankTradeBpPerTier;
}

// Shared refusals for both directions; "" = the market trades the item today.
std::string market_refusal(const WorldState& w, const Id& city, const Id& item, int qty) {
    if (qty < 1 || qty > 1000) return "bad_quantity";
    const auto book = w.economy.market_by_city.find(city);
    if (book == w.economy.market_by_city.end()) return "unknown_market";
    if (book->second.silver_by_item.find(item) == book->second.silver_by_item.end())
        return "not_sold_here";
    if (!market_open(w.db, w.cal, w.day)) return "market_closed";
    return "";
}

int trade_xp(Silver total) {
    return static_cast<int>(std::min<Silver>(kTradeXpMax, kTradeXpBase + total / kTradeXpPerSilver));
}

}  // namespace

// --- lookups -----------------------------------------------------------------

int actor_attribute(const WorldState& w, const Id& actor, const Id& attr) {
    if (std::find(w.progression.attributes.begin(), w.progression.attributes.end(), attr) ==
        w.progression.attributes.end())
        return -1;
    if (is_player(actor)) return attribute(w.character, attr);
    const NpcSheet* s = sheet_of(w, actor);
    return s == nullptr ? -1 : attribute(*s, attr);
}

int actor_skill(const WorldState& w, const Id& actor, const Id& skill) {
    if (w.progression.skills.count(skill) == 0) return -1;
    if (is_player(actor)) return skill_value(w.character, skill);
    const NpcSheet* s = sheet_of(w, actor);
    return s == nullptr ? -1 : effective_skill(*s, skill);
}

int actor_effective_skill(const WorldState& w, const Id& actor, const Id& skill) {
    if (w.progression.skills.count(skill) == 0) return -1;
    if (is_player(actor)) return effective_skill(w.character, skill);
    const NpcSheet* s = sheet_of(w, actor);
    return s == nullptr ? -1 : effective_skill(*s, skill);
}

// --- use -----------------------------------------------------------------------

SkillGain note_skill_use(WorldState& w, const Id& actor, const Id& skill, int xp) {
    if (!is_player(actor)) return {};
    return gain_skill_xp(w.progression, w.character, skill, xp);
}

SkillGain note_use(WorldState& w, const Id& actor, const std::string& token, int xp) {
    SkillGain total;
    if (!is_player(actor) || token.empty()) return total;
    const std::string want = lower(trim(token));
    for (const auto& [id, def] : w.progression.skills)
        for (const std::string& t : def.grows_by)
            if (lower(t) == want) {
                total.add(gain_skill_xp(w.progression, w.character, id, xp));
                break;
            }
    return total;
}

SkillGain note_craft(WorldState& w, const Id& actor, const Id& recipe, int times) {
    if (!is_player(actor) || times <= 0) return {};
    for (const Recipe& r : load_recipes(w.db)) {
        if (r.id != recipe) continue;
        const long long xp = static_cast<long long>(kCraftXpPerHour) * std::max(1, r.hours) *
                             std::min(times, 1000);
        return note_use(w, actor, "station:" + r.station, static_cast<int>(std::min(xp, 100000LL)));
    }
    return {};
}

SkillGain note_rite(WorldState& w, const Id& rite, bool succeeded) {
    const std::optional<Row> row = w.db.find("rites", rite);
    if (!row || !live(*row)) return {};
    return note_use(w, kPlayerActor, "rite:" + lower(trim(row->get("tradition"))),
                    kRiteXp + (succeeded ? kRiteSuccessXp : 0));
}

// --- practice, teachers, texts, work ---------------------------------------------

ProgressResult practice_skill(WorldState& w, const Id& skill, int hours) {
    ProgressResult r;
    if (w.progression.skills.count(skill) == 0) { r.refusal = "unknown_skill"; return r; }
    if (bad_hours(hours)) { r.refusal = "bad_hours"; return r; }
    if (exhausted(w)) { r.refusal = "exhausted"; return r; }
    if (skill_value(w.character, skill) >= kPracticeCap) { r.refusal = "practice_capped"; return r; }
    r.gain = gain_skill_xp(w.progression, w.character, skill, kPracticeXpPerHour * hours, kPracticeCap);
    advance_needs(w.needs, kPlayerActor, hours, false);
    r.ok = true;
    return r;
}

std::vector<Teaching> skills_taught_by(const WorldState& w, const Id& npc) {
    std::vector<Teaching> out;
    const std::string role = role_of(w, npc);
    const NpcSheet* s = sheet_of(w, npc);
    if (role.empty()) return out;
    for (const Row& r : w.db.rows("skill_teachings")) {
        if (!live(r) || lower(trim(r.get("via"))) != "teacher") continue;
        if (lower(trim(r.get("source"))) != role) continue;
        const Id skill = trim(r.get("skill_id"));
        if (w.progression.skills.count(skill) == 0) continue;
        Teaching t;
        t.skill = skill;
        t.source = npc;
        t.max_level = s == nullptr ? to_int(r.get("max_level")) : effective_skill(*s, skill);
        t.fee_per_hour = teacher_fee(w, skill, npc);
        out.push_back(t);
    }
    std::sort(out.begin(), out.end(), [](const Teaching& a, const Teaching& b) { return a.skill < b.skill; });
    return out;
}

std::vector<Teaching> teachers_of(const WorldState& w, const Id& skill) {
    std::vector<Teaching> out;
    for (const Npc& n : w.population.npcs)
        for (const Teaching& t : skills_taught_by(w, n.id))
            if (t.skill == skill) out.push_back(t);
    std::sort(out.begin(), out.end(), [](const Teaching& a, const Teaching& b) { return a.source < b.source; });
    return out;
}

std::vector<Teaching> skills_taught_in(const Db& db, const Id& item) {
    std::vector<Teaching> out;
    for (const Row& r : db.rows("skill_teachings")) {
        if (!live(r) || lower(trim(r.get("via"))) != "text") continue;
        if (trim(r.get("source")) != item) continue;
        Teaching t;
        t.skill = trim(r.get("skill_id"));
        t.source = item;
        t.max_level = std::min(kSkillMax, to_int(r.get("max_level")));
        out.push_back(t);
    }
    std::sort(out.begin(), out.end(), [](const Teaching& a, const Teaching& b) { return a.skill < b.skill; });
    return out;
}

Silver teacher_fee(const WorldState& w, const Id& skill, const Id& teacher) {
    const std::optional<Row> row = teaching_row(w.db, skill, "teacher", role_of(w, teacher));
    if (!row) return -1;
    return scale_bp(to_int(row->get("fee_per_hour")), w.character.derived.fee_bp);
}

ProgressResult train_with_teacher(WorldState& w, const Id& skill, const Id& teacher, int hours) {
    ProgressResult r;
    if (w.progression.skills.count(skill) == 0) { r.refusal = "unknown_skill"; return r; }
    if (bad_hours(hours)) { r.refusal = "bad_hours"; return r; }
    if (find_npc(w.population, teacher) == nullptr) { r.refusal = "unknown_teacher"; return r; }
    const Silver fee_per_hour = teacher_fee(w, skill, teacher);
    if (fee_per_hour < 0) { r.refusal = "not_taught_by_teacher"; return r; }
    const NpcSheet* s = sheet_of(w, teacher);
    const int teacher_skill = s == nullptr ? 0 : effective_skill(*s, skill);
    if (skill_value(w.character, skill) >= teacher_skill) { r.refusal = "teacher_surpassed"; return r; }
    if (exhausted(w)) { r.refusal = "exhausted"; return r; }
    const Silver fee = fee_per_hour * hours;
    if (purse(w.property, kPlayerActor) < fee) { r.refusal = "cannot_afford"; return r; }

    if (fee > 0) {
        (void)take_from_purse(w.property, kPlayerActor, fee);
        credit_purse(w.property, teacher, fee);
    }
    r.silver = fee;
    r.gain = gain_skill_xp(w.progression, w.character, skill, kTeacherXpPerHour * hours, teacher_skill);
    advance_needs(w.needs, kPlayerActor, hours, false);
    r.ok = true;
    return r;
}

ProgressResult study_text(WorldState& w, const Id& skill, const Id& item, int hours) {
    ProgressResult r;
    if (w.progression.skills.count(skill) == 0) { r.refusal = "unknown_skill"; return r; }
    if (bad_hours(hours)) { r.refusal = "bad_hours"; return r; }
    const std::optional<Row> row = teaching_row(w.db, skill, "text", item);
    if (!row) { r.refusal = "not_taught_in_text"; return r; }
    if (held(w, kPlayerActor, item) <= 0) { r.refusal = "text_not_held"; return r; }
    if (effective_skill(w.character, "scribal_arts") < kLiteracySkill) { r.refusal = "cannot_read"; return r; }
    const int cap = std::min(kSkillMax, to_int(row->get("max_level")));
    if (skill_value(w.character, skill) >= cap) { r.refusal = "text_exhausted"; return r; }
    if (exhausted(w)) { r.refusal = "exhausted"; return r; }

    const int xp = static_cast<int>(scale_bp(kStudyXpPerHour * hours, w.character.derived.study_bp));
    r.gain = gain_skill_xp(w.progression, w.character, skill, xp, cap);
    r.gain.add(note_use(w, kPlayerActor, "verb:read", kReadingXpPerHour * hours));  // the tablet is read, not consumed
    advance_needs(w.needs, kPlayerActor, hours, false);
    r.ok = true;
    return r;
}

ProgressResult work_for(WorldState& w, const Id& employer, int hours) {
    ProgressResult r;
    if (bad_hours(hours)) { r.refusal = "bad_hours"; return r; }
    if (find_npc(w.population, employer) == nullptr) { r.refusal = "unknown_employer"; return r; }
    const std::optional<Row> row = work_row(w, role_of(w, employer));
    if (!row) { r.refusal = "not_an_employer"; return r; }
    const std::vector<std::string> skills = split_semi(row->get("skills"));
    const int min_skill = to_int(row->get("min_skill"));
    if (!skills.empty() && effective_skill(w.character, skills.front()) < min_skill) {
        r.refusal = "skill_too_low";
        return r;
    }
    if (exhausted(w)) { r.refusal = "exhausted"; return r; }

    for (const std::string& sk : skills)
        r.gain.add(gain_skill_xp(w.progression, w.character, sk, kWorkXpPerHour * hours));

    // rpg-systems §6: rations as wages. wage_qty per per_hours hours, floored.
    const std::int64_t qty = to_int(row->get("wage_qty"), 1);
    const std::int64_t per = std::max(1, to_int(row->get("per_hours"), 1));
    const std::int64_t earned =
        static_cast<std::int64_t>(hours) * qty * std::max<std::int64_t>(0, kBp + w.character.derived.wage_bp) /
        (per * kBp);
    const Id wage_item = trim(row->get("wage_item"));
    if (earned > 0) {
        if (wage_item == "silver") {
            credit_purse(w.property, kPlayerActor, earned);
            r.silver = earned;
        } else if (!wage_item.empty()) {
            int& have = w.inventories[kPlayerActor].counts[wage_item];
            have = static_cast<int>(std::min<std::int64_t>(have + earned, 1000000000));
            r.items = static_cast<int>(earned);
        }
    }
    advance_needs(w.needs, kPlayerActor, hours, false);
    r.ok = true;
    return r;
}

// --- trade ------------------------------------------------------------------------

Silver quote_buy(const WorldState& w, const Id& actor, const Id& city, const Id& item, int qty) {
    const auto book = w.economy.market_by_city.find(city);
    if (book == w.economy.market_by_city.end() || qty < 1) return -1;
    const auto p = book->second.silver_by_item.find(item);
    if (p == book->second.silver_by_item.end()) return -1;
    const int barg = std::max(0, actor_effective_skill(w, actor, "bargaining"));
    const int off = std::clamp(barg * kBargainBpPerPoint + trade_bp_of(w, actor, city), 0, 5000);
    const Silver gross = p->second * qty;
    return std::max<Silver>(qty, gross * (kBp - off) / kBp);  // never below 1 a unit
}

Silver quote_sell(const WorldState& w, const Id& actor, const Id& city, const Id& item, int qty) {
    const auto book = w.economy.market_by_city.find(city);
    if (book == w.economy.market_by_city.end() || qty < 1) return -1;
    const auto p = book->second.silver_by_item.find(item);
    if (p == book->second.silver_by_item.end()) return -1;
    const int barg = std::max(0, actor_effective_skill(w, actor, "bargaining"));
    const int bp = std::clamp(kSellBaseBp + barg * kSellBpPerPoint + trade_bp_of(w, actor, city), 0, 9000);
    return p->second * qty * bp / kBp;  // the market never pays more than it charges
}

ProgressResult buy_from_market(WorldState& w, const Id& actor, const Id& city, const Id& item, int qty) {
    ProgressResult r;
    r.refusal = market_refusal(w, city, item, qty);
    if (!r.refusal.empty()) return r;
    const auto stock = w.economy.stock_by_city_item.find(city + "/" + item);
    if (stock == w.economy.stock_by_city_item.end() || stock->second < qty) {
        r.refusal = "out_of_stock";
        return r;
    }
    const Silver total = quote_buy(w, actor, city, item, qty);
    if (purse(w.property, actor) < total) { r.refusal = "cannot_afford"; return r; }

    (void)take_from_purse(w.property, actor, total);
    consume(w.economy, city, item, qty);
    int& have = w.inventories[actor].counts[item];
    have = static_cast<int>(std::min<std::int64_t>(static_cast<std::int64_t>(have) + qty, 1000000000));
    r.silver = total;
    r.items = qty;
    r.gain = note_use(w, actor, "verb:trade", trade_xp(total));
    r.ok = true;
    return r;
}

ProgressResult sell_to_market(WorldState& w, const Id& actor, const Id& city, const Id& item, int qty) {
    ProgressResult r;
    r.refusal = market_refusal(w, city, item, qty);
    if (!r.refusal.empty()) return r;
    if (held(w, actor, item) < qty) { r.refusal = "not_held"; return r; }
    const Silver total = quote_sell(w, actor, city, item, qty);

    w.inventories[actor].counts[item] -= qty;
    deliver(w.economy, city, item, qty);
    credit_purse(w.property, actor, total);
    r.silver = total;
    r.items = qty;
    r.gain = note_use(w, actor, "verb:trade", trade_xp(total));
    r.gain.add(note_use(w, actor, "verb:sell", trade_xp(total)));
    r.ok = true;
    return r;
}

// --- rank ---------------------------------------------------------------------------

ProgressResult raise_rank(WorldState& w, const Id& polity, const Id& patron) {
    ProgressResult r;
    const std::optional<Row> f = w.db.find("factions", polity);
    if (!f || !live(*f)) { r.refusal = "unknown_polity"; return r; }
    if (is_outlawed(w.faction, polity)) { r.refusal = "outlawed"; return r; }
    const int next = rank(w.character, polity) + 1;
    const std::optional<Row> tier_row = w.db.find("ranks", "rank_" + std::to_string(next));
    if (next > 6 || !tier_row || !live(*tier_row)) { r.refusal = "at_highest_rank"; return r; }
    if (standing(w.faction, polity) < kRankStanding[next]) { r.refusal = "standing_too_low"; return r; }
    if (next < 6) {
        const Npc* p = find_npc(w.population, patron);
        if (p == nullptr) { r.refusal = "unknown_patron"; return r; }
        if (p->faction_id != polity) { r.refusal = "patron_not_of_polity"; return r; }
    }
    w.character.rank_by_polity[polity] = next;
    r.ok = true;
    return r;
}

void strip_rank(WorldState& w, const Id& actor, const Id& polity) {
    if (is_player(actor)) w.character.rank_by_polity.erase(polity);  // absent = 0, the Outsider
}

// --- init ---------------------------------------------------------------------------

void seed_npc_sheets(WorldState& w) {
    w.npc_sheets.clear();
    for (const Npc& n : w.population.npcs) {
        NpcSheet s;
        const std::string role = lower(trim(n.role));
        if (!role.empty()) {
            if (const std::optional<Row> work = work_row(w, role))
                for (const std::string& sk : split_semi(work->get("skills")))
                    if (w.progression.skills.count(sk) != 0) s.skills[sk] = std::max(s.skills[sk], kWorkerSkill);
            for (const Row& r : w.db.rows("skill_teachings")) {
                if (!live(r) || lower(trim(r.get("via"))) != "teacher") continue;
                if (lower(trim(r.get("source"))) != role) continue;
                const Id sk = trim(r.get("skill_id"));
                if (w.progression.skills.count(sk) == 0) continue;
                s.skills[sk] = std::max(s.skills[sk], std::min(kSkillMax, to_int(r.get("max_level"))));
            }
        }
        w.npc_sheets[n.id] = std::move(s);
    }
}

}  // namespace sim
