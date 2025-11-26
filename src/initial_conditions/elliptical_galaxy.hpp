#pragma once

#include "initializer.hpp"

namespace unisim {

/**
 * @brief Elliptical galaxy initial conditions
 * 
 * Creates an elliptical galaxy with:
 * - Dense central core
 * - Ellipsoidal distribution
 * - Random orbital directions
 */
class EllipticalGalaxyInitializer : public Initializer {
public:
    EllipticalGalaxyInitializer(double galaxy_size = 15.0, double ellipticity = 0.7,
                                float color_r = 0.8f, float color_g = 0.8f, float color_b = 0.2f)
        : galaxy_size_(galaxy_size), ellipticity_(ellipticity),
          color_r_(color_r), color_g_(color_g), color_b_(color_b) {}

    void initialize(Universe& universe, std::size_t num_bodies) override;

    const char* name() const override {
        return "Elliptical Galaxy";
    }

private:
    double galaxy_size_;
    double ellipticity_; // 0 = spherical, 1 = very elongated
    float color_r_, color_g_, color_b_;
};

} // namespace unisim

