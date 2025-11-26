#pragma once

#include "initializer.hpp"

namespace unisim {

/**
 * @brief Galaxy collision initial conditions
 * 
 * Creates two galaxies on a collision course:
 * - Two spiral galaxies
 * - Approaching each other
 * - Can create interesting merger dynamics
 */
class GalaxyCollisionInitializer : public Initializer {
public:
    GalaxyCollisionInitializer(double galaxy_size = 15.0, double separation = 30.0,
                               float galaxy1_r = 1.0f, float galaxy1_g = 0.3f, float galaxy1_b = 0.0f,
                               float galaxy2_r = 0.0f, float galaxy2_g = 0.5f, float galaxy2_b = 1.0f)
        : galaxy_size_(galaxy_size), separation_(separation),
          galaxy1_r_(galaxy1_r), galaxy1_g_(galaxy1_g), galaxy1_b_(galaxy1_b),
          galaxy2_r_(galaxy2_r), galaxy2_g_(galaxy2_g), galaxy2_b_(galaxy2_b) {}

    void initialize(Universe& universe, std::size_t num_bodies) override;

    const char* name() const override {
        return "Galaxy Collision";
    }

private:
    double galaxy_size_;
    double separation_; // Initial separation between galaxy centers
    float galaxy1_r_, galaxy1_g_, galaxy1_b_;
    float galaxy2_r_, galaxy2_g_, galaxy2_b_;
};

} // namespace unisim

