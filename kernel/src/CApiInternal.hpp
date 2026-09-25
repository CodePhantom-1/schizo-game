#pragma once
// CApiInternal.hpp — the one definition of the opaque SimWorld handle, shared
// by every C API translation unit (CApi.cpp, CApiWild.cpp, ...). Defining it
// separately in each .cpp is an ODR violation (undefined behaviour) the moment
// the copies differ. Private to kernel/src; never installed.
#include "sim/World.hpp"

#include <string>

struct SimWorld {
    sim::WorldState world;
    std::string w4a_refusal;  // W4-A: why the last progression verb refused ("" = it didn't)
};
