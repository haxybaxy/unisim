#include "leapfrog.hpp"

namespace unisim {

void LeapfrogIntegrator::step(Universe& universe, double dt) {
    // Compute forces at current positions
    force_computer_->compute_forces(universe);

    // Leapfrog: v(t+dt/2) = v(t) + a(t)*dt/2
    //           x(t+dt) = x(t) + v(t+dt/2)*dt
    //           v(t+dt) = v(t+dt/2) + a(t+dt)*dt/2
    
    // Half-step velocity update
    for (std::size_t i = 0; i < universe.size(); ++i) {
        universe[i].velocity += universe[i].acceleration * (dt * 0.5);
    }

    // Full-step position update
    for (std::size_t i = 0; i < universe.size(); ++i) {
        universe[i].position += universe[i].velocity * dt;
    }

    // Compute forces at new positions
    force_computer_->compute_forces(universe);

    // Half-step velocity update
    for (std::size_t i = 0; i < universe.size(); ++i) {
        universe[i].velocity += universe[i].acceleration * (dt * 0.5);
    }
}

} // namespace unisim

