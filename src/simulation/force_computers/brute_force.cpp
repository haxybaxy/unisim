#include "brute_force.hpp"
#include <cmath>

namespace unisim {

void BruteForce::compute_forces(Universe& universe) {
    const std::size_t n = universe.size();
    
    // Reset accelerations
    for (std::size_t i = 0; i < n; ++i) {
        universe[i].acceleration = Vector3D(0.0, 0.0, 0.0);
    }

    // Compute forces between all pairs
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            Vector3D r = universe[j].position - universe[i].position;
            double dist_sq = r.magnitude_squared() + softening_ * softening_;
            double dist = std::sqrt(dist_sq);
            
            // Force magnitude: F = G * m1 * m2 / r²
            double force_mag = G_ * universe[i].mass * universe[j].mass / dist_sq;
            
            // Force direction
            Vector3D force_dir = r / dist;
            Vector3D force = force_dir * force_mag;
            
            // Apply forces (Newton's third law)
            universe[i].acceleration += force / universe[i].mass;
            universe[j].acceleration -= force / universe[j].mass;
        }
    }
}

} // namespace unisim

