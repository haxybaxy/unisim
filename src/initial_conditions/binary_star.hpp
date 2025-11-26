#pragma once

#include "initializer.hpp"

namespace unisim {

/**
 * @brief Binary star system initial conditions
 * 
 * Creates a binary star system with:
 * - Two massive stars orbiting each other
 * - Planets orbiting the binary
 * - Stable orbital configuration
 */
class BinaryStarInitializer : public Initializer {
public:
    BinaryStarInitializer(double separation = 10.0, int num_planets = 4, double mass_ratio = 1.0)
        : separation_(separation), num_planets_(num_planets), mass_ratio_(mass_ratio) {}

    void initialize(Universe& universe, std::size_t num_bodies) override;

    const char* name() const override {
        return "Binary Star System";
    }

private:
    double separation_;  // Separation between stars
    int num_planets_;
    double mass_ratio_;  // Mass ratio (star2 / star1)
};

} // namespace unisim

