// CombatActions.cpp — W4-B: implements sim/CombatActions.hpp (the caller
// layer over Combat). Like Actions.cpp, one of the few files allowed to
// write several modules' state; every write below is a sequencing of the
// modules' own public verbs (needs_of, witness, commit_crime, purses,
// issue_loan) plus the documented Population/Events removals of a death.
#include "sim/CombatActions.hpp"

#include "sim/Actions.hpp"
#include "sim/Progression.hpp"  // W4-A: attributes, skills, growth by use

#include <algorithm>
#include <set>

namespace sim {
namespace {

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

int held(const WorldState& w, const Id& actor, const Id& item) {
    const auto inv = w.inventories.find(actor);
    if (inv == w.inventories.end()) return 0;
    const auto it = inv->second.counts.find(item);
    return it == inv->second.counts.end() ? 0 : it->second;
}

void give(WorldState& w, const Id& actor, const Id& item, int qty) {
    int& n = w.inventories[actor].counts[item];
    n = std::max(0, n + qty);
    if (n == 0) w.inventories[actor].counts.erase(item);
}

Silver band_of(const Db& db, const Id& item) {
    const auto row = db.find("items", item);
    if (!row || row->get("tag") == "OPEN") return 1;
    Silver band = 0;
    for (const char c : row->get("price_band")) {
        if (c < '0' || c > '9') break;
        band = band * 10 + (c - '0');
    }
    return band > 0 ? band : 1;
}

const Needs* needs_ptr(const WorldState& w, const Id& actor) {
    const auto it = w.needs.by_actor.find(actor);
    return it == w.needs.by_actor.end() ? nullptr : &it->second;
}

std::string display_name(const WorldState& w, const Id& id) {
    if (id.empty()) return "an unknown hand";
    if (const Npc* n = find_npc(w.population, id); n && !n->name.empty()) return n->name;
    return id;
}

bool is_castable(const std::string& tier) {
    return tier == "copper" || tier == "arsenical" || tier == "tin_bronze";
}

bool is_smith(const WorldState& w, const Id& smith) {
    if (find_npc(w.population, smith) == nullptr) return false;
    if (const Combatant* c = find_combatant(w.combat, smith); c && c->dead) return false;
    const auto person = w.db.find("people", smith);
    if (!person) return false;
    const auto place = w.db.find("places", person->get("work_place"));
    return place && lower(place->get("kind")) == "smithy";
}

bool is_healer(const WorldState& w, const Id& healer) {
    const Npc* n = find_npc(w.population, healer);
    if (n == nullptr || lower(n->role) != "physician") return false;
    const Combatant* c = find_combatant(w.combat, healer);
    return c == nullptr || conscious(*c);
}

bool pay(WorldState& w, const Id& from, const Id& to, Silver fee) {
    if (fee <= 0) return true;
    if (!take_from_purse(w.property, from, fee)) return false;
    credit_purse(w.property, to, fee);
    return true;
}

void unequip_ranged_without_ammo(WorldState& w, const Id& actor) {
    Combatant& c = combatant_of(w.combat, actor);
    const auto def = arms_def(w.db, c.weapon);
    if (def && def->slot == "ranged" && !def->ammo.empty() && held(w, actor, def->ammo) <= 0)
        c.weapon.clear();
}

// W4-A: the combat skill tokens (arms.csv `skill`, plus dodge and unarmed)
// read W4-A's skills.csv. Dodging is footwork, the wrestler's art.
struct TokenSkill {
    const char* token;
    const char* skill;
};
constexpr TokenSkill kTokenSkills[] = {
    {"blades", "dagger"},  {"axes", "mace_and_axe"},  {"maces", "mace_and_axe"},
    {"spears", "spear"},   {"bows", "bow_and_sling"}, {"slings", "bow_and_sling"},
    {"shields", "shield"}, {"unarmed", "wrestling"},  {"dodge", "wrestling"},
};
// Anyone can swing a club: an untrained fighter still counts this much.
constexpr int kUntrainedFloor = 10;

Id skill_for_token(const std::string& token) {
    for (const TokenSkill& t : kTokenSkills)
        if (token == t.token) return t.skill;
    return {};
}

// Skill growth by use (W4-A note_skill_use: the player's sheet only).
void note_combat_xp(WorldState& w, const AttackResult& r) {
    if (r.outcome == AttackOutcome::Invalid) return;
    const auto wd = arms_def(w.db, r.weapon);
    const std::string token = wd ? wd->skill : std::string("unarmed");
    const bool landed = static_cast<int>(r.outcome) >= static_cast<int>(AttackOutcome::Wounded);
    (void)note_skill_use(w, r.attacker, skill_for_token(token), landed ? 3 : 1);
    if (r.outcome == AttackOutcome::Blocked) (void)note_skill_use(w, r.defender, "shield", 2);
    if (r.outcome == AttackOutcome::Dodged) (void)note_skill_use(w, r.defender, "wrestling", 1);
    if (r.outcome == AttackOutcome::Parried) {
        const Combatant* d = find_combatant(w.combat, r.defender);
        const auto dw = d != nullptr ? arms_def(w.db, d->weapon) : std::nullopt;
        (void)note_skill_use(w, r.defender, skill_for_token(dw ? dw->skill : "unarmed"), 2);
    }
}

}  // namespace

CombatInputs combat_inputs_for(const WorldState& w, const Id& actor) {
    // A combat style is the fighter's drill; W4-A's sheet is what he has
    // learned. Each skill takes the better of the two (never below the
    // untrained floor); the attributes are the sheet's.
    CombatInputs in;
    in.default_skill = kUntrainedFloor;
    const Combatant* c = find_combatant(w.combat, actor);
    if (c != nullptr && !c->style.empty())
        if (const auto st = combat_style(w.db, c->style)) in = style_inputs(*st);
    struct AttrSlot {
        const char* id;
        int* slot;
    };
    for (const AttrSlot a : {AttrSlot{"strength", &in.strength}, AttrSlot{"agility", &in.agility},
                             AttrSlot{"endurance", &in.endurance}}) {
        const int v = actor_attribute(w, actor, a.id);
        if (v > 0) *a.slot = v;
    }
    for (const TokenSkill& t : kTokenSkills) {
        const auto have = in.skills.find(t.token);
        const int drilled = have == in.skills.end() ? kUntrainedFloor : have->second;
        in.skills[t.token] =
            std::max({drilled, kUntrainedFloor, actor_effective_skill(w, actor, t.skill)});
    }
    return in;
}

Id style_of_npc(const WorldState& w, const Id& npc) {
    const Npc* n = find_npc(w.population, npc);
    if (n == nullptr) return {};
    return style_for(w.db, n->faction_id, n->role);
}

bool apply_style_in_world(WorldState& w, const Id& actor, const Id& style_id) {
    const auto st = combat_style(w.db, style_id);
    if (!st) return false;
    std::vector<Id> kit = st->armour;
    if (!st->weapon.empty()) kit.push_back(st->weapon);
    if (!st->shield.empty()) kit.push_back(st->shield);
    for (const Id& item : kit)
        if (held(w, actor, item) <= 0) give(w, actor, item, 1);
    if (const auto wd = arms_def(w.db, st->weapon); wd && !wd->ammo.empty() && held(w, actor, wd->ammo) <= 0)
        give(w, actor, wd->ammo, wd->ammo == st->weapon ? 3 : 12);  // a quiver / a pouch
    return apply_style(w.db, w.combat, actor, style_id);
}

void ensure_combatant(WorldState& w, const Id& actor) {
    if (actor.empty() || actor == "player" || find_combatant(w.combat, actor) != nullptr) return;
    const Id style = style_of_npc(w, actor);
    if (!style.empty()) (void)apply_style_in_world(w, actor, style);
    (void)combatant_of(w.combat, actor);
}

int equip_in_world(WorldState& w, const Id& actor, const Id& item) {
    if (!arms_def(w.db, item)) return -2;
    if (held(w, actor, item) <= 0) return -3;
    ensure_combatant(w, actor);
    switch (equip(w.db, w.combat, actor, item)) {
        case EquipResult::Ok: return 0;
        case EquipResult::Dead: return -4;
        default: return -2;
    }
}

AttackResult attack_in_world(WorldState& w, const Id& attacker, const Id& defender, int zone_hint) {
    ensure_combatant(w, attacker);
    ensure_combatant(w, defender);
    const Combatant& a = combatant_of(w.combat, attacker);
    const auto wd = arms_def(w.db, a.weapon);
    const bool uses_ammo = wd && wd->slot == "ranged" && !wd->ammo.empty();
    if (uses_ammo && held(w, attacker, wd->ammo) <= 0) {
        AttackResult r;
        r.attacker = attacker;
        r.defender = defender;
        r.refusal = "no_ammunition";
        r.text = "outcome=invalid;refusal=no_ammunition";
        return r;
    }
    const Fighter fa{attacker, combat_inputs_for(w, attacker), needs_ptr(w, attacker)};
    const Fighter fd{defender, combat_inputs_for(w, defender), needs_ptr(w, defender)};
    AttackResult r = resolve_attack(w.db, w.rng, w.combat, fa, fd, zone_hint, w.day);
    if (r.outcome == AttackOutcome::Invalid) return r;
    note_combat_xp(w, r);
    if (uses_ammo) {
        give(w, attacker, wd->ammo, -1);
        if (wd->ammo == wd->id && held(w, attacker, wd->id) <= 0) (void)unequip(w.combat, attacker, wd->id);
    }
    if (r.outcome == AttackOutcome::Killed) on_killed(w, defender, attacker, "slain");
    return r;
}

SkirmishResult skirmish_in_world(WorldState& w, const std::vector<Id>& side_a,
                                 const std::vector<Id>& side_b, int max_rounds) {
    std::map<Id, CombatInputs> inputs;
    std::map<Id, const Needs*> needs;
    for (const auto* side : {&side_a, &side_b})
        for (const Id& id : *side) {
            ensure_combatant(w, id);
            unequip_ranged_without_ammo(w, id);
            inputs[id] = combat_inputs_for(w, id);
            needs[id] = needs_ptr(w, id);
        }
    const std::size_t deaths_before = w.combat.deaths.size();
    SkirmishResult r = resolve_skirmish(w.db, w.rng, w.combat, side_a, side_b, inputs, needs, w.day,
                                        max_rounds);
    for (const AttackResult& blow : r.log) {
        note_combat_xp(w, blow);
        const auto wd = arms_def(w.db, blow.weapon);
        if (wd && wd->slot == "ranged" && !wd->ammo.empty()) give(w, blow.attacker, wd->ammo, -1);
    }
    const std::vector<DeathRecord> fresh(w.combat.deaths.begin() + static_cast<std::ptrdiff_t>(deaths_before),
                                         w.combat.deaths.end());
    for (const DeathRecord& d : fresh) on_killed(w, d.id, d.killer, d.cause);
    return r;
}

void on_killed(WorldState& w, const Id& victim, const Id& killer, const std::string& cause) {
    mark_dead(w.combat, victim, killer, cause, w.day);  // idempotent
    const std::string name = display_name(w, victim);
    const std::string killer_name = display_name(w, killer);

    const auto npc_it = std::find_if(w.population.npcs.begin(), w.population.npcs.end(),
                                     [&](const Npc& n) { return n.id == victim; });
    if (npc_it != w.population.npcs.end()) {
        std::vector<Id> mourners;
        if (const auto k = w.population.knows.find(victim); k != w.population.knows.end())
            mourners = k->second;
        w.population.npcs.erase(npc_it);  // out of the population: no schedule answers for him
        w.population.knows.erase(victim);
        for (auto& [id, edges] : w.population.knows)
            edges.erase(std::remove(edges.begin(), edges.end(), victim), edges.end());
        for (const Id& m : mourners)
            if (m != killer)
                witness(w.population, m, victim,
                        "mourns " + name + ", " + (cause == "bled_out" ? "bled to death" : "killed") +
                            " by " + killer_name,
                        w.day);
    }
    w.needs.by_actor.erase(victim);

    // The dead hold no prisoners, and are no one's.
    std::vector<Id> freed;
    for (const Prisoner& p : w.combat.prisoners)
        if (p.captor == victim || p.captive == victim) freed.push_back(p.captive);
    for (const Id& id : freed) (void)release_prisoner(w.combat, id);

    w.events.fired.push_back(TriggeredEvent{
        "combat_death", w.day,
        name + (cause == "bled_out" ? " bled to death of wounds from " : " was killed by ") + killer_name});
}

bool advance_body_hours(WorldState& w, const Id& actor, int hours, bool resting) {
    if (hours <= 0) return false;
    const Combatant* c = find_combatant(w.combat, actor);
    if (c == nullptr || c->dead) return false;
    const int pain = open_damage(*c, -1) / 25;
    const int bleeding_now = bleeding(*c) > 0 ? 1 : 0;
    const int heat = resting ? 0 : armour_heat(w.db, *c) / 4;
    const Id killer = c->last_attacker;
    const bool died = advance_combat_hours(w.combat, actor, hours, resting, w.day);
    Needs& n = needs_of(w.needs, actor);
    n.fatigue = std::min(100, n.fatigue + (pain + heat) * hours);
    n.thirst = std::min(100, n.thirst + bleeding_now * hours);
    if (died) on_killed(w, actor, killer, "bled_out");
    return died;
}

bool rest_in_world(WorldState& w, const Id& actor, int hours) {
    if (hours <= 0) return false;
    advance_needs(w.needs, actor, hours, /*sleeping=*/true);
    return advance_body_hours(w, actor, hours, /*resting=*/true);
}

int treat_in_world(WorldState& w, const Id& actor, const std::string& method, const Id& healer) {
    const Combatant* c = find_combatant(w.combat, actor);
    if (c != nullptr && c->dead) return -6;
    // W4-A: a hand that knows medicine binds as well as clean linen does.
    const bool physician_hand =
        actor_effective_skill(w, actor, "medicine") >= kSkilledBinderMedicine;
    if (method == "bind") {
        const bool linen = held(w, actor, "linen_bandage") > 0;
        const int n = treat_wounds(w.combat, actor, kTreatBound, linen || physician_hand);
        if (linen && n > 0) give(w, actor, "linen_bandage", -1);
        if (n > 0) (void)note_use(w, actor, "verb:treat", 1);
        return n;
    }
    if (method == "herbs") {
        if (held(w, actor, "healing_herbs") <= 0) return -3;
        const int n = treat_wounds(w.combat, actor, kTreatHerbs, false);
        if (n > 0) give(w, actor, "healing_herbs", -1);
        if (n > 0) (void)note_use(w, actor, "verb:treat", 2);
        return n;
    }
    if (method == "healer") {
        if (!is_healer(w, healer) || healer == actor) return -4;
        if (!pay(w, actor, healer, kHealerFee)) return -5;
        return treat_wounds(w.combat, actor, kTreatHealer, true);
    }
    return -2;
}

Id file_combat_crime(WorldState& w, const Id& attacker, const Id& victim, const Id& place_city,
                     const std::vector<Id>& witnesses, Id* crime_id) {
    if (crime_id != nullptr) crime_id->clear();
    // The pair's quarrel on record (Combat keeps one per pair: its day and first blow).
    const Hostility* h = nullptr;
    for (const Hostility& x : w.combat.hostilities)
        if ((x.aggressor == attacker && x.other == victim) || (x.aggressor == victim && x.other == attacker))
            h = &x;
    if (h == nullptr) return {};
    const DayNumber day = h->day;
    const Combatant& v = combatant_of(w.combat, victim);
    bool killed = false;
    for (const DeathRecord& d : w.combat.deaths)
        if (d.id == victim && d.killer == attacker) killed = true;

    bool victim_outlaw = v.outlaw;
    if (victim == "player")
        for (const auto& [faction, flag] : w.faction.outlawed_by_faction)
            if (flag) victim_outlaw = true;

    Id law;
    if (v.surrendered_to == attacker || v.captor == attacker)
        law = killed ? "murder" : "assault";   // a man who yielded is under protection
    else if (victim_outlaw)
        law = "slaying_a_robber";
    else if (duel_agreed(w.combat, attacker, victim, day))
        law = killed ? "duel_killing" : "";
    else if (first_aggressor(w.combat, attacker, victim, day) == victim)
        law = "self_defence";
    else
        law = killed ? "murder" : "assault";
    if (law.empty()) return {};
    const Id id = commit_crime(w, attacker, law, place_city, witnesses, victim);
    if (crime_id != nullptr) *crime_id = id;
    return law;
}

Silver ransom_prisoner(WorldState& w, const Id& captive, Id* loan_id) {
    if (loan_id != nullptr) loan_id->clear();
    const Prisoner* p = find_prisoner(w.combat, captive);
    if (p == nullptr) return -1;
    const Id captor = p->captor;
    const Silver paid = std::min<Silver>(kCaptiveRansomSilver, std::max<Silver>(0, purse(w.property, captive)));
    if (paid > 0) (void)pay(w, captive, captor, paid);
    if (paid < kCaptiveRansomSilver) {
        Loan& l = issue_loan(w.property, captive, captor, kCaptiveRansomSilver - paid, kRansomLoanRatePct, w.day,
                             kRansomLoanTermDays);
        if (loan_id != nullptr) *loan_id = l.id;
    }
    (void)release_prisoner(w.combat, captive);
    return paid;
}

int loot_body(WorldState& w, const Id& looter, const Id& body) {
    if (looter == body) return -1;
    Combatant& b = combatant_of(w.combat, body);
    if (!b.dead && b.captor != looter) return -1;
    const auto inv = w.inventories.find(body);
    int moved = 0;
    Combatant& l = combatant_of(w.combat, looter);
    if (inv != w.inventories.end()) {
        const std::map<Id, int> goods = inv->second.counts;
        for (const auto& [item, qty] : goods) {
            if (qty <= 0) continue;
            if (const auto d = b.durability.find(item); d != b.durability.end() && held(w, looter, item) <= 0)
                l.durability[item] = d->second;  // the dead man's notched blade stays notched
            give(w, looter, item, qty);
            moved += qty;
        }
        inv->second.counts.clear();
    }
    b.weapon.clear();
    b.shield.clear();
    b.armour.clear();
    b.durability.clear();
    return moved;
}

int repair_at_smith(WorldState& w, const Id& actor, const Id& item, const Id& smith) {
    const auto def = arms_def(w.db, item);
    if (!def) return -2;
    if (held(w, actor, item) <= 0) return -3;
    if (!is_smith(w, smith)) return -4;
    Combatant& c = combatant_of(w.combat, actor);
    const int cur = current_durability(w.db, c, item);
    if (def->durability <= 0 || cur >= def->durability) return -6;
    if (cur <= 0 && is_castable(def->material_tier)) return -7;
    const Silver fee = std::max<Silver>(1, band_of(w.db, item) * kRepairFeePerBand *
                                               (def->durability - cur) / def->durability);
    if (!pay(w, actor, smith, fee)) return -5;
    c.durability.erase(item);  // full again
    return 0;
}

int recast_at_smith(WorldState& w, const Id& actor, const Id& from, const Id& into, const Id& smith) {
    const auto fd = arms_def(w.db, from);
    const auto id = arms_def(w.db, into);
    if (!fd || !id) return -2;
    if (held(w, actor, from) <= 0) return -3;
    if (!is_smith(w, smith)) return -4;
    if (!is_castable(fd->material_tier) || !is_castable(id->material_tier)) return -7;
    Silver fee = band_of(w.db, into) * kRecastFeePerBand;
    if (id->weight_g > fd->weight_g) fee += (id->weight_g - fd->weight_g) / 100;  // more metal
    if (id->material_tier == "tin_bronze" && fd->material_tier != "tin_bronze") fee += kTinFee;
    if (!pay(w, actor, smith, fee)) return -5;
    Combatant& c = combatant_of(w.combat, actor);
    give(w, actor, from, -1);
    if (held(w, actor, from) <= 0) {
        (void)unequip(w.combat, actor, from);
        c.durability.erase(from);
    }
    give(w, actor, into, 1);
    c.durability.erase(into);  // fresh from the mould
    return 0;
}

void tick_combat_world(WorldState& w) {
    std::map<Id, Id> killer;
    for (const auto& [id, c] : w.combat.by_actor) killer[id] = c.last_attacker;
    for (const Id& id : tick_combat(w.combat, w.day)) on_killed(w, id, killer[id], "bled_out");
}

SkirmishOutcome combat_skirmish_adapter(const SkirmishRequest& req, Rng& rng) {
    static const Db kNoCanon;  // immutable and empty: fighters carry ordinary_arms
    CombatState s;
    std::map<Id, CombatInputs> inputs;
    std::vector<Id> sides[2];
    const SkirmishSide* reqs[2] = {&req.attacker, &req.defender};
    for (int sd = 0; sd < 2; ++sd) {
        const SkirmishSide& side = *reqs[sd];
        const int n = std::clamp(side.fighters, 0, kWildSideCap);
        const int prowess = std::clamp(side.prowess_pct, 1, 300);
        for (int i = 0; i < n; ++i) {
            const Id id = (side.has_player && i == 0) ? Id("player")
                                                      : Id(sd == 0 ? "atk_" : "def_") + std::to_string(i);
            sides[sd].push_back(id);
            Combatant& c = combatant_of(s, id);
            c.weapon = "ordinary_spear";
            c.morale = std::clamp(side.morale, 0, 100);
            CombatInputs in;
            in.default_skill = std::clamp(prowess * 35 / 100, 5, 95);
            in.strength = std::clamp(2 + prowess * 3 / 100, 1, 10);
            in.agility = in.strength;
            inputs[id] = in;
        }
    }
    const Rng base(rng.next());
    const SkirmishResult r = resolve_skirmish(kNoCanon, base, s, sides[0], sides[1], inputs, {}, 1, 20);

    auto side_of = [&](const Id& id) {
        return std::find(sides[0].begin(), sides[0].end(), id) != sides[0].end() ? 0 : 1;
    };
    std::set<Id> out_of_fight(r.fallen.begin(), r.fallen.end());
    out_of_fight.insert(r.surrendered.begin(), r.surrendered.end());
    int lost[2] = {0, 0};
    for (const Id& id : out_of_fight) ++lost[side_of(id)];
    int standing[2] = {0, 0}, health[2] = {0, 0};
    const std::set<Id> fled(r.fled.begin(), r.fled.end());
    for (int sd = 0; sd < 2; ++sd)
        for (const Id& id : sides[sd]) {
            const Combatant& c = combatant_of(s, id);
            if (can_fight(c) && !fled.count(id)) {
                ++standing[sd];
                health[sd] += c.health;
            }
        }

    SkirmishOutcome out;
    if (r.winner >= 0)
        out.attacker_won = r.winner == 0;
    else
        out.attacker_won = standing[0] > standing[1] ||
                           (standing[0] == standing[1] && health[0] > health[1]);
    for (int sd = 0; sd < 2; ++sd) {
        const int n = static_cast<int>(sides[sd].size());
        const int real = std::max(0, reqs[sd]->fighters);
        int l = n > 0 ? lost[sd] * real / n : 0;
        (sd == 0 ? out.attacker_losses : out.defender_losses) = std::clamp(l, 0, real);
    }
    if (const Combatant* p = find_combatant(s, "player"))
        out.player_wounded = !p->wounds.empty() || p->dead;
    out.note = "combat: rounds=" + std::to_string(r.rounds) + ";blows=" + std::to_string(r.log.size()) +
               ";fled=" + std::to_string(r.fled.size()) + ";fallen=" + std::to_string(r.fallen.size()) +
               ";surrendered=" + std::to_string(r.surrendered.size());
    return out;
}

}  // namespace sim
