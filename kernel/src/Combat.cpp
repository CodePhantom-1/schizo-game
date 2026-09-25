// Combat.cpp — W4-B: implements sim/Combat.hpp. See the header for the
// contract and kernel/contracts/module_Combat.md for every formula.
//
// D-022: integer maths throughout. Chances are basis points (0..10000)
// against Rng::int_in(0, 9999); multipliers are integer percentages.
// Every tuning number below is INVENTED machinery (D-018, D-021), listed in
// docs/proposals/invented-ledger-combat.md.
#include "sim/Combat.hpp"

#include "sim/Db.hpp"
#include "sim/Needs.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <set>

namespace sim {
namespace {

// --- tuning (INVENTED) -------------------------------------------------------
constexpr int kMaxHealth = 100;
constexpr int kChanceBase = 5000;       // an even match hits half the time
constexpr int kChancePerPoint = 60;     // +0.6% per point of score difference
constexpr int kChanceMin = 500;         // nothing is certain...
constexpr int kChanceMax = 9500;        // ...either way
constexpr int kGapMargin = 4000;        // a blow this far inside the chance finds a gap
constexpr int kAimHead = 3000;          // margin needed to land an aimed blow on the head
constexpr int kAimLimb = 1500;          // on the arms or legs (the torso needs none)
constexpr int kLimpAt = 20;             // open leg damage that lames
constexpr int kUselessArmAt = 30;       // open arm damage that disables the arm
constexpr int kConcussedAt = 15;        // open head damage that dazes
constexpr int kWindedAt = 15;           // stamina below this is "winded"
constexpr int kCollapseAt = 15;         // health at/below which a wounded man collapses
constexpr int kKnockoutBluntHead = 9;   // net blunt head damage that knocks out
constexpr std::uint64_t kCombatSalt = 0xC0B7A7C0B7A7ull;

std::uint64_t fnv1a64(const std::string& s) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return h;
}

std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::vector<std::string> split_semi(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        const std::size_t semi = s.find(';', start);
        const std::size_t end = semi == std::string::npos ? s.size() : semi;
        const std::string part = trim(s.substr(start, end - start));
        if (!part.empty()) out.push_back(part);
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    return out;
}

int to_int(const std::string& s, int fallback = 0) {
    const std::string t = trim(s);
    if (t.empty()) return fallback;
    char* end = nullptr;
    const long v = std::strtol(t.c_str(), &end, 10);
    if (end == t.c_str()) return fallback;
    if (v > INT_MAX / 4) return INT_MAX / 4;
    if (v < INT_MIN / 4) return INT_MIN / 4;
    return static_cast<int>(v);
}

bool row_open(const Row& r) { return lower(trim(r.get("tag"))).rfind("open", 0) == 0; }

int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

bool has_lasting(const Combatant& c, const std::string& mark) {
    return std::find(c.lasting.begin(), c.lasting.end(), mark) != c.lasting.end();
}

void add_lasting(Combatant& c, const std::string& mark) {
    if (!has_lasting(c, mark)) {
        c.lasting.push_back(mark);
        std::sort(c.lasting.begin(), c.lasting.end());
    }
}

bool limping(const Combatant& c) {
    return open_damage(c, kZoneLegs) >= kLimpAt || has_lasting(c, "limp");
}

ArmsDef weapon_of(const Db& db, const Combatant& c) {
    if (c.weapon == "ordinary_spear") return ordinary_arms();
    if (!c.weapon.empty())
        if (auto d = arms_def(db, c.weapon)) return *d;
    return fists();
}

// Wears `amount` durability off an item; true when this wear broke it.
bool wear(const Db& db, Combatant& c, const ArmsDef& def, int amount) {
    if (def.id == "fists" || def.durability <= 0 || amount <= 0) return false;
    const int cur = current_durability(db, c, def.id);
    if (cur <= 0) return false;
    const int next = std::max(0, cur - amount);
    c.durability[def.id] = next;
    return next == 0;
}

bool broken(const Db& db, const Combatant& c, const ArmsDef& def) {
    return def.id != "fists" && def.durability > 0 && current_durability(db, c, def.id) <= 0;
}

int quality_pct(int quality) { return 75 + clampi(quality, 0, 100) / 2; }  // 75..125

int severity_of(int net) {
    if (net <= 3) return kScratch;
    if (net <= 8) return kLight;
    if (net <= 15) return kSerious;
    if (net <= 24) return kGrave;
    return kMortal;
}

int bleed_of(const std::string& type, int severity) {
    if (type == "blunt") return 0;
    static const int kBleed[] = {0, 0, 1, 2, 4, 7};
    int b = kBleed[clampi(severity, 0, 5)];
    if (type == "pierce" && severity >= kSerious) ++b;  // deep punctures bleed more
    return b;
}

int zone_multiplier_pct(int zone) {
    switch (zone) {
        case kZoneHead: return 150;
        case kZoneTorso: return 100;
        default: return 75;
    }
}

void record_hostility(CombatState& s, const Id& aggressor, const Id& other, DayNumber day) {
    for (Hostility& h : s.hostilities) {
        const bool same_pair = (h.aggressor == aggressor && h.other == other) ||
                               (h.aggressor == other && h.other == aggressor);
        if (!same_pair) continue;
        if (h.day != day) {  // yesterday's quarrel is over: today's first blow decides
            h.aggressor = aggressor;
            h.other = other;
            h.day = day;
        }
        return;
    }
    s.hostilities.push_back(Hostility{aggressor, other, day});
}

const char* outcome_name(AttackOutcome o) {
    switch (o) {
        case AttackOutcome::Dodged: return "dodged";
        case AttackOutcome::Blocked: return "blocked";
        case AttackOutcome::Parried: return "parried";
        case AttackOutcome::Deflected: return "deflected";
        case AttackOutcome::Wounded: return "wounded";
        case AttackOutcome::KnockedOut: return "knocked_out";
        case AttackOutcome::Killed: return "killed";
        default: return "invalid";
    }
}

void describe(AttackResult& r) {
    auto b = [](bool v) { return v ? std::string("1") : std::string("0"); };
    r.text = "outcome=" + std::string(outcome_name(r.outcome));
    if (r.outcome == AttackOutcome::Invalid) {
        r.text += ";refusal=" + r.refusal;
        return;
    }
    r.text += ";attacker=" + r.attacker + ";defender=" + r.defender + ";weapon=" + r.weapon;
    r.text += ";chance_bp=" + std::to_string(r.chance_bp) + ";roll_bp=" + std::to_string(r.roll_bp);
    if (r.zone >= 0) {
        r.text += ";zone=" + std::string(zone_name(r.zone)) + ";aimed=" + b(r.aimed);
        r.text += ";type=" + r.damage_type + ";damage=" + std::to_string(r.damage);
        if (r.severity > 0)
            r.text += ";severity=" + std::string(severity_name(r.severity)) +
                      ";bleed=" + std::to_string(r.bleed);
        r.text += ";gap=" + b(r.found_gap);
    }
    r.text += ";shield_broke=" + b(r.shield_broke) + ";weapon_broke=" + b(r.weapon_broke) +
              ";armour_broke=" + b(r.armour_broke) + ";winded=" + b(r.winded);
}

AttackResult refuse(AttackResult r, const std::string& why) {
    r.outcome = AttackOutcome::Invalid;
    r.refusal = why;
    describe(r);
    return r;
}

int needs_penalty(const Needs* n) {
    if (n == nullptr) return 0;
    int p = 0;
    if (n->fatigue >= 70) p += 15;  // Needs.cpp "exhausted"
    if (n->hunger >= 75) p += 10;   // "starving"
    if (n->thirst >= 75) p += 10;
    return p;
}

}  // namespace

// --- zones / names -------------------------------------------------------------
const char* zone_name(int zone) {
    switch (zone) {
        case kZoneHead: return "head";
        case kZoneTorso: return "torso";
        case kZoneArms: return "arms";
        case kZoneLegs: return "legs";
        default: return "";
    }
}

int zone_from_name(const std::string& name) {
    const std::string n = lower(trim(name));
    for (int z = 0; z < kZoneCount; ++z)
        if (n == zone_name(z)) return z;
    return -1;
}

const char* severity_name(int severity) {
    switch (severity) {
        case kScratch: return "scratch";
        case kLight: return "light";
        case kSerious: return "serious";
        case kGrave: return "grave";
        case kMortal: return "mortal";
        default: return "";
    }
}

const char* stance_name(Stance st) {
    switch (st) {
        case Stance::Fight: return "fight";
        case Stance::Flee: return "flee";
        case Stance::Surrender: return "surrender";
        default: return "none";
    }
}

int CombatInputs::skill(const std::string& token) const {
    const auto it = skills.find(token);
    return clampi(it == skills.end() ? default_skill : it->second, 0, 100);
}

// --- arms ----------------------------------------------------------------------
bool ArmsDef::two_handed() const { return skill == "bows" || skill == "slings"; }

int ArmsDef::protection(const std::string& type) const {
    if (type == "cut") return prot_cut;
    if (type == "pierce") return prot_pierce;
    if (type == "blunt") return prot_blunt;
    return 0;
}

std::optional<ArmsDef> arms_def(const Db& db, const Id& item) {
    if (item.empty()) return std::nullopt;
    const std::optional<Row> row = db.find("arms", item);
    if (!row || row_open(*row)) return std::nullopt;
    ArmsDef d;
    d.id = item;
    d.slot = lower(trim(row->get("slot")));
    d.skill = lower(trim(row->get("skill")));
    d.damage_type = lower(trim(row->get("damage_type")));
    d.damage = to_int(row->get("damage"));
    d.reach_cm = to_int(row->get("reach_cm"));
    d.speed = to_int(row->get("speed"), 5);
    d.weight_g = std::max(0, to_int(row->get("weight_g")));
    d.balance = to_int(row->get("balance"), 5);
    d.durability = std::max(0, to_int(row->get("durability")));
    d.quality = clampi(to_int(row->get("quality"), 50), 0, 100);
    for (const std::string& z : split_semi(row->get("zones"))) {
        const int zi = zone_from_name(z);
        if (zi >= 0) d.zones.push_back(zi);
    }
    d.layer = lower(trim(row->get("layer")));
    d.prot_cut = to_int(row->get("prot_cut"));
    d.prot_pierce = to_int(row->get("prot_pierce"));
    d.prot_blunt = to_int(row->get("prot_blunt"));
    d.block = to_int(row->get("block"));
    d.stamina = to_int(row->get("stamina"));
    d.heat = to_int(row->get("heat"));
    d.material_tier = lower(trim(row->get("material_tier")));
    d.ammo = trim(row->get("ammo"));
    d.culture = row->get("culture");
    return d;
}

ArmsDef fists() {
    ArmsDef d;
    d.id = "fists";
    d.slot = "melee";
    d.skill = "unarmed";
    d.damage_type = "blunt";
    d.damage = 3;
    d.reach_cm = 50;
    d.speed = 8;
    d.balance = 6;
    d.quality = 50;
    return d;
}

ArmsDef ordinary_arms() {
    ArmsDef d;
    d.id = "ordinary_spear";
    d.slot = "melee";
    d.skill = "spears";
    d.damage_type = "pierce";
    d.damage = 13;
    d.reach_cm = 200;
    d.speed = 6;
    d.weight_g = 1800;
    d.balance = 6;
    d.stamina = 3;
    d.quality = 50;
    return d;  // durability 0: never wears
}

// --- the body ------------------------------------------------------------------
Combatant& combatant_of(CombatState& s, const Id& actor) {
    auto it = s.by_actor.find(actor);
    if (it != s.by_actor.end()) return it->second;
    Combatant c;
    c.id = actor;
    return s.by_actor.emplace(actor, std::move(c)).first->second;
}

const Combatant* find_combatant(const CombatState& s, const Id& actor) {
    const auto it = s.by_actor.find(actor);
    return it == s.by_actor.end() ? nullptr : &it->second;
}

int open_damage(const Combatant& c, int zone) {
    int sum = 0;
    for (const Wound& w : c.wounds)
        if (zone < 0 || w.zone == zone) sum += w.remaining;
    return sum;
}

int health_cap(const Combatant& c) { return std::max(1, kMaxHealth - open_damage(c, -1) / 2); }

int bleeding(const Combatant& c) {
    if (c.dead) return 0;
    int b = 0;
    for (const Wound& w : c.wounds)
        if (w.remaining > 0) b += w.bleed;
    return b;
}

bool conscious(const Combatant& c) { return !c.dead && c.ko_hours <= 0; }

bool can_fight(const Combatant& c) {
    return conscious(c) && c.surrendered_to.empty() && c.captor.empty();
}

int armour_weight_g(const Db& db, const Combatant& c) {
    int g = 0;
    for (const Id& a : c.armour)
        if (auto d = arms_def(db, a)) g += d->weight_g;
    if (auto d = arms_def(db, c.shield)) g += d->weight_g;
    return g;
}

int armour_heat(const Db& db, const Combatant& c) {
    int h = 0;
    for (const Id& a : c.armour)
        if (auto d = arms_def(db, a)) h += d->heat;
    if (auto d = arms_def(db, c.shield)) h += d->heat;
    return h;
}

int current_durability(const Db& db, const Combatant& c, const Id& item) {
    const auto it = c.durability.find(item);
    if (it != c.durability.end()) return it->second;
    const auto d = arms_def(db, item);
    return d ? d->durability : 0;
}

std::vector<std::string> combat_effects(const Combatant& c) {
    std::vector<std::string> out;
    if (c.dead) {
        out.push_back("dead");
        return out;
    }
    if (bleeding(c) > 0) out.push_back("bleeding");
    if (open_damage(c, kZoneHead) >= kConcussedAt) out.push_back("concussed");
    if (c.ko_hours > 0) out.push_back("knocked_out");
    if (limping(c)) out.push_back("limp");
    if (!c.captor.empty()) out.push_back("prisoner");
    if (!c.surrendered_to.empty()) out.push_back("surrendered");
    if (open_damage(c, kZoneArms) >= kUselessArmAt) out.push_back("useless_arm");
    if (has_lasting(c, "weak_arm")) out.push_back("weak_arm");
    if (c.stamina < kWindedAt) out.push_back("winded");
    if (c.outlaw) out.push_back("outlaw");
    for (const std::string& m : c.lasting)
        if (m.rfind("scar:", 0) == 0) out.push_back(m);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// --- equipment -------------------------------------------------------------------
EquipResult equip(const Db& db, CombatState& s, const Id& actor, const Id& item) {
    const std::optional<ArmsDef> def = arms_def(db, item);
    if (!def) return EquipResult::UnknownItem;
    Combatant& c = combatant_of(s, actor);
    if (c.dead) return EquipResult::Dead;
    if (def->slot == "melee" || def->slot == "ranged") {
        c.weapon = item;
        if (def->two_handed()) c.shield.clear();
    } else if (def->slot == "shield") {
        c.shield = item;
        if (auto w = arms_def(db, c.weapon); w && w->two_handed()) c.weapon.clear();
    } else if (def->slot == "armour") {
        std::vector<Id> kept;
        for (const Id& worn : c.armour) {
            const auto wd = arms_def(db, worn);
            bool overlaps = false;
            if (wd && wd->layer == def->layer)
                for (int z : wd->zones)
                    if (std::find(def->zones.begin(), def->zones.end(), z) != def->zones.end())
                        overlaps = true;
            if (!overlaps && worn != item) kept.push_back(worn);
        }
        kept.push_back(item);
        std::sort(kept.begin(), kept.end());
        c.armour = std::move(kept);
    } else {
        return EquipResult::UnknownItem;
    }
    return EquipResult::Ok;
}

bool unequip(CombatState& s, const Id& actor, const Id& item) {
    auto it = s.by_actor.find(actor);
    if (it == s.by_actor.end() || item.empty()) return false;
    Combatant& c = it->second;
    bool found = false;
    if (c.weapon == item) { c.weapon.clear(); found = true; }
    if (c.shield == item) { c.shield.clear(); found = true; }
    const auto a = std::find(c.armour.begin(), c.armour.end(), item);
    if (a != c.armour.end()) { c.armour.erase(a); found = true; }
    return found;
}

std::vector<Id> equipped(const Combatant& c) {
    std::vector<Id> out;
    if (!c.weapon.empty()) out.push_back(c.weapon);
    if (!c.shield.empty()) out.push_back(c.shield);
    for (const Id& a : c.armour) out.push_back(a);
    return out;
}

// --- the attack -----------------------------------------------------------------
AttackResult resolve_attack(const Db& db, const Rng& base, CombatState& s, const Fighter& A,
                            const Fighter& D, int zone_hint, DayNumber day) {
    AttackResult r;
    r.attacker = A.id;
    r.defender = D.id;
    if (A.id.empty() || D.id.empty()) return refuse(r, "no_actor");
    if (A.id == D.id) return refuse(r, "same_actor");
    Combatant& a = combatant_of(s, A.id);
    Combatant& d = combatant_of(s, D.id);
    if (!can_fight(a)) return refuse(r, "attacker_incapacitated");
    if (d.dead) return refuse(r, "defender_dead");

    const ArmsDef w = weapon_of(db, a);
    r.weapon = w.id;
    const bool ranged = w.slot == "ranged";
    Rng rng = base.fork(kCombatSalt ^ fnv1a64(A.id) ^ (fnv1a64(D.id) * 31u) ^
                        (s.seq * 0x9E3779B97F4A7C15ull));
    ++s.seq;
    record_hostility(s, A.id, D.id, day);

    // Stamina: the blow's cost grows with the weapon and the armour carried.
    const int load_g = armour_weight_g(db, a);
    const int cost = 2 + w.stamina * 2 + w.weight_g / 500 + load_g / 4000;
    int att_pen = 0;
    if (a.stamina < cost) {
        att_pen += 30;
        a.stamina = 0;
        r.winded = true;
    } else {
        a.stamina -= cost;
    }
    att_pen += std::min(40, open_damage(a, kZoneArms) / 2);
    att_pen += std::min(30, open_damage(a, kZoneHead) / 2);
    if (has_lasting(a, "weak_arm")) att_pen += 10;
    att_pen += needs_penalty(A.needs);

    int attack = A.in.skill(w.skill) + (ranged ? A.in.agility * 4 : A.in.agility * 3) +
                 w.balance * 2 + (w.speed - 5) * 2 + w.quality / 10 - att_pen;
    if (broken(db, a, w)) attack -= 10;

    const bool helpless = !can_fight(d);
    int chance = 10000;
    AttackOutcome miss_kind = AttackOutcome::Dodged;
    std::optional<ArmsDef> shield_def;
    ArmsDef dw = weapon_of(db, d);
    if (!helpless) {
        int def_pen = std::min(30, open_damage(d, kZoneHead) / 2) + needs_penalty(D.needs);
        if (d.stamina < 5) def_pen += 25;
        const int d_load = armour_weight_g(db, d);
        int best = INT_MIN;
        int stamina_cost = 0;
        shield_def = arms_def(db, d.shield);
        if (shield_def && !broken(db, d, *shield_def)) {
            const int block = D.in.skill("shields") + D.in.strength * 2 + shield_def->block +
                              shield_def->quality / 10 + (ranged ? 10 : 0);
            if (block > best) {
                best = block;
                miss_kind = AttackOutcome::Blocked;
                stamina_cost = 3 + shield_def->weight_g / 2000;
            }
        }
        const bool can_parry = !ranged && dw.slot == "melee" && (dw.id != "fists" || w.id == "fists");
        if (can_parry) {
            const int parry = D.in.skill(dw.skill) / 2 + dw.balance * 2 + D.in.agility;
            if (parry > best) {
                best = parry;
                miss_kind = AttackOutcome::Parried;
                stamina_cost = 3;
            }
        }
        const int dodge = D.in.skill("dodge") + D.in.agility * 3 - (d_load / 1000) * 2 -
                          (limping(d) ? 20 : 0) - (ranged ? 10 : 0);
        if (dodge > best) {
            best = dodge;
            miss_kind = AttackOutcome::Dodged;
            stamina_cost = 4 + d_load / 2000;
        }
        d.stamina = std::max(0, d.stamina - stamina_cost);
        if (!ranged) attack += clampi((w.reach_cm - dw.reach_cm) / 10, -10, 10);
        const int defence = best - def_pen;
        chance = clampi(kChanceBase + (attack - defence) * kChancePerPoint, kChanceMin, kChanceMax);
    }
    const int roll = rng.int_in(0, 9999);
    r.chance_bp = chance;
    r.roll_bp = roll;

    if (roll >= chance) {  // the blow does not land
        r.outcome = miss_kind;
        if (miss_kind == AttackOutcome::Blocked && shield_def) {
            r.shield_broke = wear(db, d, *shield_def, 1 + w.damage / 4);
            r.weapon_broke = wear(db, a, w, 1);
        } else if (miss_kind == AttackOutcome::Parried) {
            r.weapon_broke = wear(db, a, w, 1);
            (void)wear(db, d, dw, 1);
        }
        describe(r);
        return r;
    }
    const int margin = chance - roll;  // 1..10000

    // Where it lands.
    int zone = -1;
    if (zone_hint >= 0 && zone_hint < kZoneCount) {
        const int need = helpless ? 0
                         : zone_hint == kZoneHead ? kAimHead
                         : zone_hint == kZoneTorso ? 0
                                                   : kAimLimb;
        if (margin >= need) {
            zone = zone_hint;
            r.aimed = true;
        }
    }
    if (zone < 0) {
        const int z = rng.int_in(0, 99);
        zone = z < 10 ? kZoneHead : z < 50 ? kZoneTorso : z < 75 ? kZoneArms : kZoneLegs;
    }
    r.zone = zone;
    r.damage_type = w.damage_type;

    // How hard: the weapon, its quality and state, the arm behind it, the margin.
    int blow = w.damage * quality_pct(w.quality) / 100;
    if (broken(db, a, w)) blow /= 2;
    if (!ranged) blow += (A.in.strength - 5) * (w.damage_type == "blunt" ? 2 : 1);
    blow += margin / 1000;
    if (helpless) blow += 3;
    blow = std::max(1, blow);

    // Armour on that zone, both layers; a well-placed cut or thrust finds a gap.
    r.found_gap = margin >= kGapMargin && w.damage_type != "blunt";
    int prot = 0;
    std::vector<std::pair<ArmsDef, int>> worn_here;
    for (const Id& piece : d.armour) {
        const auto pd = arms_def(db, piece);
        if (!pd || std::find(pd->zones.begin(), pd->zones.end(), zone) == pd->zones.end()) continue;
        int p = broken(db, d, *pd) ? 0 : pd->protection(w.damage_type) * quality_pct(pd->quality) / 100;
        if (r.found_gap) p /= 2;
        prot += p;
        worn_here.emplace_back(*pd, p);
    }
    int net = blow - prot;
    if (net > 0) net = std::max(1, net * zone_multiplier_pct(zone) / 100);
    if (net <= 0 && w.damage_type == "blunt" && prot > 0) net = 1;  // the bruise under the armour
    r.weapon_broke = wear(db, a, w, 1);
    for (const auto& [pd, p] : worn_here)
        if (wear(db, d, pd, 1 + std::min(p, blow) / 3)) r.armour_broke = true;
    if (net <= 0) {
        r.outcome = AttackOutcome::Deflected;
        describe(r);
        return r;
    }

    // The wound.
    Wound wound;
    wound.zone = zone;
    wound.type = w.damage_type;
    wound.severity = severity_of(net);
    wound.damage = net;
    wound.remaining = net;
    wound.bleed = bleed_of(w.damage_type, wound.severity);
    wound.clot_hours = (wound.severity == kLight && wound.bleed > 0) ? 3 : 0;
    wound.day = day;
    d.wounds.push_back(wound);
    d.health = std::max(0, d.health - net);
    d.last_attacker = A.id;
    d.morale = clampi(d.morale - (net / 2 + 1), 0, 100);
    a.morale = clampi(a.morale + 2, 0, 100);
    r.damage = net;
    r.severity = wound.severity;
    r.bleed = wound.bleed;

    // A mortal cut or thrust to the head kills outright; a mortal blunt blow
    // stuns (the skull may hold) unless the body has nothing left.
    if (d.health <= 0 || (zone == kZoneHead && wound.severity == kMortal && w.damage_type != "blunt")) {
        mark_dead(s, D.id, A.id, "slain", day);
        r.outcome = AttackOutcome::Killed;
    } else if (w.damage_type == "blunt" && zone == kZoneHead && net >= kKnockoutBluntHead) {
        d.ko_hours = std::max(d.ko_hours, 1 + rng.int_in(0, 2));
        r.outcome = AttackOutcome::KnockedOut;
    } else if (d.health <= kCollapseAt && d.ko_hours <= 0) {
        d.ko_hours = 2;
        r.outcome = AttackOutcome::KnockedOut;
    } else {
        r.outcome = AttackOutcome::Wounded;
    }
    describe(r);
    return r;
}

// --- morale and stance -----------------------------------------------------------
Stance choose_stance(const Combatant& c, const CombatInputs& in, int allies, int enemies) {
    if (!conscious(c) || !c.captor.empty()) return Stance::None;
    if (!c.surrendered_to.empty()) return Stance::Surrender;
    const int resolve = c.morale + (allies - enemies) * 8 + (c.health - 50) / 2;
    const bool yields = in.surrender_at > 0;
    const bool can_flee = !limping(c);
    if (yields && c.health <= in.surrender_at && resolve < 40) return Stance::Surrender;
    if (c.health <= in.flee_at || resolve < 15) {
        if (can_flee) return Stance::Flee;
        if (yields) return Stance::Surrender;
    }
    return Stance::Fight;
}

// --- group skirmish ---------------------------------------------------------------
SkirmishResult resolve_skirmish(const Db& db, const Rng& base, CombatState& s,
                                const std::vector<Id>& side_a, const std::vector<Id>& side_b,
                                const std::map<Id, CombatInputs>& inputs,
                                const std::map<Id, const Needs*>& needs, DayNumber day,
                                int max_rounds) {
    SkirmishResult out;
    std::vector<Id> order;
    std::map<Id, int> side;
    for (std::size_t i = 0; i < std::max(side_a.size(), side_b.size()); ++i) {
        if (i < side_a.size() && !side.count(side_a[i])) { order.push_back(side_a[i]); side[side_a[i]] = 0; }
        if (i < side_b.size() && !side.count(side_b[i])) { order.push_back(side_b[i]); side[side_b[i]] = 1; }
    }
    std::set<Id> fled;
    const CombatInputs defaults;
    auto in_of = [&](const Id& id) -> const CombatInputs& {
        const auto it = inputs.find(id);
        return it == inputs.end() ? defaults : it->second;
    };
    auto needs_of_id = [&](const Id& id) -> const Needs* {
        const auto it = needs.find(id);
        return it == needs.end() ? nullptr : it->second;
    };
    auto active = [&](const Id& id) { return !fled.count(id) && can_fight(combatant_of(s, id)); };
    auto count_active = [&](int sd) {
        int n = 0;
        for (const Id& id : order)
            if (side[id] == sd && active(id)) ++n;
        return n;
    };
    for (const Id& id : order) (void)combatant_of(s, id);

    for (int round = 1; round <= std::max(0, max_rounds); ++round) {
        if (count_active(0) == 0 || count_active(1) == 0) break;
        out.rounds = round;
        for (const Id& id : order) {
            if (!active(id)) continue;
            const int my = side[id];
            const int allies = count_active(my) - 1;
            const int enemies = count_active(1 - my);
            if (enemies == 0) break;
            Combatant& me = combatant_of(s, id);
            const Stance st = choose_stance(me, in_of(id), allies, enemies);
            if (st == Stance::Flee) {
                fled.insert(id);
                out.fled.push_back(id);
                continue;
            }
            // The weakest and the strongest standing enemies (health, then id).
            Id weakest, strongest;
            for (const Id& e : order) {
                if (side[e] == my || !active(e)) continue;
                const Combatant& ec = combatant_of(s, e);
                if (weakest.empty() || ec.health < combatant_of(s, weakest).health) weakest = e;
                if (strongest.empty() || ec.health > combatant_of(s, strongest).health) strongest = e;
            }
            if (st == Stance::Surrender) {
                if (me.surrendered_to.empty()) (void)surrender(s, id, strongest);
                out.surrendered.push_back(id);
                continue;
            }
            if (st != Stance::Fight || weakest.empty()) continue;
            const Fighter fa{id, in_of(id), needs_of_id(id)};
            const Fighter fd{weakest, in_of(weakest), needs_of_id(weakest)};
            AttackResult r = resolve_attack(db, base, s, fa, fd, in_of(id).favoured_zone, day);
            if (r.outcome == AttackOutcome::KnockedOut || r.outcome == AttackOutcome::Killed) {
                out.fallen.push_back(weakest);
                for (const Id& ally : order)  // a comrade falls: his side's hearts sink
                    if (side[ally] == 1 - my && ally != weakest) {
                        Combatant& ac = combatant_of(s, ally);
                        ac.morale = clampi(ac.morale - 10, 0, 100);
                    }
            }
            out.log.push_back(std::move(r));
        }
        for (const Id& id : order)
            if (active(id)) catch_breath(s, id, 1);
    }
    const int a_left = count_active(0), b_left = count_active(1);
    out.winner = (a_left > 0 && b_left == 0) ? 0 : (b_left > 0 && a_left == 0) ? 1 : -1;
    return out;
}

// --- time and healing ---------------------------------------------------------------
bool advance_combat_hours(CombatState& s, const Id& actor, int hours, bool resting, DayNumber day) {
    auto it = s.by_actor.find(actor);
    if (it == s.by_actor.end() || hours <= 0) return false;
    Combatant& c = it->second;
    for (int h = 0; h < hours && !c.dead; ++h) {
        int loss = 0;
        for (Wound& w : c.wounds) {
            if (w.remaining <= 0 || w.bleed <= 0) continue;
            loss += w.bleed;
            if (w.clot_hours > 0 && --w.clot_hours == 0) w.bleed = 0;
        }
        c.health -= loss;
        if (c.health <= 0) {
            mark_dead(s, actor, c.last_attacker, "bled_out", day);
            return true;
        }
        c.stamina = std::min(100, c.stamina + (resting ? 40 : 20));
        if (c.ko_hours > 0) --c.ko_hours;
        if (resting) c.rest_hours = std::min(24, c.rest_hours + 1);
    }
    return false;
}

void catch_breath(CombatState& s, const Id& actor, int rounds) {
    auto it = s.by_actor.find(actor);
    if (it == s.by_actor.end() || rounds <= 0 || it->second.dead) return;
    it->second.stamina = std::min(100, it->second.stamina + kBreathStamina * rounds);
}

int treat_wounds(CombatState& s, const Id& actor, int method, bool with_linen) {
    auto it = s.by_actor.find(actor);
    if (it == s.by_actor.end() || it->second.dead) return 0;
    int improved = 0;
    for (Wound& w : it->second.wounds) {
        if (w.remaining <= 0) continue;
        const int before_bleed = w.bleed, before_treat = w.treatment;
        if (method == kTreatBound) {
            int cap = 0;
            if (w.severity == kGrave) cap = with_linen ? 0 : 1;
            if (w.severity == kMortal) cap = with_linen ? 1 : 2;
            w.bleed = std::min(w.bleed, cap);
        } else if (method == kTreatHerbs) {
            w.bleed = std::min(w.bleed, w.severity == kMortal ? 1 : 0);
        } else if (method == kTreatHealer) {
            w.bleed = 0;
        } else {
            continue;
        }
        if (w.bleed == 0) w.clot_hours = 0;
        // A bound wound that still seeps stops within hours (herbs sooner).
        else if (w.clot_hours == 0) w.clot_hours = method == kTreatHerbs ? 3 : 6;
        w.treatment = std::max(w.treatment, method);
        if (w.bleed != before_bleed || w.treatment != before_treat) ++improved;
    }
    return improved;
}

std::vector<Id> tick_combat(CombatState& s, DayNumber day) {
    std::vector<Id> died;
    std::vector<Id> ids;
    for (const auto& [id, c] : s.by_actor) ids.push_back(id);  // sorted (std::map)
    for (const Id& id : ids) {
        Combatant& c = s.by_actor.at(id);
        if (c.dead) continue;
        // Conscious npcs bind their own wounds; the player's hands are the engine's.
        if (id != "player" && conscious(c) && bleeding(c) > 0) (void)treat_wounds(s, id, kTreatBound, false);
        // Whoever is still bleeding bleeds out the day's remaining hours.
        if (bleeding(c) > 0 && advance_combat_hours(s, id, 24, false, day)) {
            died.push_back(id);
            continue;
        }
        const bool rested = c.rest_hours >= 8;
        for (Wound& w : c.wounds) {
            if (w.remaining <= 0) continue;
            if (w.bleed > 0 && w.treatment == kTreatNone) continue;  // an open, bleeding wound does not knit
            int heal = 1 + (rested ? 1 : 0) + (w.treatment >= kTreatHerbs ? 1 : 0) +
                       (w.treatment >= kTreatHealer ? 1 : 0);
            w.remaining = std::max(0, w.remaining - heal);
            if (w.remaining > 0) continue;
            w.bleed = 0;
            if (w.severity >= kSerious) add_lasting(c, std::string("scar:") + zone_name(w.zone));
            if (w.severity >= kGrave && w.treatment < kTreatHealer) {
                if (w.zone == kZoneLegs) add_lasting(c, "limp");
                if (w.zone == kZoneArms) add_lasting(c, "weak_arm");
            }
        }
        c.wounds.erase(std::remove_if(c.wounds.begin(), c.wounds.end(),
                                      [](const Wound& w) { return w.remaining <= 0; }),
                       c.wounds.end());
        c.health = std::min(health_cap(c), c.health + 4 + (rested ? 6 : 0));
        if (c.morale < 50) c.morale = std::min(50, c.morale + 5);
        if (c.morale > 50) c.morale = std::max(50, c.morale - 5);
        c.ko_hours = 0;  // nobody lies senseless for a whole day from a blow
        c.rest_hours = 0;
    }
    return died;
}

// --- yielding, prisoners, duels, outlaws --------------------------------------------
bool surrender(CombatState& s, const Id& who, const Id& to) {
    Combatant& c = combatant_of(s, who);
    if (c.dead || !c.captor.empty() || who == to) return false;
    c.surrendered_to = to;
    return true;
}

bool take_prisoner(CombatState& s, const Id& captor, const Id& captive, DayNumber day) {
    if (captor == captive || captor.empty() || captive.empty()) return false;
    Combatant& c = combatant_of(s, captive);
    const Combatant& k = combatant_of(s, captor);
    if (c.dead || !c.captor.empty() || !can_fight(k)) return false;
    if (c.surrendered_to != captor && c.ko_hours <= 0) return false;
    c.captor = captor;
    c.surrendered_to = captor;
    s.prisoners.push_back(Prisoner{captive, captor, day});
    return true;
}

bool release_prisoner(CombatState& s, const Id& captive) {
    const auto it = std::find_if(s.prisoners.begin(), s.prisoners.end(),
                                 [&](const Prisoner& p) { return p.captive == captive; });
    if (it == s.prisoners.end()) return false;
    s.prisoners.erase(it);
    Combatant& c = combatant_of(s, captive);
    c.captor.clear();
    c.surrendered_to.clear();
    return true;
}

const Prisoner* find_prisoner(const CombatState& s, const Id& captive) {
    for (const Prisoner& p : s.prisoners)
        if (p.captive == captive) return &p;
    return nullptr;
}

void agree_duel(CombatState& s, const Id& a, const Id& b, DayNumber day) {
    if (a == b || duel_agreed(s, a, b, day)) return;
    s.duels.push_back(Duel{std::min(a, b), std::max(a, b), day});
}

bool duel_agreed(const CombatState& s, const Id& a, const Id& b, DayNumber day) {
    const Id lo = std::min(a, b), hi = std::max(a, b);
    for (const Duel& d : s.duels)
        if (d.a == lo && d.b == hi && d.day == day) return true;
    return false;
}

Id first_aggressor(const CombatState& s, const Id& a, const Id& b, DayNumber day) {
    for (const Hostility& h : s.hostilities) {
        const bool pair = (h.aggressor == a && h.other == b) || (h.aggressor == b && h.other == a);
        if (pair && h.day == day) return h.aggressor;
    }
    return {};
}

void set_outlaw(CombatState& s, const Id& actor, bool outlaw) { combatant_of(s, actor).outlaw = outlaw; }

void mark_dead(CombatState& s, const Id& actor, const Id& killer, const std::string& cause,
               DayNumber day) {
    Combatant& c = combatant_of(s, actor);
    if (c.dead) return;
    c.dead = true;
    c.health = 0;
    c.ko_hours = 0;
    s.deaths.push_back(DeathRecord{actor, day, killer, cause});
}

// --- combat styles ----------------------------------------------------------------
std::optional<CombatStyle> combat_style(const Db& db, const Id& style_id) {
    const std::optional<Row> row = db.find("combat_styles", style_id);
    if (!row || row_open(*row)) return std::nullopt;
    CombatStyle st;
    st.id = style_id;
    st.faction = trim(row->get("faction"));
    st.role = lower(trim(row->get("role")));
    st.weapon = trim(row->get("weapon"));
    st.shield = trim(row->get("shield"));
    st.armour = split_semi(row->get("armour"));
    st.skill = clampi(to_int(row->get("skill"), 15), 0, 100);
    st.morale = clampi(to_int(row->get("morale"), 50), 0, 100);
    st.flee_at = clampi(to_int(row->get("flee_at"), 30), 0, 100);
    st.surrender_at = clampi(to_int(row->get("surrender_at"), 15), 0, 100);
    st.favoured_zone = zone_from_name(row->get("favoured_zone"));
    return st;
}

Id style_for(const Db& db, const Id& faction, const std::string& role) {
    const std::string r = lower(trim(role));
    if (!r.empty())
        for (const Row& row : db.rows("combat_styles"))
            if (!row_open(row) && lower(trim(row.get("role"))) == r) return row.at("id");
    if (!faction.empty())
        for (const Row& row : db.rows("combat_styles"))
            if (!row_open(row) && trim(row.get("faction")) == faction) return row.at("id");
    return combat_style(db, "commoner") ? Id("commoner") : Id();
}

CombatInputs style_inputs(const CombatStyle& st) {
    CombatInputs in;
    for (const char* tok : {"blades", "axes", "maces", "spears", "bows", "slings", "shields", "unarmed"})
        in.skills[tok] = st.skill;
    in.skills["dodge"] = std::max(0, st.skill - 10);
    in.default_skill = st.skill / 2;
    in.flee_at = st.flee_at;
    in.surrender_at = st.surrender_at;
    in.favoured_zone = st.favoured_zone;
    return in;
}

bool apply_style(const Db& db, CombatState& s, const Id& actor, const Id& style_id) {
    const std::optional<CombatStyle> st = combat_style(db, style_id);
    if (!st) return false;
    Combatant& c = combatant_of(s, actor);
    if (c.dead) return false;
    c.style = style_id;
    c.morale = st->morale;
    if (!st->weapon.empty()) (void)equip(db, s, actor, st->weapon);
    if (!st->shield.empty()) (void)equip(db, s, actor, st->shield);
    for (const Id& piece : st->armour) (void)equip(db, s, actor, piece);
    return true;
}

}  // namespace sim
