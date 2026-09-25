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
#include "sim/Progression.hpp"  // W4-A

#include <algorithm>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>

using namespace sim;

struct SimWorld {
    WorldState world;
    std::string w4a_refusal;  // W4-A: why the last progression verb refused ("" = it didn't)
};

namespace {
// A canon directory is one that holds seasons.csv (WorldState::init builds the
// calendar from it). Db::load skips absent tables, so without this check a
// wrong path yields a valid-but-empty world instead of the documented nullptr.
// W4-A: hunger shows (Needs' own "hungry"/"starving" effects), before a meal.
bool hungry(const WorldState& w, const Id& actor) {
    const auto it = w.needs.by_actor.find(actor);
    if (it == w.needs.by_actor.end()) return false;
    for (const std::string& e : need_effects(it->second))
        if (e == "hungry" || e == "starving") return true;
    return false;
}

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

// Never throws. ~WorldState is implicitly noexcept, so a throw inside it would
// terminate before reaching this catch; the guard keeps the uniform pattern
// and covers anything added around the delete later.
void sim_world_destroy(SimWorld* world) {
    try {
        delete world;
    } catch (...) {
        return;
    }
}

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
        const bool was_hungry = hungry(world->world, Id(actor));  // W4-A
        if (!eat(world->world.db, world->world.needs, Id(actor), Id(item), &reason)) return -3;
        world->world.inventories[actor].counts[item] = have - 1;
        // W4-A: a hungry meal is use of survival (rationing, knowing what is safe to eat).
        if (was_hungry) (void)note_use(world->world, Id(actor), "verb:eat", kEatXp);
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
            (void)note_craft(world->world, Id(actor), Id(recipe), times);  // W4-A: craft skills by use
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
    try {
        if (world == nullptr || rite == nullptr) return -1;
        return knows_rite(world->world.magic, Id(rite)) ? 1 : 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_known_rites(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        const std::set<Id>& known = world->world.magic.known_rites;
        return write_str(out, cap, join_ids(std::vector<Id>(known.begin(), known.end())));
    } catch (...) {
        return -1;
    }
}

int sim_world_learn_rite_from_teacher(SimWorld* world, const char* rite, const char* teacher) {
    try {
        if (world == nullptr || rite == nullptr || teacher == nullptr) return -1;
        return learn_code(learn_rite_from_teacher(world->world, Id(rite), Id(teacher)));
    } catch (...) {
        return -1;
    }
}

int sim_world_learn_rite_from_text(SimWorld* world, const char* rite, const char* item) {
    try {
        if (world == nullptr || rite == nullptr || item == nullptr) return -1;
        return learn_code(learn_rite_from_text(world->world, Id(rite), Id(item)));
    } catch (...) {
        return -1;
    }
}

int sim_world_rites_taught_by(const SimWorld* world, const char* teacher, char* out, int cap) {
    try {
        if (world == nullptr || teacher == nullptr) return -1;
        return write_str(out, cap, join_ids(rites_taught_by(world->world, Id(teacher))));
    } catch (...) {
        return -1;
    }
}

int sim_world_rites_taught_in(const SimWorld* world, const char* item, char* out, int cap) {
    try {
        if (world == nullptr || item == nullptr) return -1;
        return write_str(out, cap, join_ids(rites_taught_in(world->world.db, Id(item))));
    } catch (...) {
        return -1;
    }
}

void sim_world_set_rite_place(SimWorld* world, const char* place) {
    try {
        if (world != nullptr && place != nullptr) world->world.magic.place = place;
    } catch (...) {
        return;
    }
}

int sim_world_rite_place(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, world->world.magic.place);
    } catch (...) {
        return -1;
    }
}

int sim_world_purity(const SimWorld* world) {
    try {
        return world ? world->world.magic.purity : -1;
    } catch (...) {
        return -1;
    }
}

void sim_world_set_purity(SimWorld* world, int purity) {
    try {
        if (world == nullptr) return;
        world->world.magic.purity = purity < 0 ? 0 : (purity > 100 ? 100 : purity);
    } catch (...) {
        return;
    }
}

int sim_world_perform_rite(SimWorld* world, const char* rite, const char* target,
                           char* effect_out, int cap) {
    try {
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
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_ward_until(const SimWorld* world, const char* place) {
    try {
        if (world == nullptr || place == nullptr) return -1;
        const auto& wards = world->world.rite_effects.wards_by_place;
        const auto it = wards.find(place);
        return it == wards.end() ? 0 : it->second.until;
    } catch (...) {
        return -1;
    }
}

int sim_world_warded(const SimWorld* world, const char* place) {
    try {
        if (world == nullptr || place == nullptr) return -1;
        return ward_holds(world->world.rite_effects, place, world->world.day) ? 1 : 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_omen_count(const SimWorld* world) {
    try {
        return world ? static_cast<int>(world->world.rite_effects.omens.size()) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_omen(const SimWorld* world, int index, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        const auto& omens = world->world.rite_effects.omens;
        if (index < 0 || static_cast<std::size_t>(index) >= omens.size()) return -1;
        const Omen& o = omens[static_cast<std::size_t>(index)];
        return write_str(out, cap, std::to_string(o.day) + ";" + o.rite_id + ";" + o.subject + ";" +
                                       o.sign + ";" + std::to_string(o.confidence_pct));
    } catch (...) {
        return -1;
    }
}

// Festivals (K-2) ----------------------------------------------------------
int sim_world_is_festival(const SimWorld* world) {
    try {
        if (world == nullptr) return -1;
        return world->world.cal.is_festival(world->world.day) ? 1 : 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_is_festival_day(const SimWorld* world, int64_t day) {
    try {
        if (world == nullptr || day < 1) return -1;
        return world->world.cal.is_festival(day) ? 1 : 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_festival(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, world->world.cal.festival_id(world->world.day));
    } catch (...) {
        return -1;
    }
}

int sim_world_festival_on(const SimWorld* world, int64_t day, char* out, int cap) {
    try {
        if (world == nullptr || day < 1) return -1;
        return write_str(out, cap, world->world.cal.festival_id(day));
    } catch (...) {
        return -1;
    }
}

int sim_world_market_open(const SimWorld* world) {
    try {
        if (world == nullptr) return -1;
        return market_open(world->world.db, world->world.cal, world->world.day) ? 1 : 0;
    } catch (...) {
        return -1;
    }
}

// People and their day (K-2) -------------------------------------------------
int sim_world_npc_id(const SimWorld* world, int index, char* out, int cap) {
    try {
        if (world == nullptr || index < 0) return -1;
        const auto& npcs = world->world.population.npcs;
        if (static_cast<std::size_t>(index) >= npcs.size()) return -1;
        return write_str(out, cap, npcs[static_cast<std::size_t>(index)].id);
    } catch (...) {
        return -1;
    }
}

namespace {
// May throw (Id construction, the schedule lookup): every caller guards it.
std::optional<ScheduledTask> npc_now(const SimWorld* world, const char* npc, int hour) {
    if (world == nullptr || npc == nullptr) return std::nullopt;
    const WorldState& w = world->world;
    return npc_task_at(w.db, w.cal, w.population, Id(npc), w.day, hour);
}
}  // namespace

int sim_world_npc_task_at(const SimWorld* world, const char* npc, int hour, char* out, int cap) {
    try {
        const std::optional<ScheduledTask> t = npc_now(world, npc, hour);
        return t ? write_str(out, cap, t->task) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_npc_place_at(const SimWorld* world, const char* npc, int hour, char* out, int cap) {
    try {
        const std::optional<ScheduledTask> t = npc_now(world, npc, hour);
        return t ? write_str(out, cap, t->place) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_npc_schedule_at(const SimWorld* world, const char* npc, int hour, char* out,
                              int cap) {
    try {
        const std::optional<ScheduledTask> t = npc_now(world, npc, hour);
        return t ? write_str(out, cap, t->schedule_id) : -1;
    } catch (...) {
        return -1;
    }
}

// Character progression (W4-A) -------------------------------------------------
namespace {
int w4a_code(const std::string& why) {
    static const std::map<std::string, int> kCodes = {
        {"unknown_skill", -2},        {"unknown_talent", -2},        {"unknown_calling", -2},
        {"unknown_teacher", -2},      {"unknown_employer", -2},      {"unknown_market", -2},
        {"unknown_polity", -2},       {"unknown_patron", -2},        {"level_too_low", -3},
        {"skill_too_low", -3},        {"calling_required", -3},      {"cannot_read", -3},
        {"standing_too_low", -3},     {"no_calling", -3},            {"outlawed", -3},
        {"practice_capped", -4},      {"teacher_surpassed", -4},     {"text_exhausted", -4},
        {"already_taken", -4},        {"already_chosen", -4},        {"at_highest_rank", -4},
        {"same_calling", -4},         {"cannot_afford", -5},         {"not_held", -5},
        {"text_not_held", -5},        {"out_of_stock", -5},          {"no_talent_point", -5},
        {"exhausted", -6},            {"not_taught_by_teacher", -7}, {"not_taught_in_text", -7},
        {"not_an_employer", -7},      {"not_sold_here", -7},         {"market_closed", -7},
        {"not_a_calling", -7},        {"not_a_specialisation", -7},  {"not_of_your_calling", -7},
        {"patron_not_of_polity", -7}, {"bad_hours", -7},             {"bad_quantity", -7},
    };
    const auto it = kCodes.find(why);
    return it == kCodes.end() ? -1 : it->second;
}

// Records the refusal (or clears it) and returns the verb's C result.
int choice_result(SimWorld* world, const std::string& why) {
    world->w4a_refusal = why;
    return why.empty() ? 0 : w4a_code(why);
}

int progress_result(SimWorld* world, const ProgressResult& r) {
    world->w4a_refusal = r.refusal;
    return r.ok ? r.gain.points : w4a_code(r.refusal);
}

int read_code(int v) { return v < 0 ? -2 : v; }  // unknown actor/attribute/skill -> -2

std::string join_teachings(const std::vector<Teaching>& ts, bool by_source) {
    std::string out;
    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (i != 0) out += ';';
        out += (by_source ? ts[i].source : ts[i].skill) + ":" + std::to_string(ts[i].max_level) + ":" +
               std::to_string(ts[i].fee_per_hour);
    }
    return out;
}
}  // namespace

int sim_world_attribute(const SimWorld* world, const char* actor, const char* attr) {
    try {
        if (world == nullptr || actor == nullptr || attr == nullptr) return -1;
        return read_code(actor_attribute(world->world, Id(actor), Id(attr)));
    } catch (...) {
        return -1;
    }
}

int sim_world_skill(const SimWorld* world, const char* actor, const char* skill) {
    try {
        if (world == nullptr || actor == nullptr || skill == nullptr) return -1;
        return read_code(actor_skill(world->world, Id(actor), Id(skill)));
    } catch (...) {
        return -1;
    }
}

int sim_world_effective_skill(const SimWorld* world, const char* actor, const char* skill) {
    try {
        if (world == nullptr || actor == nullptr || skill == nullptr) return -1;
        return read_code(actor_effective_skill(world->world, Id(actor), Id(skill)));
    } catch (...) {
        return -1;
    }
}

int sim_world_level(const SimWorld* world) { return world ? world->world.character.level : -1; }

int sim_world_character_xp(const SimWorld* world) { return world ? world->world.character.xp : -1; }

int sim_world_xp_for_next_level(const SimWorld* world) {
    if (world == nullptr) return -1;
    const int level = world->world.character.level;
    return level >= kLevelCap ? 0 : xp_for_level(level + 1);
}

int sim_world_talent_points(const SimWorld* world) {
    return world ? world->world.character.talent_points : -1;
}

int sim_world_talents(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, join_ids(world->world.character.talents));
    } catch (...) {
        return -1;
    }
}

int sim_world_talents_available(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap,
                         join_ids(available_talents(world->world.progression, world->world.character)));
    } catch (...) {
        return -1;
    }
}

int sim_world_choose_talent(SimWorld* world, const char* talent) {
    try {
        if (world == nullptr || talent == nullptr) return -1;
        return choice_result(world,
                             choose_talent(world->world.progression, world->world.character, Id(talent)));
    } catch (...) {
        return -1;
    }
}

int sim_world_calling(const SimWorld* world, int slot, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        const CharacterState& c = world->world.character;
        if (slot == 0) return write_str(out, cap, c.calling);
        if (slot == 1) return write_str(out, cap, c.specialisation);
        if (slot == 2) return write_str(out, cap, c.second_calling);
        return -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_choose_calling(SimWorld* world, const char* calling) {
    try {
        if (world == nullptr || calling == nullptr) return -1;
        return choice_result(world,
                             choose_calling(world->world.progression, world->world.character, Id(calling)));
    } catch (...) {
        return -1;
    }
}

int sim_world_choose_specialisation(SimWorld* world, const char* specialisation) {
    try {
        if (world == nullptr || specialisation == nullptr) return -1;
        return choice_result(world, choose_specialisation(world->world.progression, world->world.character,
                                                          Id(specialisation)));
    } catch (...) {
        return -1;
    }
}

int sim_world_choose_second_calling(SimWorld* world, const char* calling) {
    try {
        if (world == nullptr || calling == nullptr) return -1;
        return choice_result(world, choose_second_calling(world->world.progression, world->world.character,
                                                          Id(calling)));
    } catch (...) {
        return -1;
    }
}

int sim_world_practice_skill(SimWorld* world, const char* skill, int hours) {
    try {
        if (world == nullptr || skill == nullptr) return -1;
        return progress_result(world, practice_skill(world->world, Id(skill), hours));
    } catch (...) {
        return -1;
    }
}

int sim_world_train_skill(SimWorld* world, const char* skill, const char* teacher, int hours) {
    try {
        if (world == nullptr || skill == nullptr || teacher == nullptr) return -1;
        return progress_result(world, train_with_teacher(world->world, Id(skill), Id(teacher), hours));
    } catch (...) {
        return -1;
    }
}

int sim_world_study_skill(SimWorld* world, const char* skill, const char* item, int hours) {
    try {
        if (world == nullptr || skill == nullptr || item == nullptr) return -1;
        return progress_result(world, study_text(world->world, Id(skill), Id(item), hours));
    } catch (...) {
        return -1;
    }
}

int sim_world_work(SimWorld* world, const char* employer, int hours) {
    try {
        if (world == nullptr || employer == nullptr) return -1;
        return progress_result(world, work_for(world->world, Id(employer), hours));
    } catch (...) {
        return -1;
    }
}

int sim_world_skill_teachers(const SimWorld* world, const char* skill, char* out, int cap) {
    try {
        if (world == nullptr || skill == nullptr) return -1;
        return write_str(out, cap, join_teachings(teachers_of(world->world, Id(skill)), true));
    } catch (...) {
        return -1;
    }
}

int sim_world_skills_taught_by(const SimWorld* world, const char* npc, char* out, int cap) {
    try {
        if (world == nullptr || npc == nullptr) return -1;
        return write_str(out, cap, join_teachings(skills_taught_by(world->world, Id(npc)), false));
    } catch (...) {
        return -1;
    }
}

int sim_world_skill_ids(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        std::vector<Id> ids;
        for (const auto& [id, def] : world->world.progression.skills) ids.push_back(id);
        return write_str(out, cap, join_ids(ids));
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_purse(const SimWorld* world, const char* actor) {
    try {
        if (world == nullptr || actor == nullptr) return -1;
        return purse(world->world.property, Id(actor));
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_buy(SimWorld* world, const char* actor, const char* city, const char* item, int qty) {
    try {
        if (world == nullptr || actor == nullptr || city == nullptr || item == nullptr) return -1;
        const ProgressResult r = buy_from_market(world->world, Id(actor), Id(city), Id(item), qty);
        world->w4a_refusal = r.refusal;
        return r.ok ? r.silver : w4a_code(r.refusal);
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_sell(SimWorld* world, const char* actor, const char* city, const char* item, int qty) {
    try {
        if (world == nullptr || actor == nullptr || city == nullptr || item == nullptr) return -1;
        const ProgressResult r = sell_to_market(world->world, Id(actor), Id(city), Id(item), qty);
        world->w4a_refusal = r.refusal;
        return r.ok ? r.silver : w4a_code(r.refusal);
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_buy_quote(const SimWorld* world, const char* actor, const char* city,
                            const char* item, int qty) {
    try {
        if (world == nullptr || actor == nullptr || city == nullptr || item == nullptr) return -1;
        const Silver q = quote_buy(world->world, Id(actor), Id(city), Id(item), qty);
        return q < 0 ? -7 : q;
    } catch (...) {
        return -1;
    }
}

int64_t sim_world_sell_quote(const SimWorld* world, const char* actor, const char* city,
                             const char* item, int qty) {
    try {
        if (world == nullptr || actor == nullptr || city == nullptr || item == nullptr) return -1;
        const Silver q = quote_sell(world->world, Id(actor), Id(city), Id(item), qty);
        return q < 0 ? -7 : q;
    } catch (...) {
        return -1;
    }
}

int sim_world_rank(const SimWorld* world, const char* polity) {
    try {
        if (world == nullptr || polity == nullptr) return -1;
        return rank(world->world.character, Id(polity));
    } catch (...) {
        return -1;
    }
}

int sim_world_raise_rank(SimWorld* world, const char* polity, const char* patron) {
    try {
        if (world == nullptr || polity == nullptr) return -1;
        const ProgressResult r =
            raise_rank(world->world, Id(polity), patron == nullptr ? Id() : Id(patron));
        world->w4a_refusal = r.refusal;
        return r.ok ? rank(world->world.character, Id(polity)) : w4a_code(r.refusal);
    } catch (...) {
        return -1;
    }
}

int sim_world_character_summary(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, character_summary(world->world.progression, world->world.character));
    } catch (...) {
        return -1;
    }
}

int sim_world_progression_refusal(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, world->w4a_refusal);
    } catch (...) {
        return -1;
    }
}
