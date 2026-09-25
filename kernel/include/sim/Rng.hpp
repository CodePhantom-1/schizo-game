#pragma once
// Rng.hpp — the one deterministic random source (contract, coordinator-owned).
// Every module takes its randomness from the WorldContext's Rng; nothing else
// may seed or hold one. Determinism of the daily tick depends on this.
#include <cstdint>

namespace sim {

class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed ? seed : 0x9E3779B97F4A7C15ull) {}

    std::uint64_t next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // Uniform integer in [lo, hi] (inclusive).
    int int_in(int lo, int hi) {
        if (hi <= lo) return lo;
        const std::uint64_t span = static_cast<std::uint64_t>(hi - lo) + 1;
        return lo + static_cast<int>(next() % span);
    }

    // Uniform double in [0, 1).
    double unit() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }

    // A derived stream: mixes the current state with a salt WITHOUT advancing
    // the parent, so a module's draws depend only on (world state, salt).
    Rng fork(std::uint64_t salt) const {
        return Rng(state_ ^ (salt * 0xD1B54A32D192ED03ull));
    }

    // Save/load only (sim::Snapshot): the raw generator state, so a save can
    // restore the exact sequence rather than reseed it.
    std::uint64_t state() const { return state_; }
    void set_state(std::uint64_t s) { state_ = s; }

private:
    std::uint64_t state_;
};

}  // namespace sim
