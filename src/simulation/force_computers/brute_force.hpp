#pragma once

#include "force_computer.hpp"

namespace unisim {

/**
 * @brief Brute-force O(N²) force computation
 * 
 * Computes gravitational forces between all pairs of bodies.
 * Simple but computationally expensive for large N.
 */
class BruteForce : public ForceComputer {
public:
    BruteForce(double gravitational_constant = 1.0, double softening = 0.01)
        : G_(gravitational_constant), softening_(softening) {}

    void compute_forces(Universe& universe) override;

    const char* name() const override {
        return "Brute Force";
    }

    void set_gravitational_constant(double G) {
        G_ = G;
    }

    void set_softening(double softening) {
        softening_ = softening;
    }

private:
    double G_;          // Gravitational constant
    double softening_;  // Softening parameter to prevent singularities
};

} // namespace unisim

