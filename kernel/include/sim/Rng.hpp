#pragma once
// Rng.hpp — the one deterministic random source (contract, coordinator-owned).
// Every module takes its randomness from the WorldContext's Rng; nothing else
// may seed or hold one. Determinism of the daily tick depends on this.
#include <cstdint>
#include <string_view>

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

    // Uniform integer in [0, n), unbiased: draws falling in the short final
    // block of 2^64 (the 2^64 mod n lowest values) are rejected and redrawn.
    // Pure integer maths, so identical on every CPU (D-022). n == 0 returns 0.
    // (int_in above keeps its modulo draw: Events' existing rolls depend on it.)
    std::uint64_t below(std::uint64_t n) {
        if (n == 0) return 0;
        const std::uint64_t reject_under = (0 - n) % n;  // == 2^64 mod n
        for (;;) {
            const std::uint64_t x = next();
            if (x >= reject_under) return x % n;
        }
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

// FNV-1a 64 over the bytes of `s`: a salt for fork() that is the same with
// every standard library and platform (std::hash is not). D-022.
inline std::uint64_t stable_hash(std::string_view s) {
    std::uint64_t h = 14695981039346656037ull;  // FNV offset basis
    for (const char c : s) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;  // FNV prime
    }
    return h;
}

}  // namespace sim
