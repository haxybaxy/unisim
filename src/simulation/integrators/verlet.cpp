#include "verlet.hpp"
#include "../parallel_utils.hpp"

namespace unisim {

void VerletIntegrator::step(Universe& universe, double dt) {
    const std::size_t n = universe.size();
    if (n == 0) {
        return;
    }
    
    // Reset if universe size changed (bodies added/removed)
    if (initialized_ && previous_positions_.size() != n) {
        initialized_ = false;
        previous_positions_.clear();
    }
    
    // Initialize previous positions on first step
    if (!initialized_) {
        previous_positions_.resize(n);
        parallel_for_range(0, n, [&](std::size_t begin, std::size_t end, std::size_t, std::size_t) {
            for (std::size_t i = begin; i < end; ++i) {
                previous_positions_[i] = universe[i].position - universe[i].velocity * dt;
            }
        }, 512);
        initialized_ = true;
    }

    // Compute forces
    force_computer_->compute_forces(universe);

    // Verlet integration: x(t+dt) = 2*x(t) - x(t-dt) + a(t)*dt²
    parallel_for_range(0, n, [&](std::size_t begin, std::size_t end, std::size_t, std::size_t) {
        for (std::size_t i = begin; i < end; ++i) {
            Vector3D current_position = universe[i].position; // x(t)
            Vector3D old_position = previous_positions_[i];  // x(t-dt)

            Vector3D new_position = current_position * 2.0 - old_position 
                                    + universe[i].acceleration * (dt * dt);

            universe[i].velocity = (new_position - old_position) / (2.0 * dt);

            previous_positions_[i] = current_position;
            universe[i].position = new_position;
        }
    }, 512);
}

} // namespace unisim

