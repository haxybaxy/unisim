#include "leapfrog.hpp"
#include "../parallel_utils.hpp"

namespace unisim {

void LeapfrogIntegrator::step(Universe& universe, double dt) {
    const std::size_t n = universe.size();
    if (n == 0) {
        return;
    }

    // Compute forces at current positions
    force_computer_->compute_forces(universe);

    // Leapfrog: v(t+dt/2) = v(t) + a(t)*dt/2
    //           x(t+dt) = x(t) + v(t+dt/2)*dt
    //           v(t+dt) = v(t+dt/2) + a(t+dt)*dt/2
    
    // Half-step velocity update
    parallel_for_range(0, n, [&](std::size_t begin, std::size_t end, std::size_t, std::size_t) {
        for (std::size_t i = begin; i < end; ++i) {
            universe[i].velocity += universe[i].acceleration * (dt * 0.5);
        }
    }, 512);

    // Full-step position update
    parallel_for_range(0, n, [&](std::size_t begin, std::size_t end, std::size_t, std::size_t) {
        for (std::size_t i = begin; i < end; ++i) {
            universe[i].position += universe[i].velocity * dt;
        }
    }, 512);

    // Compute forces at new positions
    force_computer_->compute_forces(universe);

    // Half-step velocity update
    parallel_for_range(0, n, [&](std::size_t begin, std::size_t end, std::size_t, std::size_t) {
        for (std::size_t i = begin; i < end; ++i) {
            universe[i].velocity += universe[i].acceleration * (dt * 0.5);
        }
    }, 512);
}

} // namespace unisim

