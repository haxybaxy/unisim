#pragma once

#include "integrator.hpp"
#include "../force_computers/force_computer.hpp"
#include <memory>

namespace unisim {

/**
 * @brief Leapfrog integrator (second-order, symplectic)
 * 
 * Also known as Verlet velocity method. Good energy conservation.
 */
class LeapfrogIntegrator : public Integrator {
public:
    explicit LeapfrogIntegrator(std::shared_ptr<ForceComputer> force_computer)
        : force_computer_(force_computer) {}

    void step(Universe& universe, double dt) override;

    const char* name() const override {
        return "Leapfrog";
    }

private:
    std::shared_ptr<ForceComputer> force_computer_;
};

} // namespace unisim

