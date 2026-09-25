// CApiDivine.cpp — W5: the C ABI over divine wrath (sim/CApiDivine.h).
// Thin: every entry point delegates to sim/Divine.hpp, and none lets an
// exception cross into the engine (the CApiWild.cpp pattern). SimWorld is
// defined once in kernel/src/CApiInternal.hpp.
#include "sim/CApiDivine.h"

#include "sim/Divine.hpp"

#include <cstring>
#include <string>

#include "CApiInternal.hpp"

using namespace sim;

namespace {

int write_req(char* out, int cap, const std::string& s) {
    if (out == nullptr || cap <= 0) return -1;
    const int len = static_cast<int>(s.size());
    const int copy = (cap - 1 < len) ? cap - 1 : len;
    std::memcpy(out, s.data(), static_cast<std::size_t>(copy));
    out[copy] = '\0';
    return len;
}

template <class F>
int guard(F&& f) {
    try {
        return f();
    } catch (...) {
        return -1;
    }
}

}  // namespace

extern "C" {

int sim_world_divine_wrath(const SimWorld* world, const char* offender, const char* deity) {
    return guard([&] {
        return world && offender && deity ? wrath(world->world.divine, Id(offender), Id(deity)) : -1;
    });
}

int sim_world_divine_tier(const SimWorld* world, const char* offender, const char* deity) {
    return guard([&] {
        return world && offender && deity
                   ? wrath_tier(wrath(world->world.divine, Id(offender), Id(deity)))
                   : -1;
    });
}

int sim_world_divine_cursed(const SimWorld* world, const char* offender, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || offender == nullptr) return -1;
        const Id who = Id(offender);
        if (!is_cursed(world->world.divine, who)) return 0;
        if (out != nullptr && cap > 0) (void)write_req(out, cap, curse_deity(world->world.divine, who));
        return 1;
    });
}

int sim_world_divine_entry_count(const SimWorld* world) {
    return guard([&] {
        if (world == nullptr) return -1;
        int n = 0;
        for (const auto& [offender, per_deity] : world->world.divine.wrath_by_offender)
            n += static_cast<int>(per_deity.size());
        return n;
    });
}

int sim_world_divine_entry(const SimWorld* world, int index, char* out, int cap) {
    return guard([&] {
        if (world == nullptr || index < 0) return -1;
        int i = 0;
        for (const auto& [offender, per_deity] : world->world.divine.wrath_by_offender)
            for (const auto& [deity, value] : per_deity) {
                if (i++ != index) continue;
                return write_req(out, cap, offender + ";" + deity + ";" + std::to_string(value));
            }
        return -1;  // index past the last entry
    });
}

int sim_world_break_oath(SimWorld* world, const char* oath_id) {
    return guard([&] {
        if (world == nullptr || oath_id == nullptr) return -1;
        const Oath* oath = nullptr;
        for (const Oath& o : world->world.faction.oaths)
            if (o.id == Id(oath_id)) {
                oath = &o;
                break;
            }
        if (oath == nullptr) return -2;
        if (oath->broken) return -3;
        break_oath_in_world(world->world, Id(oath_id));
        return 0;
    });
}

}  // extern "C"
