#pragma once

#include "initializer.hpp"

namespace unisim {

/**
 * @brief Black hole system initial conditions
 * 
 * Creates a black hole with orbiting bodies:
 * - Central black hole (very massive)
 * - Orbiting stars/particles in various orbits (spiral patterns, accretion disk)
 * - Realistic orbital velocities based on black hole mass
 */
class BlackHoleInitializer : public Initializer {
public:
    BlackHoleInitializer(double black_hole_mass = 10000.0, int num_orbiting_bodies = 150, 
                         double system_radius = 50.0)
        : black_hole_mass_(black_hole_mass), 
          num_orbiting_bodies_(num_orbiting_bodies),
          system_radius_(system_radius) {}

    void initialize(Universe& universe, std::size_t num_bodies) override;

    const char* name() const override {
        return "Black Hole System";
    }

private:
    double black_hole_mass_;
    int num_orbiting_bodies_;
    double system_radius_;
};

} // namespace unisim
