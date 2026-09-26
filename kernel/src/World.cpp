// World.cpp — Wave 2 integration: the deterministic daily tick (coordinator).
// Tick order is fixed in Context.hpp; every module reads the other states as of
// the same morning and writes only its own.
#include "sim/World.hpp"

#include <algorithm>
#include <map>

#include "sim/Actions.hpp"
#include "sim/Progression.hpp"  // W4-A
#include "sim/WildActions.hpp"  // W4-C
#include "sim/CombatActions.hpp"  // W4-B

#include <cstdlib>

namespace sim {

void WorldState::init(const std::string& canon_dir, std::uint64_t world_seed) {
    db = Db::load(canon_dir);
    seed = world_seed;
    rng = Rng(world_seed);
    // The calendar's shape is ratified machinery; its seasons are canon data
    // (seasons.csv, D-015); its yearly festivals are festivals.csv (K-2 — the
    // machine-readable form of calendar.csv's INVENTED festival_days row,
    // D-018). Month names are months.csv; named days of the month are calendar_days.csv (A14).
    CalendarConfig cfg;
    for (const Row& r : db.rows("seasons")) {
        if (r.get("tag") == "OPEN") continue;
        SeasonDef s;
        s.id = r.at("id");
        s.start_day_of_year = std::strtol(r.get("start_day_of_year", "1").c_str(), nullptr, 10);
        if (s.start_day_of_year >= 1) cfg.seasons.push_back(s);
    }
    for (const Row& r : db.rows("festivals")) {
        if (r.get("tag") == "OPEN") continue;  // an unwritten festival never lands on the calendar
        FestivalDef f;
        f.id = r.at("id");
        f.name = r.get("name");
        f.day_of_year = std::strtol(r.get("day_of_year", "0").c_str(), nullptr, 10);
        cfg.festivals.push_back(f);  // Calendar validates range + one-per-day
    }
    // months.csv: all 12 non-OPEN rows by number, or none (a partial list
    // would misname the year, so the months then stay unnamed numbers).
    {
        std::vector<std::string> names(static_cast<std::size_t>(cfg.months_per_year));
        int named = 0;
        for (const Row& r : db.rows("months")) {
            if (r.get("tag") == "OPEN") continue;
            const long n = std::strtol(r.get("number", "0").c_str(), nullptr, 10);
            if (n >= 1 && n <= cfg.months_per_year && names[static_cast<std::size_t>(n - 1)].empty()) {
                names[static_cast<std::size_t>(n - 1)] = r.get("name");
                ++named;
            }
        }
        if (named == cfg.months_per_year) cfg.month_names = names;
    }
    for (const Row& r : db.rows("calendar_days")) {
        if (r.get("tag") == "OPEN") continue;
        CalendarDayDef d;
        d.id = r.at("id");
        d.name = r.get("name");
        d.day_of_month = static_cast<int>(std::strtol(r.get("day_of_month", "0").c_str(), nullptr, 10));
        d.omen = r.get("omen");
        d.deity = r.get("deity");
        cfg.month_days.push_back(d);  // Calendar validates range + one-per-day
    }
    cal = Calendar{cfg};
    facts = WorldFacts{};
    day = 1;

    economy = EconomyState{};
    population = PopulationState{};
    faction = FactionState{};
    magic = MagicState{};
    justice = JusticeState{};
    events = EventsState{};
    property = PropertyState{};
    quests = QuestState{};

    needs = NeedsState{};
    inventories.clear();
    inventories["player"] = Inventory{};  // the prisoner start (D-009): empty-handed
    rite_effects = RiteEffectsState{};    // K-1: no ward laid, no omen read
    combat = CombatState{};               // W4-B: nobody hurt yet
    divine = DivineState{};               // W5: the gods hold no grudge yet
    world_items = WorldItemsState{};      // P0a: nothing lies on the ground yet
    notices = NoticeState{};              // P0a: nothing to tell the player yet

    WorldContext ctx = context();
    for (const Row& city : db.rows("cities"))
        if (city.get("alias_of").empty())  // an alias is the same city: one market
            seed_market(ctx, economy, city.at("id"));
    seed_people(ctx, population);
    load_rules(ctx, events);  // events.csv is canon-empty at Wave 1: zero rules
    load_defs(ctx, quests);   // quests.csv likewise

    // W4-A: the progression catalog from canon, a fresh prisoner's sheet
    // (every attribute 5, no skill, level 1), and every resident's light
    // sheet seeded from its role.
    progression = load_progression(db);
    character = new_character(progression);
    seed_npc_sheets(*this);
    // --- W4-C: the wild lands (static tables + day-1 camps, herds, sites).
    init_wild(db, wild);
    // --- end W4-C
    // W4-B: real combat behind every fight in the wild (the W4-C seam).
    wild.skirmish = &combat_skirmish_adapter;
}

namespace {

// FND-09: what the day's ticks did that the player must hear about, found by
// comparing the world before and after the day (so no module file changes).
struct NoticeMarks {
    int next_raid = 0;
    std::size_t failed_quests = 0;
    std::size_t verdicts = 0;
    std::map<Id, Id> player_crimes;  // open crime id -> law row
};

NoticeMarks mark(const WorldState& w) {
    NoticeMarks m;
    m.next_raid = w.wild.next_raid;
    m.failed_quests = w.quests.failed_list.size();
    m.verdicts = w.justice.verdicts.size();
    for (const Crime& c : w.justice.open_crimes)
        if (c.criminal == "player") m.player_crimes[c.id] = c.law_row;
    return m;
}

void post_notices(WorldState& w, const NoticeMarks& m, DayNumber day) {
    const int new_raids = w.wild.next_raid - m.next_raid;
    const std::size_t logged = w.wild.raids.size();
    const std::size_t from = new_raids <= 0 ? logged
                             : logged - std::min<std::size_t>(logged, static_cast<std::size_t>(new_raids));
    for (std::size_t i = from; i < logged; ++i) push_notice(w.notices, day, "notice.raid", w.wild.raids[i].summary);
    for (std::size_t i = m.failed_quests; i < w.quests.failed_list.size(); ++i)
        push_notice(w.notices, day, "notice.quest_failed", w.quests.failed_list[i]);
    for (std::size_t i = m.verdicts; i < w.justice.verdicts.size(); ++i) {
        const Hearing& h = w.justice.verdicts[i];
        const auto c = m.player_crimes.find(h.crime_id);
        if (c != m.player_crimes.end()) push_notice(w.notices, day, "notice.verdict", c->second + ":" + h.verdict);
    }
}

}  // namespace

void WorldState::advance_days(int days) {
    for (int i = 0; i < days; ++i) {
        const NoticeMarks marks = mark(*this);
        const WorldContext ctx = context();
        tick_economy(ctx, economy, 1);
        tick_population(ctx, population, 1);
        tick_faction(ctx, faction, 1);
        // Magic has no tick: rites are performed by the engine layer through
        // perform_rite; favour only moves when someone performs.
        tick_justice(ctx, justice, 1);
        tick_events(ctx, events, 1);
        tick_property(ctx, property, 1);
        tick_quests(ctx, quests, 1);
        // World-orchestrated hearings (commit_crime) need several modules'
        // state, so they run here, after the module ticks, on the same day.
        hold_due_hearings(*this);
        // --- W4-C: the wild's day (weather, caravans, camps, the raid formula).
        tick_wild(*this);
        // --- end W4-C
        // W4-B: wounds heal (or bleed out) once a day; deaths reach
        // Population and Events through the caller layer.
        tick_combat_world(*this);
        // W5: the gods' morning — banked favour penalties land on the
        // performer, wrath decays (a curse holds; only atonement lifts it).
        tick_divine(*this);
        post_notices(*this, marks, day);
        ++day;
    }
}

WorldContext WorldState::context() {
    return WorldContext{db,  rng,      day,      cal,      facts,
                        economy, population, faction, magic,
                        justice, events,     property, quests,
                        needs, inventories};
}

}  // namespace sim
