#pragma once

#include "integrator.hpp"
#include "../force_computers/force_computer.hpp"
#include <memory>

namespace unisim {

/**
 * @brief Euler method integrator (first-order)
 * 
 * Simple but not very accurate. Good for testing.
 */
class EulerIntegrator : public Integrator {
public:
    explicit EulerIntegrator(std::shared_ptr<ForceComputer> force_computer)
        : force_computer_(force_computer) {}

    void step(Universe& universe, double dt) override;

    const char* name() const override {
        return "Euler";
    }

private:
    std::shared_ptr<ForceComputer> force_computer_;
};

} // namespace unisim

