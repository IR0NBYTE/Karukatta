#pragma once

#include "../ir.hpp"
#include <random>
#include <string>

/*
    Karukatta Pass Infrastructure
    All optimization and obfuscation passes implement this interface.
    Passes transform IR → IR.
*/

class Pass {
public:
    virtual ~Pass() = default;
    virtual std::string name() const = 0;
    virtual void run(IRModule& module) = 0;
};

// Seeded PRNG for reproducible obfuscation
class ObfuscationRNG {
public:
    explicit ObfuscationRNG(uint64_t seed = 0) : m_rng(seed) {}

    void set_seed(uint64_t seed) { m_rng.seed(seed); }

    // Random integer in [min, max]
    int64_t rand_int(int64_t min, int64_t max) {
        std::uniform_int_distribution<int64_t> dist(min, max);
        return dist(m_rng);
    }

    // Random register-sized value
    int64_t rand_val() {
        return rand_int(1, 0x7FFFFFFF);
    }

    // Random bool with given probability of true
    bool rand_bool(double p = 0.5) {
        std::bernoulli_distribution dist(p);
        return dist(m_rng);
    }

    // Random choice from 0..n-1
    int rand_choice(int n) {
        return (int)rand_int(0, n - 1);
    }

private:
    std::mt19937_64 m_rng;
};

// Global RNG instance (set seed from CLI)
inline ObfuscationRNG g_obf_rng;
