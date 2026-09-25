// CApiPeople.cpp — A7: NPC names, memories and the events feed through C.
#include "sim/CApiPeople.h"

#include "sim/Events.hpp"
#include "sim/Population.hpp"

#include <algorithm>
#include <cstring>
#include <string>

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

const Npc* find_npc(const WorldState& w, const char* id) {
    for (const Npc& n : w.population.npcs)
        if (n.id == id) return &n;
    return nullptr;
}

}  // namespace

extern "C" {

int sim_world_npc_name(const SimWorld* w, const char* npc, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || npc == nullptr) return -1;
        const Npc* n = find_npc(w->world, npc);
        return n ? write_req(out, cap, n->name) : -1;  // people.csv name: the display name
    });
}

int sim_world_npc_memory_count(const SimWorld* w, const char* npc) {
    return guard([&] {
        if (w == nullptr || npc == nullptr) return -1;
        const Npc* n = find_npc(w->world, npc);
        return n ? static_cast<int>(n->memory.size()) : -1;
    });
}

int sim_world_npc_memory_at(const SimWorld* w, const char* npc, int index, int64_t* day,
                            char* subject, int subject_cap, char* fact, int fact_cap) {
    return guard([&] {
        if (w == nullptr || npc == nullptr || index < 0) return -1;
        const Npc* n = find_npc(w->world, npc);
        if (n == nullptr || index >= static_cast<int>(n->memory.size())) return -1;
        const MemoryEntry& m = n->memory[static_cast<std::size_t>(index)];
        if (day != nullptr) *day = static_cast<int64_t>(m.day);
        write_req(subject, subject_cap, m.subject);
        return write_req(fact, fact_cap, m.fact);
    });
}

int sim_world_event_at(const SimWorld* w, int index, int64_t* day,
                       char* rule, int rule_cap, char* summary, int summary_cap) {
    return guard([&] {
        if (w == nullptr || index < 0) return -1;
        const auto& fired = w->world.events.fired;
        if (index >= static_cast<int>(fired.size())) return -1;
        const TriggeredEvent& e = fired[static_cast<std::size_t>(index)];
        if (day != nullptr) *day = static_cast<int64_t>(e.day);
        write_req(rule, rule_cap, e.rule_id);
        return write_req(summary, summary_cap, e.summary);
    });
}

}  // extern "C"
