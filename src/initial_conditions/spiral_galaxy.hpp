#pragma once

#include "initializer.hpp"

namespace unisim {

/**
 * @brief Spiral galaxy initial conditions
 * 
 * Creates a spiral galaxy with:
 * - Central bulge (massive core)
 * - Spiral arms (rotating disk)
 * - Dark matter halo (optional)
 */
class SpiralGalaxyInitializer : public Initializer {
public:
    SpiralGalaxyInitializer(double galaxy_size = 20.0, int num_arms = 2, double arm_tightness = 0.3,
                            float color_r = 1.0f, float color_g = 0.5f, float color_b = 0.0f)
        : galaxy_size_(galaxy_size), num_arms_(num_arms), arm_tightness_(arm_tightness),
          color_r_(color_r), color_g_(color_g), color_b_(color_b) {}

    void initialize(Universe& universe, std::size_t num_bodies) override;

    const char* name() const override {
        return "Spiral Galaxy";
    }

private:
    double galaxy_size_;
    int num_arms_;
    double arm_tightness_; // Controls how tightly wound the arms are
    float color_r_, color_g_, color_b_;
};

} // namespace unisim

