// CApiQuests.cpp — A7: quests, the journal and dialogue through the C API.
#include "sim/CApiQuests.h"

#include "sim/Actions.hpp"
#include "sim/Dialogue.hpp"
#include "sim/Population.hpp"
#include "sim/Quests.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "CApiInternal.hpp"

using namespace sim;

namespace {

int write_req(char* out, int cap, const std::string& s) {
    if (out != nullptr && cap > 0) {
        const int n = std::min<int>(cap - 1, static_cast<int>(s.size()));
        std::memcpy(out, s.data(), static_cast<std::size_t>(n));
        out[n] = '\0';
    }
    return static_cast<int>(s.size());
}

template <class F>
int guard(F&& f) {
    try {
        return f();
    } catch (...) {
        return -1;
    }
}

bool contains(const std::vector<Id>& v, const Id& id) { return std::find(v.begin(), v.end(), id) != v.end(); }

const Row* quest_row(const WorldState& w, const Id& id) {
    for (const Row& r : w.db.rows("quests"))
        if (r.get("id") == id) return &r;
    return nullptr;
}

const QuestDef* find_def(const WorldState& w, const Id& id) {
    for (const QuestDef& d : w.quests.defs)
        if (d.id == id) return &d;
    return nullptr;
}

bool known_def(const WorldState& w, const Id& id) { return find_def(w, id) != nullptr; }

// A quest the world drives itself: a def with no quests.csv row (WildWorld's
// rescue_<npc>). The kernel creates, advances and pays it; the player's verbs
// never do (-4). Every canon row, of any kind, stays the player's (D-021).
bool world_driven(const WorldState& w, const Id& id) { return known_def(w, id) && quest_row(w, id) == nullptr; }

// The title of a world-driven quest, generated from what the kernel knows.
std::string generated_title(const WorldState& w, const Id& id) {
    const std::string prefix = "rescue_";
    if (id.rfind(prefix, 0) == 0) {
        const Id npc = id.substr(prefix.size());
        for (const Npc& n : w.population.npcs)
            if (n.id == npc) return "Rescue " + n.name;
    }
    return id;
}

// The ids in one named list, in stable order (defs order for "available").
bool list_ids(const WorldState& w, const std::string& list, std::vector<Id>& out) {
    const QuestState& q = w.quests;
    if (list == "active") {
        for (const Quest& a : q.active) out.push_back(a.def_id);
        return true;
    }
    if (list == "completed") { out = q.completed; return true; }
    if (list == "failed") { out = q.failed_list; return true; }
    if (list == "available") {
        for (const QuestDef& d : q.defs)
            if (!find_active(q, d.id) && !contains(q.completed, d.id) && !contains(q.failed_list, d.id) &&
                !world_driven(w, d.id))
                out.push_back(d.id);
        return true;
    }
    return false;
}

PlayerContext player_context(const WorldState& w) {
    PlayerContext p;
    for (const Quest& q : w.quests.active) p.active_quests.push_back(q.def_id);
    p.completed_quests = w.quests.completed;
    return p;
}

}  // namespace

extern "C" {

int sim_world_quest_count(const SimWorld* w, const char* list) {
    return guard([&] {
        if (w == nullptr || list == nullptr) return -1;
        std::vector<Id> ids;
        return list_ids(w->world, list, ids) ? static_cast<int>(ids.size()) : -1;
    });
}

int sim_world_quest_at(const SimWorld* w, const char* list, int index, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || list == nullptr || index < 0) return -1;
        std::vector<Id> ids;
        if (!list_ids(w->world, list, ids) || index >= static_cast<int>(ids.size())) return -1;
        return write_req(out, cap, ids[static_cast<std::size_t>(index)]);
    });
}

int sim_world_quest_title(const SimWorld* w, const char* quest, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        if (const Row* r = quest_row(w->world, quest)) return write_req(out, cap, r->get("name"));
        return known_def(w->world, quest) ? write_req(out, cap, generated_title(w->world, quest)) : -1;
    });
}

int sim_world_quest_giver(const SimWorld* w, const char* quest, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        if (const Row* r = quest_row(w->world, quest)) return write_req(out, cap, r->get("giver"));
        return known_def(w->world, quest) ? write_req(out, cap, std::string()) : -1;  // the world gives it
    });
}

int sim_world_quest_kind(const SimWorld* w, const char* quest, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        const QuestDef* d = find_def(w->world, quest);
        return d ? write_req(out, cap, d->kind) : -1;
    });
}

int sim_world_quest_act(const SimWorld* w, const char* quest, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        if (const Row* r = quest_row(w->world, quest)) return write_req(out, cap, r->get("act"));
        return known_def(w->world, quest) ? write_req(out, cap, std::string()) : -1;
    });
}

int sim_world_quest_stage(const SimWorld* w, const char* quest, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        const Quest* q = find_active(w->world.quests, quest);
        return q ? write_req(out, cap, q->stage) : -1;
    });
}

int64_t sim_world_quest_deadline(const SimWorld* w, const char* quest) {
    try {
        if (w == nullptr || quest == nullptr) return -1;
        const Quest* q = find_active(w->world.quests, quest);
        return q ? static_cast<int64_t>(q->deadline) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_quest_accept(SimWorld* w, const char* quest) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        QuestState& q = w->world.quests;
        if (!known_def(w->world, quest)) return -2;
        if (world_driven(w->world, quest)) return -4;
        if (find_active(q, quest) || contains(q.completed, quest) || contains(q.failed_list, quest)) return -3;
        accept(q, quest, w->world.day);
        return 0;
    });
}

int sim_world_quest_advance(SimWorld* w, const char* quest, const char* stage, const char* text) {
    return guard([&] {
        if (w == nullptr || quest == nullptr || stage == nullptr) return -1;
        if (world_driven(w->world, quest)) return -4;
        if (!find_active(w->world.quests, quest)) return -3;
        return advance_stage(w->world.quests, quest, w->world.day, stage, text ? text : "") ? 1 : 0;
    });
}

int sim_world_quest_complete(SimWorld* w, const char* quest) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        if (world_driven(w->world, quest)) return -4;
        if (!find_active(w->world.quests, quest)) return -3;
        complete_quest(w->world, quest, w->world.day);  // pays silver + standing (Actions.hpp)
        return 0;
    });
}

int sim_world_quest_abandon(SimWorld* w, const char* quest) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        if (world_driven(w->world, quest)) return -4;
        if (!find_active(w->world.quests, quest)) return -3;
        abandon(w->world.quests, quest, w->world.day);
        return 0;
    });
}

int sim_world_journal_count(const SimWorld* w, const char* quest) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        return static_cast<int>(journal(w->world.quests, quest).size());
    });
}

int sim_world_journal_at(const SimWorld* w, const char* quest, int index, int64_t* day,
                         char* stage, int stage_cap, char* text, int text_cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr || index < 0) return -1;
        const auto& j = journal(w->world.quests, quest);
        if (index >= static_cast<int>(j.size())) return -1;
        const JournalEntry& e = j[static_cast<std::size_t>(index)];
        if (day != nullptr) *day = static_cast<int64_t>(e.day);
        write_req(stage, stage_cap, e.stage);
        return write_req(text, text_cap, e.text);
    });
}

int sim_world_dialogue_count(const SimWorld* w, const char* speaker) {
    return guard([&] {
        if (w == nullptr || speaker == nullptr) return -1;
        return static_cast<int>(eligible_lines(w->world.db, speaker, player_context(w->world)).size());
    });
}

int sim_world_dialogue_at(const SimWorld* w, const char* speaker, int index,
                          char* id, int id_cap, char* text, int text_cap) {
    return guard([&] {
        if (w == nullptr || speaker == nullptr || index < 0) return -1;
        const auto lines = eligible_lines(w->world.db, speaker, player_context(w->world));
        if (index >= static_cast<int>(lines.size())) return -1;
        const DialogueLine& l = lines[static_cast<std::size_t>(index)];
        write_req(id, id_cap, l.id);
        return write_req(text, text_cap, l.text);
    });
}

}  // extern "C"
