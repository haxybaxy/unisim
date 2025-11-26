#include "runge_kutta.hpp"

namespace unisim {

void RungeKuttaIntegrator::step(Universe& universe, double dt) {
    size_t n = universe.size();
    if (n == 0) return;

    // We need a temporary universe to compute forces at intermediate steps
    Universe temp_universe = universe;

    struct Derivative {
        Vector3D dx; // velocity
        Vector3D dv; // acceleration
    };

    std::vector<Derivative> k1(n), k2(n), k3(n), k4(n);

    // Helper to compute derivatives
    auto compute_derivative = [&](Universe& u, std::vector<Derivative>& k) {
        force_computer_->compute_forces(u);
        for (size_t i = 0; i < n; ++i) {
            k[i].dx = u[i].velocity;
            k[i].dv = u[i].acceleration;
        }
    };

    // Helper to advance state
    auto advance_state = [&](const Universe& initial, Universe& target, const std::vector<Derivative>& k, double scale) {
        for (size_t i = 0; i < n; ++i) {
            target[i].position = initial[i].position + k[i].dx * scale;
            target[i].velocity = initial[i].velocity + k[i].dv * scale;
        }
    };

    // k1: compute at initial state
    compute_derivative(temp_universe, k1); // temp_universe is effectively initial state here

    // k2: compute at initial + k1*dt/2
    advance_state(universe, temp_universe, k1, dt * 0.5);
    compute_derivative(temp_universe, k2);

    // k3: compute at initial + k2*dt/2 (NOT k1!)
    advance_state(universe, temp_universe, k2, dt * 0.5);
    compute_derivative(temp_universe, k3);

    // k4: compute at initial + k3*dt
    advance_state(universe, temp_universe, k3, dt);
    compute_derivative(temp_universe, k4);

    // Final update
    for (size_t i = 0; i < n; ++i) {
        Vector3D d_pos = (k1[i].dx + k2[i].dx * 2.0 + k3[i].dx * 2.0 + k4[i].dx) * (dt / 6.0);
        Vector3D d_vel = (k1[i].dv + k2[i].dv * 2.0 + k3[i].dv * 2.0 + k4[i].dv) * (dt / 6.0);

        universe[i].position += d_pos;
        universe[i].velocity += d_vel;
    }
}

} // namespace unisim

