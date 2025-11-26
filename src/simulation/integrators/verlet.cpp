#include "verlet.hpp"

namespace unisim {

void VerletIntegrator::step(Universe& universe, double dt) {
    const std::size_t n = universe.size();
    
    // Reset if universe size changed (bodies added/removed)
    if (initialized_ && previous_positions_.size() != n) {
        initialized_ = false;
        previous_positions_.clear();
    }
    
    // Initialize previous positions on first step
    if (!initialized_) {
        previous_positions_.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            previous_positions_[i] = universe[i].position - universe[i].velocity * dt;
        }
        initialized_ = true;
    }

    // Compute forces
    force_computer_->compute_forces(universe);

    // Verlet integration: x(t+dt) = 2*x(t) - x(t-dt) + a(t)*dt²
    for (std::size_t i = 0; i < n; ++i) {
        Vector3D current_position = universe[i].position; // x(t)
        Vector3D old_position = previous_positions_[i];  // x(t-dt)
        
        // Compute new position: x(t+dt) = 2*x(t) - x(t-dt) + a(t)*dt²
        Vector3D new_position = current_position * 2.0 - old_position 
                                + universe[i].acceleration * (dt * dt);
        
        // Update velocity: v(t) = (x(t+dt) - x(t-dt)) / (2*dt)
        // This is the standard Verlet velocity formula (centered difference at time t)
        // More accurate than forward difference
        universe[i].velocity = (new_position - old_position) / (2.0 * dt);
        
        // Store current position as previous for next iteration
        previous_positions_[i] = current_position;
        universe[i].position = new_position;
    }
}

} // namespace unisim

