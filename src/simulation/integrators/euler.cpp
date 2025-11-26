#include "euler.hpp"

namespace unisim {

void EulerIntegrator::step(Universe& universe, double dt) {
    // Compute forces
    force_computer_->compute_forces(universe);

    // Update positions and velocities
    for (std::size_t i = 0; i < universe.size(); ++i) {
        universe[i].velocity += universe[i].acceleration * dt;
        universe[i].position += universe[i].velocity * dt;
    }
}

} // namespace unisim

