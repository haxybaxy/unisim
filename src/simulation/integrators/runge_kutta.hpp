#pragma once

#include "integrator.hpp"
#include "../force_computers/force_computer.hpp"
#include <vector>
#include <memory>

namespace unisim {

class RungeKuttaIntegrator : public Integrator {
public:
    explicit RungeKuttaIntegrator(std::shared_ptr<ForceComputer> force_computer)
        : force_computer_(force_computer) {}

    void step(Universe& universe, double dt) override;

    const char* name() const override {
        return "Runge-Kutta (RK4)";
    }

private:
    std::shared_ptr<ForceComputer> force_computer_;

    struct State {
        Vector3D position;
        Vector3D velocity;
        Vector3D acceleration;
    };
};

} // namespace unisim
