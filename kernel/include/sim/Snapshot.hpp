#pragma once
// Snapshot.hpp — the save file (T1 module; contract fixed by World.hpp).
// World.hpp: "This is the save file: serialize the whole struct, restore it,
// and the world continues identically."
//
// Format: a deterministic, versioned, human-diffable tab-separated text
// ("SIMSAVE 1\n" header line, then one scalar/section line per field of every
// module state). Same WorldState => identical bytes. Free text fields
// (names, summaries, place tags…) are escaped (\\, \t, \n, \r) so any commas,
// quotes or newlines in canon-sourced strings round-trip safely.
//
// The Db (canon) is NOT part of the save — canon is data, not state. Calendar
// is also not saved: it is rebuilt deterministically from canon by
// WorldState::init, called internally by load_world with the given seed
// before every field is restored.
#include "sim/World.hpp"

#include <string>

namespace sim {

// Serializes every field of every module state + day, seed, rng state and
// WorldFacts. Deterministic: identical WorldState content => identical bytes.
std::string save_world(const WorldState& w);

// Reloads canon from canon_dir (rebuilds db + calendar), then restores every
// field of every module state + day, seed, rng state, facts from `data`.
// Throws std::runtime_error on a malformed save or an unknown version header.
void load_world(WorldState& w, const std::string& canon_dir, const std::string& data);

}  // namespace sim
