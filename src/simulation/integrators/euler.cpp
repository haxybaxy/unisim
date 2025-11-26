#include "euler.hpp"
#include "../parallel_utils.hpp"

namespace unisim {

void EulerIntegrator::step(Universe& universe, double dt) {
    const std::size_t n = universe.size();
    if (n == 0) {
        return;
    }

    force_computer_->compute_forces(universe);

    parallel_for_range(0, n, [&](std::size_t begin, std::size_t end, std::size_t, std::size_t) {
        for (std::size_t i = begin; i < end; ++i) {
            universe[i].velocity += universe[i].acceleration * dt;
            universe[i].position += universe[i].velocity * dt;
        }
    }, 512);
}

} // namespace unisim

