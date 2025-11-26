#include "brute_force.hpp"
#include <cmath>
#include <vector>
#include <thread>

#include "../parallel_utils.hpp"

namespace unisim {

void BruteForce::compute_forces(Universe& universe) {
    const std::size_t n = universe.size();

    if (n == 0) {
        return;
    }

    if (n == 1) {
        universe[0].acceleration = Vector3D(0.0, 0.0, 0.0);
        return;
    }

    const std::size_t num_threads = determine_thread_count(n, 256);
    std::vector<std::vector<Vector3D>> thread_acc(num_threads, std::vector<Vector3D>(n, Vector3D(0.0, 0.0, 0.0)));

    parallel_for_range(0, n, num_threads, [&](std::size_t begin, std::size_t end, std::size_t worker_idx, std::size_t /*worker_count*/) {
        auto& local = thread_acc[worker_idx];

        for (std::size_t i = begin; i < end; ++i) {
            const Body& body_i = universe[i];
            for (std::size_t j = i + 1; j < n; ++j) {
                const Body& body_j = universe[j];

                Vector3D r = body_j.position - body_i.position;
                
                // Adaptive softening for black holes (use Schwarzschild radius as minimum softening)
                double effective_softening = softening_;
                if (body_j.is_blackhole) {
                    double schwarzschild_r = body_j.schwarzschild_radius(G_);
                    effective_softening = std::max(softening_, schwarzschild_r * 0.5);
                } else if (body_i.is_blackhole) {
                    double schwarzschild_r = body_i.schwarzschild_radius(G_);
                    effective_softening = std::max(softening_, schwarzschild_r * 0.5);
                }
                
                double dist_sq = r.magnitude_squared() + effective_softening * effective_softening;

                if (dist_sq <= 0.0) {
                    continue;
                }

                double dist = std::sqrt(dist_sq);

                // Force magnitude: F = G * m1 * m2 / r²
                double force_mag = G_ * body_i.mass * body_j.mass / dist_sq;

                // Force direction
                Vector3D force_dir = r / dist;
                Vector3D force = force_dir * force_mag;

                // Acceleration contributions
                local[i] += force / body_i.mass;
                local[j] -= force / body_j.mass;
            }
        }
    });

    for (std::size_t i = 0; i < n; ++i) {
        Vector3D total(0.0, 0.0, 0.0);
        for (const auto& acc : thread_acc) {
            total += acc[i];
        }
        universe[i].acceleration = total;
    }
}

} // namespace unisim

