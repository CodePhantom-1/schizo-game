// CApiItems.cpp — P0a: the C ABI over item stacks, carrying, world items,
// containers, timed actions and notices (sim/CApiItems.h).
#include "sim/CApiItems.h"

#include "sim/ItemActions.hpp"

#include <cstring>
#include <string>

#include "CApiInternal.hpp"

using namespace sim;

namespace {

int write_str(char* out, int cap, const std::string& s) {
    if (out == nullptr || cap <= 0) return static_cast<int>(s.size());
    const int len = static_cast<int>(s.size());
    const int copy = (cap - 1 < len) ? cap - 1 : len;
    std::memcpy(out, s.data(), static_cast<std::size_t>(copy));
    out[copy] = '\0';
    return len;
}

// Getter variant: a null buffer is an error (-1), as in CApi.cpp.
int write_req(char* out, int cap, const std::string& s) {
    if (out == nullptr || cap <= 0) return -1;
    return write_str(out, cap, s);
}

template <class F>
auto guard(F&& f) -> decltype(f()) {
    try {
        return f();
    } catch (...) {
        return -1;
    }
}

}  // namespace

extern "C" {

int sim_world_last_reason(const SimWorld* world, char* out, int cap) {
    return guard([&] {
        if (world == nullptr) return -1;
        return write_req(out, cap, world->last_reason);
    });
}

int64_t sim_world_capacity_g(const SimWorld* world, const char* actor) {
    return guard([&]() -> int64_t {
        if (world == nullptr || actor == nullptr) return -1;
        return capacity_g(world->world, Id(actor));
    });
}

int64_t sim_world_carried_g(const SimWorld* world, const char* actor) {
    return guard([&]() -> int64_t {
        if (world == nullptr || actor == nullptr) return -1;
        return carried_g(world->world, Id(actor));
    });
}

int sim_world_encumbrance(const SimWorld* world, const char* actor) {
    return guard([&] {
        if (world == nullptr || actor == nullptr) return -1;
        return encumbrance(world->world, Id(actor));
    });
}

}  // extern "C"
