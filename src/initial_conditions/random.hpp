#pragma once

#include "initializer.hpp"

namespace unisim {

/**
 * @brief Random initial conditions generator
 */
class RandomInitializer : public Initializer {
public:
    RandomInitializer(double box_size = 10.0, double max_velocity = 1.0, double mass_range_min = 0.1, double mass_range_max = 1.0)
        : box_size_(box_size), max_velocity_(max_velocity), mass_min_(mass_range_min), mass_max_(mass_range_max) {}

    void initialize(Universe& universe, std::size_t num_bodies) override;

    const char* name() const override {
        return "Random";
    }

private:
    double box_size_;
    double max_velocity_;
    double mass_min_;
    double mass_max_;
};

} // namespace unisim

