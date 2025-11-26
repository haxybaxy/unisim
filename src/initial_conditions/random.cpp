#include "random.hpp"
#include <random>
#include <cmath>

namespace unisim {

void RandomInitializer::initialize(Universe& universe, std::size_t num_bodies) {
    universe.clear();
    universe.reserve(num_bodies);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> pos_dist(-box_size_ / 2.0, box_size_ / 2.0);
    std::uniform_real_distribution<double> vel_dist(-max_velocity_, max_velocity_);
    std::uniform_real_distribution<double> mass_dist(mass_min_, mass_max_);

    for (std::size_t i = 0; i < num_bodies; ++i) {
        Body body;
        body.position = Vector3D(pos_dist(gen), pos_dist(gen), pos_dist(gen));
        body.velocity = Vector3D(vel_dist(gen), vel_dist(gen), vel_dist(gen));
        body.mass = mass_dist(gen);
        body.radius = std::cbrt(body.mass) * 0.1; // Radius proportional to mass^(1/3)
        
        universe.add_body(std::move(body));
    }
}

} // namespace unisim

