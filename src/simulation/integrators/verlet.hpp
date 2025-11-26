#pragma once

#include "integrator.hpp"
#include "../force_computers/force_computer.hpp"
#include "../universe.hpp"
#include <memory>
#include <vector>

namespace unisim {

/**
 * @brief Verlet integrator (second-order, symplectic)
 * 
 * More accurate than Euler and preserves energy better.
 */
class VerletIntegrator : public Integrator {
public:
    explicit VerletIntegrator(std::shared_ptr<ForceComputer> force_computer)
        : force_computer_(force_computer) {}

    void step(Universe& universe, double dt) override;
    
    void reset() {
        initialized_ = false;
        previous_positions_.clear();
    }

    const char* name() const override {
        return "Verlet";
    }

private:
    std::shared_ptr<ForceComputer> force_computer_;
    std::vector<Vector3D> previous_positions_;
    bool initialized_{false};
};

} // namespace unisim

