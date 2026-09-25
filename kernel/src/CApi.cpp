// CApi.cpp — the C ABI implementation (coordinator-owned). Thin: everything
// here delegates to the kernel's own API; no logic lives at the boundary.
//
// No C++ exception may cross into the engine (bug review 2026-09-25): every
// entry point catches everything and returns its documented error value
// (-1 / nullptr / no-op). A throw mid-advance leaves the world as far as it
// got; the handle stays valid.
#include "sim/CApi.h"

#include "sim/World.hpp"
#include "sim/Snapshot.hpp"
#include "sim/Population.hpp"
#include "sim/Schedule.hpp"
#include "sim/Rites.hpp"
#include "sim/CombatActions.hpp"  // W4-B

#include <algorithm>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>

using namespace sim;

struct SimWorld {
    WorldState world;
};

namespace {
// A canon directory is one that holds seasons.csv (WorldState::init builds the
// calendar from it). Db::load skips absent tables, so without this check a
// wrong path yields a valid-but-empty world instead of the documented nullptr.
bool canon_present(const char* canon_dir) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(canon_dir) / "seasons.csv", ec);
}

int write_str(char* out, int cap, const std::string& s) {
    if (out == nullptr || cap <= 0) return -1;
    const int len = static_cast<int>(s.size());
    const int copy = (cap - 1 < len) ? cap - 1 : len;
    std::memcpy(out, s.data(), static_cast<std::size_t>(copy));
    out[copy] = '\0';
    return len;
}
}  // namespace

SimWorld* sim_world_create(const char* canon_dir, uint64_t seed) {
    try {
        if (canon_dir == nullptr || !canon_present(canon_dir)) return nullptr;
        SimWorld* w = new (std::nothrow) SimWorld();
        if (w == nullptr) return nullptr;
        try {
            w->world.init(canon_dir, seed);
        } catch (...) {
            delete w;
            return nullptr;
        }
        return w;
    } catch (...) {
        return nullptr;
    }
}

void sim_world_destroy(SimWorld* world) { delete world; }

void sim_world_advance_days(SimWorld* world, int days) {
    try {
        if (world != nullptr && days > 0) world->world.advance_days(days);
    } catch (...) {
        return;
    }
}

int64_t sim_world_day(const SimWorld* world) {
    try {
        return world ? world->world.day : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_season(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, world->world.cal.season_id(world->world.day));
    } catch (...) {
        return -1;
    }
}

void sim_world_set_drought(SimWorld* world, int stage) {
    try {
        if (world != nullptr) world->world.facts.drought_stage = stage;
    } catch (...) {
        return;
    }
}

int sim_world_drought(const SimWorld* world) {
    try {
        return world ? world->world.facts.drought_stage : -1;
    } catch (...) {
        return -1;
    }
}

void sim_world_set_war(SimWorld* world, int stage) {
    try {
        if (world != nullptr) world->world.facts.war_stage = stage;
    } catch (...) {
        return;
    }
}

int sim_world_war(const SimWorld* world) {
    try {
        return world ? world->world.facts.war_stage : -1;
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_price(const SimWorld* world, const char* city, const char* item) {
    try {
        if (world == nullptr || city == nullptr || item == nullptr) return -1;
        return price_of(world->world.economy, Id(city), Id(item));
    } catch (...) {
        return -1;
    }
}

int sim_world_standing(const SimWorld* world, const char* faction) {
    try {
        if (world == nullptr || faction == nullptr) return -1;
        return standing(world->world.faction, Id(faction));
    } catch (...) {
        return -1;
    }
}

void sim_world_add_standing(SimWorld* world, const char* faction, int delta) {
    try {
        if (world != nullptr && faction != nullptr) add_standing(world->world.faction, Id(faction), delta);
    } catch (...) {
        return;
    }
}

int sim_world_favour(const SimWorld* world, const char* deity) {
    try {
        if (world == nullptr || deity == nullptr) return -1;
        return favour(world->world.magic, Id(deity));
    } catch (...) {
        return -1;
    }
}

void sim_world_add_favour(SimWorld* world, const char* deity, int delta) {
    try {
        if (world != nullptr && deity != nullptr) add_favour(world->world.magic, Id(deity), delta);
    } catch (...) {
        return;
    }
}

int sim_world_verdict_count(const SimWorld* world) {
    try {
        return world ? static_cast<int>(world->world.justice.verdicts.size()) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_event_count(const SimWorld* world) {
    try {
        return world ? static_cast<int>(world->world.events.fired.size()) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_npc_count(const SimWorld* world) {
    try {
        return world ? static_cast<int>(world->world.population.npcs.size()) : -1;
    } catch (...) {
        return -1;
    }
}

// Save / load ----------------------------------------------------------------
int sim_world_save(const SimWorld* world, const char* path) {
    try {
        if (world == nullptr || path == nullptr) return -1;
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return -2;
        f << save_world(world->world);
        return f.good() ? 0 : -2;
    } catch (...) {
        return -1;
    }
}

SimWorld* sim_world_load(const char* canon_dir, const char* path) {
    try {
        if (canon_dir == nullptr || path == nullptr || !canon_present(canon_dir)) return nullptr;
        std::ifstream f(path, std::ios::binary);
        if (!f) return nullptr;
        std::ostringstream ss;
        ss << f.rdbuf();
        SimWorld* w = new (std::nothrow) SimWorld();
        if (w == nullptr) return nullptr;
        try {
            load_world(w->world, canon_dir, ss.str());
        } catch (...) {
            delete w;
            return nullptr;
        }
        return w;
    } catch (...) {
        return nullptr;
    }
}

int sim_world_save_to_buffer(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, save_world(world->world));
    } catch (...) {
        return -1;
    }
}

SimWorld* sim_world_load_from_buffer(const char* canon_dir, const char* data) {
    try {
        if (canon_dir == nullptr || data == nullptr || !canon_present(canon_dir)) return nullptr;
        SimWorld* w = new (std::nothrow) SimWorld();
        if (w == nullptr) return nullptr;
        try {
            load_world(w->world, canon_dir, std::string(data));
        } catch (...) {
            delete w;
            return nullptr;
        }
        return w;
    } catch (...) {
        return nullptr;
    }
}

// Needs ------------------------------------------------------------------
int sim_world_hunger(const SimWorld* world, const char* actor) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        auto it = world->world.needs.by_actor.find(actor);
        return it == world->world.needs.by_actor.end() ? 0 : it->second.hunger;
    } catch (...) {
        return -1;
    }
}

int sim_world_thirst(const SimWorld* world, const char* actor) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        auto it = world->world.needs.by_actor.find(actor);
        return it == world->world.needs.by_actor.end() ? 0 : it->second.thirst;
    } catch (...) {
        return -1;
    }
}

int sim_world_fatigue(const SimWorld* world, const char* actor) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        auto it = world->world.needs.by_actor.find(actor);
        return it == world->world.needs.by_actor.end() ? 0 : it->second.fatigue;
    } catch (...) {
        return -1;
    }
}

void sim_world_advance_needs(SimWorld* world, const char* actor, int hours, int sleeping) {
    try {
        if (world == nullptr || actor == nullptr) return;
        advance_needs(world->world.needs, Id(actor), hours, sleeping != 0);
    } catch (...) {
        return;
    }
}

int sim_world_eat(SimWorld* world, const char* actor, const char* item) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        // Look up, never insert: a refused call must not add an inventory (and
        // so change the save bytes) for an actor the world has never seen.
        const int have = sim_world_item_count(world, actor, item);
        if (have < 0) return -1;  // internal failure, not "none held"
        if (have < 1) return -2;
        std::string reason;
        if (!eat(world->world.db, world->world.needs, Id(actor), Id(item), &reason)) return -3;
        world->world.inventories[actor].counts[item] = have - 1;
        return 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_drink(SimWorld* world, const char* actor, const char* item) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        const bool is_water = std::strcmp(item, "water") == 0;
        int have = 0;
        if (!is_water) {
            have = sim_world_item_count(world, actor, item);  // look up, never insert
            if (have < 0) return -1;  // internal failure, not "none held"
            if (have < 1) return -2;
        }
        std::string reason;
        if (!drink(world->world.db, world->world.needs, Id(actor), Id(item), &reason)) return -3;
        if (!is_water) world->world.inventories[actor].counts[item] = have - 1;
        return 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_need_effects(const SimWorld* world, const char* actor, char* out, int cap) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        auto it = world->world.needs.by_actor.find(actor);
        const Needs n = it == world->world.needs.by_actor.end() ? Needs{} : it->second;
        const std::vector<std::string> effects = need_effects(n);
        std::string joined;
        for (std::size_t i = 0; i < effects.size(); ++i) {
            if (i != 0) joined += ';';
            joined += effects[i];
        }
        return write_str(out, cap, joined);
    } catch (...) {
        return -1;
    }
}

// Inventory ------------------------------------------------------------------
int sim_world_item_count(const SimWorld* world, const char* actor, const char* item) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        auto ait = world->world.inventories.find(actor);
        if (ait == world->world.inventories.end()) return 0;
        auto it = ait->second.counts.find(item);
        return it == ait->second.counts.end() ? 0 : it->second;
    } catch (...) {
        return -1;
    }
}

int sim_world_give_item(SimWorld* world, const char* actor, const char* item, int qty) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        Inventory& inv = world->world.inventories[actor];
        // Widened and saturated: count + qty must not overflow int.
        const long long sum = static_cast<long long>(inv.counts[item]) + qty;
        const int cur = static_cast<int>(std::clamp<long long>(sum, 0, INT_MAX));
        inv.counts[item] = cur;
        return cur;
    } catch (...) {
        return -1;
    }
}

// Crafting ---------------------------------------------------------------
int sim_world_craft(SimWorld* world, const char* actor, const char* recipe,
                     const char* stations_semicolon_list, int times) {
    try {
        if (world == nullptr || actor == nullptr || recipe == nullptr || times <= 0) return -1;
        std::set<Id> stations;
        if (stations_semicolon_list != nullptr) {
            std::stringstream ss{std::string(stations_semicolon_list)};
            std::string entry;
            while (std::getline(ss, entry, ';'))
                if (!entry.empty()) stations.insert(entry);
        }
        // Craft on a copy and write back only on success: a refused craft must
        // not add an empty inventory for an unseen actor (craft() itself leaves
        // the inventory untouched on failure).
        const auto found = world->world.inventories.find(actor);
        Inventory inv = found == world->world.inventories.end() ? Inventory{} : found->second;
        std::string reason;
        if (craft(world->world.db, inv, Id(recipe), stations, times, &reason)) {
            world->world.inventories[actor] = std::move(inv);
            return 0;
        }
        if (reason.rfind("unknown, OPEN or malformed recipe", 0) == 0) return -2;
        if (reason.rfind("station not at hand", 0) == 0) return -3;
        if (reason.rfind("missing input", 0) == 0) return -4;
        return -1;  // times <= 0/out of range guarded above and by check(); anything else falls back here
    } catch (...) {
        return -1;
    }
}

// Schedule ---------------------------------------------------------------
int sim_world_task_at(const SimWorld* world, const char* role, int hour, char* out, int cap) {
    try {
        if (world == nullptr || role == nullptr) return -1;
        const std::optional<ScheduledTask> t =
            task_at(world->world.db, world->world.cal, role, world->world.day, hour);
        if (!t) return -1;
        return write_str(out, cap, t->task);
    } catch (...) {
        return -1;
    }
}

// Rites (K-1) ----------------------------------------------------------------
namespace {
std::string join_ids(const std::vector<Id>& ids) {
    std::string out;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) out += ';';
        out += ids[i];
    }
    return out;
}

int learn_code(const RiteLearnResult& r) {
    if (r.learned) return 0;
    const std::string& why = r.refusal_reason;
    if (why == "already_known") return 1;
    if (why == "unknown_rite") return -2;
    if (why == "unknown_teacher") return -3;
    if (why == "not_taught_by_teacher" || why == "not_taught_in_text") return -4;
    if (why == "text_not_held") return -5;
    return -1;
}
}  // namespace

int sim_world_knows_rite(const SimWorld* world, const char* rite) {
    if (world == nullptr || rite == nullptr) return -1;
    return knows_rite(world->world.magic, Id(rite)) ? 1 : 0;
}

int sim_world_known_rites(const SimWorld* world, char* out, int cap) {
    if (world == nullptr) return -1;
    const std::set<Id>& known = world->world.magic.known_rites;
    return write_str(out, cap, join_ids(std::vector<Id>(known.begin(), known.end())));
}

int sim_world_learn_rite_from_teacher(SimWorld* world, const char* rite, const char* teacher) {
    if (world == nullptr || rite == nullptr || teacher == nullptr) return -1;
    return learn_code(learn_rite_from_teacher(world->world, Id(rite), Id(teacher)));
}

int sim_world_learn_rite_from_text(SimWorld* world, const char* rite, const char* item) {
    if (world == nullptr || rite == nullptr || item == nullptr) return -1;
    return learn_code(learn_rite_from_text(world->world, Id(rite), Id(item)));
}

int sim_world_rites_taught_by(const SimWorld* world, const char* teacher, char* out, int cap) {
    if (world == nullptr || teacher == nullptr) return -1;
    return write_str(out, cap, join_ids(rites_taught_by(world->world, Id(teacher))));
}

int sim_world_rites_taught_in(const SimWorld* world, const char* item, char* out, int cap) {
    if (world == nullptr || item == nullptr) return -1;
    return write_str(out, cap, join_ids(rites_taught_in(world->world.db, Id(item))));
}

void sim_world_set_rite_place(SimWorld* world, const char* place) {
    if (world != nullptr && place != nullptr) world->world.magic.place = place;
}

int sim_world_rite_place(const SimWorld* world, char* out, int cap) {
    if (world == nullptr) return -1;
    return write_str(out, cap, world->world.magic.place);
}

int sim_world_purity(const SimWorld* world) { return world ? world->world.magic.purity : -1; }

void sim_world_set_purity(SimWorld* world, int purity) {
    if (world == nullptr) return;
    world->world.magic.purity = purity < 0 ? 0 : (purity > 100 ? 100 : purity);
}

int sim_world_perform_rite(SimWorld* world, const char* rite, const char* target,
                           char* effect_out, int cap) {
    if (world == nullptr || rite == nullptr) return -1;
    const RiteOutcome o =
        perform_rite_in_world(world->world, Id(rite), target == nullptr ? Id() : Id(target));
    if (effect_out != nullptr && cap > 0) write_str(effect_out, cap, o.effect);
    if (o.rite.performed) return o.rite.succeeded ? 1 : 0;
    const std::string& why = o.refusal_reason;
    if (why == "unknown_rite") return -2;
    if (why == "rite_not_known") return -3;
    if (why == "no_deity_addressed" || why == "no_place_to_ward") return -4;
    if (why == "unknown_deity") return -5;
    return -1;
}

int64_t sim_world_ward_until(const SimWorld* world, const char* place) {
    if (world == nullptr || place == nullptr) return -1;
    const auto& wards = world->world.rite_effects.wards_by_place;
    const auto it = wards.find(place);
    return it == wards.end() ? 0 : it->second.until;
}

int sim_world_warded(const SimWorld* world, const char* place) {
    if (world == nullptr || place == nullptr) return -1;
    return ward_holds(world->world.rite_effects, place, world->world.day) ? 1 : 0;
}

int sim_world_omen_count(const SimWorld* world) {
    return world ? static_cast<int>(world->world.rite_effects.omens.size()) : -1;
}

int sim_world_omen(const SimWorld* world, int index, char* out, int cap) {
    if (world == nullptr) return -1;
    const auto& omens = world->world.rite_effects.omens;
    if (index < 0 || static_cast<std::size_t>(index) >= omens.size()) return -1;
    const Omen& o = omens[static_cast<std::size_t>(index)];
    return write_str(out, cap, std::to_string(o.day) + ";" + o.rite_id + ";" + o.subject + ";" +
                                   o.sign + ";" + std::to_string(o.confidence_pct));
}

// Festivals (K-2) ----------------------------------------------------------
int sim_world_is_festival(const SimWorld* world) {
    if (world == nullptr) return -1;
    return world->world.cal.is_festival(world->world.day) ? 1 : 0;
}

int sim_world_is_festival_day(const SimWorld* world, int64_t day) {
    if (world == nullptr || day < 1) return -1;
    return world->world.cal.is_festival(day) ? 1 : 0;
}

int sim_world_festival(const SimWorld* world, char* out, int cap) {
    if (world == nullptr) return -1;
    return write_str(out, cap, world->world.cal.festival_id(world->world.day));
}

int sim_world_festival_on(const SimWorld* world, int64_t day, char* out, int cap) {
    if (world == nullptr || day < 1) return -1;
    return write_str(out, cap, world->world.cal.festival_id(day));
}

int sim_world_market_open(const SimWorld* world) {
    if (world == nullptr) return -1;
    return market_open(world->world.db, world->world.cal, world->world.day) ? 1 : 0;
}

// People and their day (K-2) -------------------------------------------------
int sim_world_npc_id(const SimWorld* world, int index, char* out, int cap) {
    if (world == nullptr || index < 0) return -1;
    const auto& npcs = world->world.population.npcs;
    if (static_cast<std::size_t>(index) >= npcs.size()) return -1;
    return write_str(out, cap, npcs[static_cast<std::size_t>(index)].id);
}

namespace {
std::optional<ScheduledTask> npc_now(const SimWorld* world, const char* npc, int hour) {
    if (world == nullptr || npc == nullptr) return std::nullopt;
    const WorldState& w = world->world;
    return npc_task_at(w.db, w.cal, w.population, Id(npc), w.day, hour);
}
}  // namespace

int sim_world_npc_task_at(const SimWorld* world, const char* npc, int hour, char* out, int cap) {
    const std::optional<ScheduledTask> t = npc_now(world, npc, hour);
    return t ? write_str(out, cap, t->task) : -1;
}

int sim_world_npc_place_at(const SimWorld* world, const char* npc, int hour, char* out, int cap) {
    const std::optional<ScheduledTask> t = npc_now(world, npc, hour);
    return t ? write_str(out, cap, t->place) : -1;
}

int sim_world_npc_schedule_at(const SimWorld* world, const char* npc, int hour, char* out,
                              int cap) {
    const std::optional<ScheduledTask> t = npc_now(world, npc, hour);
    return t ? write_str(out, cap, t->schedule_id) : -1;
}

// Combat (W4-B) ---------------------------------------------------------------
namespace {
std::vector<Id> w4b_split(const char* list) {
    std::vector<Id> out;
    if (list == nullptr) return out;
    std::stringstream ss{std::string(list)};
    std::string entry;
    while (std::getline(ss, entry, ';')) {
        const std::size_t b = entry.find_first_not_of(" \t");
        if (b == std::string::npos) continue;
        out.push_back(entry.substr(b, entry.find_last_not_of(" \t") - b + 1));
    }
    return out;
}

std::string w4b_join(const std::vector<Id>& ids, char sep) {
    std::string s;
    for (const Id& id : ids) {
        if (!s.empty()) s += sep;
        s += id;
    }
    return s;
}

const Combatant& w4b_body(const SimWorld* world, const char* actor) {
    static const Combatant kFresh{};
    const Combatant* c = find_combatant(world->world.combat, Id(actor));
    return c ? *c : kFresh;
}

const char* w4b_treatment(int t) {
    switch (t) {
        case kTreatBound: return "bound";
        case kTreatHerbs: return "herbs";
        case kTreatHealer: return "healer";
        default: return "none";
    }
}
}  // namespace

int sim_world_equip(SimWorld* world, const char* actor, const char* item) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        return equip_in_world(world->world, Id(actor), Id(item));
    } catch (...) {
        return -1;
    }
}

int sim_world_unequip(SimWorld* world, const char* actor, const char* item) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr) return -1;
        return unequip(world->world.combat, Id(actor), Id(item)) ? 0 : 1;
    } catch (...) {
        return -1;
    }
}

int sim_world_equipped(const SimWorld* world, const char* actor, char* out, int cap) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        return write_str(out, cap, w4b_join(equipped(w4b_body(world, actor)), ';'));
    } catch (...) {
        return -1;
    }
}

int sim_world_attack(SimWorld* world, const char* attacker, const char* defender, int zone_hint,
                     char* out, int cap) {
    try {
        if (world == nullptr || attacker == nullptr || defender == nullptr) return -1;
        const AttackResult r = attack_in_world(world->world, Id(attacker), Id(defender), zone_hint);
        if (out != nullptr && cap > 0) (void)write_str(out, cap, r.text);
        return r.outcome == AttackOutcome::Invalid ? -2 : static_cast<int>(r.outcome);
    } catch (...) {
        return -1;
    }
}

int sim_world_health(const SimWorld* world, const char* actor) {
    try {
        return (world && actor) ? w4b_body(world, actor).health : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_stamina(const SimWorld* world, const char* actor) {
    try {
        return (world && actor) ? w4b_body(world, actor).stamina : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_morale(const SimWorld* world, const char* actor) {
    try {
        return (world && actor) ? w4b_body(world, actor).morale : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_wound(const SimWorld* world, const char* actor, int zone) {
    try {
        if (world == nullptr || actor == nullptr || zone < 0 || zone >= kZoneCount) return -1;
        return open_damage(w4b_body(world, actor), zone);
    } catch (...) {
        return -1;
    }
}

int sim_world_bleeding(const SimWorld* world, const char* actor) {
    try {
        return (world && actor) ? bleeding(w4b_body(world, actor)) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_is_dead(const SimWorld* world, const char* actor) {
    try {
        return (world && actor) ? (w4b_body(world, actor).dead ? 1 : 0) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_combat_status(const SimWorld* world, const char* actor, char* out, int cap) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        return write_str(out, cap, w4b_join(combat_effects(w4b_body(world, actor)), ';'));
    } catch (...) {
        return -1;
    }
}

int sim_world_wounds(const SimWorld* world, const char* actor, char* out, int cap) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        std::vector<Id> parts;
        for (const Wound& w : w4b_body(world, actor).wounds)
            if (w.remaining > 0)
                parts.push_back(std::string(zone_name(w.zone)) + ":" + w.type + ":" +
                                severity_name(w.severity) + ":" + std::to_string(w.remaining) + ":" +
                                std::to_string(w.bleed) + ":" + w4b_treatment(w.treatment));
        return write_str(out, cap, w4b_join(parts, '|'));
    } catch (...) {
        return -1;
    }
}

int sim_world_rest(SimWorld* world, const char* actor, int hours) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        return rest_in_world(world->world, Id(actor), hours) ? 1 : 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_combat_advance(SimWorld* world, const char* actor, int hours) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        return advance_body_hours(world->world, Id(actor), hours, false) ? 1 : 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_catch_breath(SimWorld* world, const char* actor, int rounds) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        (void)combatant_of(world->world.combat, Id(actor));
        catch_breath(world->world.combat, Id(actor), rounds);
        return w4b_body(world, actor).stamina;
    } catch (...) {
        return -1;
    }
}

int sim_world_treat_wounds(SimWorld* world, const char* actor, const char* method,
                           const char* healer) {
    try {
        if (world == nullptr || actor == nullptr || method == nullptr) return -1;
        return treat_in_world(world->world, Id(actor), std::string(method), healer ? Id(healer) : Id());
    } catch (...) {
        return -1;
    }
}

int sim_world_stance(const SimWorld* world, const char* actor, int allies, int enemies, char* out,
                     int cap) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        const Stance st = choose_stance(w4b_body(world, actor),
                                        combat_inputs_for(world->world, Id(actor)), allies, enemies);
        if (out != nullptr && cap > 0) (void)write_str(out, cap, stance_name(st));
        return st == Stance::None ? 3 : static_cast<int>(st);
    } catch (...) {
        return -1;
    }
}

int sim_world_apply_combat_style(SimWorld* world, const char* actor, const char* style) {
    try {
        if (world == nullptr || actor == nullptr || style == nullptr) return -1;
        return apply_style_in_world(world->world, Id(actor), Id(style)) ? 0 : -2;
    } catch (...) {
        return -1;
    }
}

int sim_world_combat_style_of(const SimWorld* world, const char* npc, char* out, int cap) {
    try {
        if (world == nullptr || npc == nullptr) return -1;
        return write_str(out, cap, style_of_npc(world->world, Id(npc)));
    } catch (...) {
        return -1;
    }
}

int sim_world_surrender(SimWorld* world, const char* who, const char* to) {
    try {
        if (world == nullptr || who == nullptr || to == nullptr) return -1;
        return surrender(world->world.combat, Id(who), Id(to)) ? 0 : -2;
    } catch (...) {
        return -1;
    }
}

int sim_world_take_prisoner(SimWorld* world, const char* captor, const char* captive) {
    try {
        if (world == nullptr || captor == nullptr || captive == nullptr) return -1;
        return take_prisoner(world->world.combat, Id(captor), Id(captive), world->world.day) ? 0 : -2;
    } catch (...) {
        return -1;
    }
}

int sim_world_release_prisoner(SimWorld* world, const char* captive) {
    try {
        if (world == nullptr || captive == nullptr) return -1;
        return release_prisoner(world->world.combat, Id(captive)) ? 0 : -2;
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_ransom(SimWorld* world, const char* captive, char* loan_out, int cap) {
    try {
        if (world == nullptr || captive == nullptr) return -1;
        Id loan;
        const Silver paid = ransom_prisoner(world->world, Id(captive), &loan);
        if (loan_out != nullptr && cap > 0) (void)write_str(loan_out, cap, paid < 0 ? Id() : loan);
        return paid;
    } catch (...) {
        return -1;
    }
}

int sim_world_loot(SimWorld* world, const char* looter, const char* body) {
    try {
        if (world == nullptr || looter == nullptr || body == nullptr) return -1;
        const int moved = loot_body(world->world, Id(looter), Id(body));
        return moved < 0 ? -2 : moved;
    } catch (...) {
        return -1;
    }
}

int sim_world_agree_duel(SimWorld* world, const char* a, const char* b) {
    try {
        if (world == nullptr || a == nullptr || b == nullptr) return -1;
        agree_duel(world->world.combat, Id(a), Id(b), world->world.day);
        return 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_set_outlaw(SimWorld* world, const char* actor, int flag) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        set_outlaw(world->world.combat, Id(actor), flag != 0);
        return 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_combat_crime(SimWorld* world, const char* attacker, const char* victim,
                           const char* place_city, const char* witnesses, char* out, int cap) {
    try {
        if (world == nullptr || attacker == nullptr || victim == nullptr) return -1;
        Id crime;
        const Id law = file_combat_crime(world->world, Id(attacker), Id(victim),
                                         place_city ? Id(place_city) : Id(), w4b_split(witnesses), &crime);
        if (out != nullptr && cap > 0) (void)write_str(out, cap, law.empty() ? Id() : law + ";" + crime);
        return law.empty() ? 0 : 1;
    } catch (...) {
        return -1;
    }
}

int sim_world_repair(SimWorld* world, const char* actor, const char* item, const char* smith) {
    try {
        if (world == nullptr || actor == nullptr || item == nullptr || smith == nullptr) return -1;
        return repair_at_smith(world->world, Id(actor), Id(item), Id(smith));
    } catch (...) {
        return -1;
    }
}

int sim_world_recast(SimWorld* world, const char* actor, const char* from, const char* into,
                     const char* smith) {
    try {
        if (world == nullptr || actor == nullptr || from == nullptr || into == nullptr || smith == nullptr)
            return -1;
        return recast_at_smith(world->world, Id(actor), Id(from), Id(into), Id(smith));
    } catch (...) {
        return -1;
    }
}

int sim_world_skirmish(SimWorld* world, const char* side_a, const char* side_b, int max_rounds,
                       char* out, int cap) {
    try {
        if (world == nullptr || side_a == nullptr || side_b == nullptr) return -1;
        const SkirmishResult r =
            skirmish_in_world(world->world, w4b_split(side_a), w4b_split(side_b), max_rounds);
        if (out != nullptr && cap > 0)
            (void)write_str(out, cap,
                            "rounds=" + std::to_string(r.rounds) + ";blows=" + std::to_string(r.log.size()) +
                                ";fled=" + w4b_join(r.fled, ',') + ";surrendered=" +
                                w4b_join(r.surrendered, ',') + ";fallen=" + w4b_join(r.fallen, ','));
        return r.winner < 0 ? 2 : r.winner;
    } catch (...) {
        return -1;
    }
}

int sim_world_death_count(const SimWorld* world) {
    try {
        return world ? static_cast<int>(world->world.combat.deaths.size()) : -1;
    } catch (...) {
        return -1;
    }
}
