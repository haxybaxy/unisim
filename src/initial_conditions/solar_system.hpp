#pragma once

#include "initializer.hpp"

namespace unisim {

/**
 * @brief Solar system initial conditions
 * 
 * Creates a realistic solar system with:
 * - Central star (Sun)
 * - Planets in roughly circular orbits
 * - Scaled masses and distances for visualization
 */
class SolarSystemInitializer : public Initializer {
public:
    SolarSystemInitializer(int num_planets = 8, bool include_asteroids = true, double scale_factor = 1.0)
        : num_planets_(num_planets), include_asteroids_(include_asteroids), scale_factor_(scale_factor) {}

    void initialize(Universe& universe, std::size_t num_bodies) override;

    const char* name() const override {
        return "Solar System";
    }

private:
    int num_planets_;
    bool include_asteroids_;
    double scale_factor_; // Scale factor for distances (1.0 = realistic scale)
};

} // namespace unisim

