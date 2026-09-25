// Character.cpp — implements sim/Character.hpp (W4-A; contract:
// kernel/contracts/module_Character.md). Pure: reads canon through Db at
// load time only, then works on the sheets it is handed. Integer maths only
// (D-022).
#include "sim/Character.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace sim {
namespace {

std::string trim(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s) {
        if (c == sep) {
            const std::string t = trim(cur);
            if (!t.empty()) out.push_back(t);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    const std::string t = trim(cur);
    if (!t.empty()) out.push_back(t);
    return out;
}

// A whole-string signed integer ("+5", "-2500", "10"); false on anything else.
bool parse_int(const std::string& raw, int& out) {
    const std::string s = trim(raw);
    if (s.empty()) return false;
    std::size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    if (i >= s.size()) return false;
    long long v = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
        if (v > 1000000) return false;  // no effect is that large; also an overflow guard
    }
    out = static_cast<int>(s[0] == '-' ? -v : v);
    return true;
}

int int_field(const Row& r, const std::string& col, int fallback) {
    int v = 0;
    return parse_int(r.get(col), v) ? v : fallback;
}

bool live(const Row& r) { return r.get("tag") != "OPEN"; }

bool targeted(const std::string& kind) {
    return kind == "skill_bonus" || kind == "growth_bp" || kind == "attribute";
}

bool scalar(const std::string& kind) {
    return kind == "trade_bp" || kind == "wage_bp" || kind == "fee_bp" || kind == "study_bp" ||
           kind == "rite_favour_bp";
}

void apply_effect(const Effect& e, CharacterDerived& d) {
    if (e.kind == "skill_bonus") d.skill_bonus[e.target] += e.amount;
    else if (e.kind == "growth_bp") d.growth_bp[e.target] += e.amount;
    else if (e.kind == "attribute") d.attr_bonus[e.target] += e.amount;
    else if (e.kind == "trade_bp") d.trade_bp += e.amount;
    else if (e.kind == "wage_bp") d.wage_bp += e.amount;
    else if (e.kind == "fee_bp") d.fee_bp += e.amount;
    else if (e.kind == "study_bp") d.study_bp += e.amount;
    else if (e.kind == "rite_favour_bp") d.rite_favour_bp += e.amount;
}

const std::string& name_of(const ProgressionCatalog& cat, const Id& id) {
    static const std::string none = "-";
    if (id.empty()) return none;
    const auto it = cat.callings.find(id);
    return it == cat.callings.end() ? id : it->second.name;
}

}  // namespace

int skill_point_cost(int value) { return 5 + std::max(0, value) / 2; }
int level_cost(int level) { return 5 + std::max(1, level); }
int xp_for_level(int level) {
    const int l = std::clamp(level, 1, kLevelCap) - 1;
    return 5 * l + l * (l + 1) / 2;
}

bool parse_effects(const std::string& text, std::vector<Effect>& out) {
    std::vector<Effect> parsed;
    for (const std::string& entry : split(text, ';')) {
        const std::vector<std::string> parts = split(entry, ':');
        Effect e;
        if (parts.size() == 3 && targeted(parts[0])) {
            e.kind = parts[0];
            e.target = parts[1];
            if (!parse_int(parts[2], e.amount)) return false;
        } else if (parts.size() == 2 && scalar(parts[0])) {
            e.kind = parts[0];
            if (!parse_int(parts[1], e.amount)) return false;
        } else {
            return false;
        }
        parsed.push_back(e);
    }
    out.insert(out.end(), parsed.begin(), parsed.end());
    return true;
}

ProgressionCatalog load_progression(const Db& db) {
    ProgressionCatalog cat;
    for (const Row& r : db.rows("attributes"))
        if (live(r)) cat.attributes.push_back(r.at("id"));

    for (const Row& r : db.rows("skills")) {
        if (!live(r)) continue;
        SkillDef s;
        s.id = r.at("id");
        s.name = r.get("name");
        s.group = trim(r.get("group"));
        s.attribute = trim(r.get("attribute"));
        s.grows_by = split(r.get("grows_by"), ';');
        cat.skills[s.id] = s;
    }

    for (const Row& r : db.rows("callings")) {
        if (!live(r)) continue;
        CallingDef c;
        c.id = r.at("id");
        c.name = r.get("name");
        c.specialisation = trim(r.get("kind")) == "specialisation";
        c.parent = trim(r.get("parent"));
        c.skills = split(r.get("skills"), ';');
        if (!parse_effects(r.get("perks"), c.perks)) continue;
        cat.callings[c.id] = c;
    }

    for (const Row& r : db.rows("talents")) {
        if (!live(r)) continue;
        TalentDef t;
        t.id = r.at("id");
        t.name = r.get("name");
        t.calling = trim(r.get("calling"));
        t.skill = trim(r.get("skill"));
        t.min_level = int_field(r, "min_level", 1);
        t.min_skill = int_field(r, "min_skill", 0);
        if (!parse_effects(r.get("effect"), t.effects)) continue;
        cat.talents[t.id] = t;
    }
    return cat;
}

void SkillGain::add(const SkillGain& o) {
    points += o.points;
    levels += o.levels;
    attributes_raised.insert(attributes_raised.end(), o.attributes_raised.begin(),
                             o.attributes_raised.end());
}

CharacterState new_character(const ProgressionCatalog& cat) {
    CharacterState c;
    for (const Id& a : cat.attributes) c.attributes[a] = kAttrStart;
    refresh_derived(cat, c);
    return c;
}

void refresh_derived(const ProgressionCatalog& cat, CharacterState& c) {
    CharacterDerived d;
    for (const Id* held : {&c.calling, &c.specialisation, &c.second_calling}) {
        const auto it = cat.callings.find(*held);
        if (it == cat.callings.end()) continue;
        d.favoured.insert(it->second.skills.begin(), it->second.skills.end());
        for (const Effect& e : it->second.perks) apply_effect(e, d);
    }
    for (const Id& t : c.talents) {
        const auto it = cat.talents.find(t);
        if (it == cat.talents.end()) continue;  // a vetoed talent row keeps its slot, loses its effect
        for (const Effect& e : it->second.effects) apply_effect(e, d);
    }
    c.derived = std::move(d);
}

int attribute(const CharacterState& c, const Id& attr) {
    const auto it = c.attributes.find(attr);
    if (it == c.attributes.end()) return 0;
    const auto b = c.derived.attr_bonus.find(attr);
    return std::clamp(it->second + (b == c.derived.attr_bonus.end() ? 0 : b->second), kAttrMin,
                      kAttrMax);
}

int attribute(const NpcSheet& s, const Id& attr) {
    const auto it = s.attributes.find(attr);
    return std::clamp(it == s.attributes.end() ? kAttrStart : it->second, kAttrMin, kAttrMax);
}

int skill_value(const CharacterState& c, const Id& skill) {
    const auto it = c.skills.find(skill);
    return it == c.skills.end() ? 0 : it->second.value;
}

int effective_skill(const CharacterState& c, const Id& skill) {
    const auto b = c.derived.skill_bonus.find(skill);
    const int bonus = b == c.derived.skill_bonus.end() ? 0 : b->second;
    return std::clamp(skill_value(c, skill) + bonus, 0, kSkillMax);
}

int effective_skill(const NpcSheet& s, const Id& skill) {
    const auto it = s.skills.find(skill);
    return std::clamp(it == s.skills.end() ? 0 : it->second, 0, kSkillMax);
}

int npc_level(const NpcSheet& s) {
    int sum = 0;
    for (const auto& [id, v] : s.skills) sum += std::clamp(v, 0, kSkillMax);
    return std::min(kLevelCap, 1 + sum / 25);
}

bool holds_calling(const CharacterState& c, const Id& id) {
    return !id.empty() && (c.calling == id || c.specialisation == id || c.second_calling == id);
}

int rank(const CharacterState& c, const Id& polity) {
    const auto it = c.rank_by_polity.find(polity);
    return it == c.rank_by_polity.end() ? 0 : it->second;
}

int growth_bp(const ProgressionCatalog& cat, const CharacterState& c, const Id& skill) {
    int bp = kBp;
    const auto def = cat.skills.find(skill);
    if (def != cat.skills.end() && !def->second.attribute.empty()) {
        const int a = attribute(c, def->second.attribute);
        if (a > 0) bp += (a - kAttrStart) * kAttrGrowthBpPerPoint;
    }
    if (c.derived.favoured.count(skill) != 0) bp += kCallingGrowthBp;
    const auto g = c.derived.growth_bp.find(skill);
    if (g != c.derived.growth_bp.end()) bp += g->second;
    return std::max(bp, kMinGrowthBp);
}

int apply_levels(CharacterState& c) {
    int gained = 0;
    while (c.level < kLevelCap && c.xp >= xp_for_level(c.level + 1)) {
        ++c.level;
        ++c.talent_points;  // row 22: one talent per level
        ++gained;
    }
    return gained;
}

SkillGain gain_skill_xp(const ProgressionCatalog& cat, CharacterState& c, const Id& skill, int xp,
                        int cap) {
    SkillGain g;
    const auto def = cat.skills.find(skill);
    if (def == cat.skills.end() || xp <= 0) return g;
    cap = std::clamp(cap, 0, kSkillMax);
    if (skill_value(c, skill) >= cap) return g;

    const std::int64_t add = static_cast<std::int64_t>(std::min(xp, 1000000)) * growth_bp(cat, c, skill);
    SkillProgress& sp = c.skills[skill];
    sp.progress += add;
    const Id& attr = def->second.attribute;
    while (sp.value < cap) {
        const std::int64_t cost = static_cast<std::int64_t>(skill_point_cost(sp.value)) * kBp;
        if (sp.progress < cost) break;
        sp.progress -= cost;
        ++sp.value;
        ++g.points;
        ++c.xp;  // row 22: character XP comes from skill points
        // Attribute growth by exercise (INVENTED rule; ledgered).
        auto a = c.attributes.find(attr);
        if (a != c.attributes.end() && a->second < kAttrMax) {
            int& ex = c.attr_exercise[attr];
            ++ex;
            const int step = kAttrExercisePerStep * (a->second + 1);
            if (ex >= step) {
                ex -= step;
                ++a->second;
                g.attributes_raised.push_back(attr);
            }
        }
    }
    if (sp.value >= cap) sp.progress = 0;  // a teacher/text can take you no further
    g.levels = apply_levels(c);
    return g;
}

std::string talent_refusal(const ProgressionCatalog& cat, const CharacterState& c, const Id& talent) {
    const auto it = cat.talents.find(talent);
    if (it == cat.talents.end()) return "unknown_talent";
    const TalentDef& t = it->second;
    if (std::find(c.talents.begin(), c.talents.end(), talent) != c.talents.end()) return "already_taken";
    if (c.level < t.min_level) return "level_too_low";
    if (!t.calling.empty() && !holds_calling(c, t.calling)) return "calling_required";
    if (!t.skill.empty() && effective_skill(c, t.skill) < t.min_skill) return "skill_too_low";
    if (c.talent_points <= 0) return "no_talent_point";
    return "";
}

std::string choose_talent(const ProgressionCatalog& cat, CharacterState& c, const Id& talent) {
    const std::string why = talent_refusal(cat, c, talent);
    if (!why.empty()) return why;
    c.talents.push_back(talent);
    --c.talent_points;
    refresh_derived(cat, c);
    return "";
}

std::vector<Id> available_talents(const ProgressionCatalog& cat, const CharacterState& c) {
    std::vector<Id> out;
    for (const auto& [id, t] : cat.talents)
        if (talent_refusal(cat, c, id).empty()) out.push_back(id);
    return out;
}

std::string choose_calling(const ProgressionCatalog& cat, CharacterState& c, const Id& calling) {
    const auto it = cat.callings.find(calling);
    if (it == cat.callings.end()) return "unknown_calling";
    if (it->second.specialisation) return "not_a_calling";
    if (c.level < kFirstCallingLevel) return "level_too_low";
    if (!c.calling.empty()) return "already_chosen";
    c.calling = calling;
    refresh_derived(cat, c);
    return "";
}

std::string choose_specialisation(const ProgressionCatalog& cat, CharacterState& c, const Id& spec) {
    const auto it = cat.callings.find(spec);
    if (it == cat.callings.end()) return "unknown_calling";
    if (!it->second.specialisation) return "not_a_specialisation";
    if (c.level < kSpecialisationLevel) return "level_too_low";
    if (c.calling.empty()) return "no_calling";
    if (it->second.parent != c.calling && it->second.parent != c.second_calling)
        return "not_of_your_calling";
    if (!c.specialisation.empty()) return "already_chosen";
    c.specialisation = spec;
    refresh_derived(cat, c);
    return "";
}

std::string choose_second_calling(const ProgressionCatalog& cat, CharacterState& c, const Id& calling) {
    const auto it = cat.callings.find(calling);
    if (it == cat.callings.end()) return "unknown_calling";
    if (it->second.specialisation) return "not_a_calling";
    if (c.level < kSecondCallingLevel) return "level_too_low";
    if (c.calling.empty()) return "no_calling";
    if (c.calling == calling) return "same_calling";
    if (!c.second_calling.empty()) return "already_chosen";
    c.second_calling = calling;
    refresh_derived(cat, c);
    return "";
}

std::string character_summary(const ProgressionCatalog& cat, const CharacterState& c) {
    std::ostringstream os;
    os << "level " << c.level << " / " << kLevelCap << " | xp " << c.xp;
    if (c.level < kLevelCap) os << " (next level at " << xp_for_level(c.level + 1) << ")";
    os << " | talent points " << c.talent_points << "\n";
    os << "calling: " << name_of(cat, c.calling) << " | specialisation: " << name_of(cat, c.specialisation)
       << " | second calling: " << name_of(cat, c.second_calling) << "\n";
    os << "attributes:";
    bool first = true;
    for (const Id& a : cat.attributes) {
        os << (first ? " " : ", ") << a << " " << attribute(c, a);
        first = false;
    }
    os << "\nskills:";
    first = true;
    for (const auto& [id, sp] : c.skills) {
        if (sp.value <= 0 && effective_skill(c, id) <= 0) continue;
        os << (first ? " " : ", ") << id << " " << sp.value;
        const int eff = effective_skill(c, id);
        if (eff != sp.value) os << " (" << eff << ")";
        if (c.derived.favoured.count(id) != 0) os << "*";
        first = false;
    }
    if (first) os << " none";
    os << "\ntalents:";
    first = true;
    for (const Id& t : c.talents) {
        const auto it = cat.talents.find(t);
        os << (first ? " " : ", ") << (it == cat.talents.end() ? t : it->second.name);
        first = false;
    }
    if (first) os << " none";
    os << "\nrank:";
    first = true;
    for (const auto& [polity, tier] : c.rank_by_polity) {
        if (tier <= 0) continue;
        os << (first ? " " : ", ") << polity << " " << tier;
        first = false;
    }
    if (first) os << " the Outsider everywhere";
    os << "\n";
    return os.str();
}

}  // namespace sim
