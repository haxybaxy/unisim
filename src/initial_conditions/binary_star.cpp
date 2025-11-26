#include "binary_star.hpp"
#include <random>
#include <cmath>

namespace unisim {

void BinaryStarInitializer::initialize(Universe& universe, std::size_t num_bodies) {
    universe.clear();
    universe.reserve(num_bodies);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radial_dist(0.0, 1.0);
    
    // Star masses
    double m1 = 1.0; // Primary star
    double m2 = m1 * mass_ratio_; // Secondary star
    double total_mass = m1 + m2;
    
    // Center of mass
    double com_distance = separation_ * m2 / total_mass;
    
    // Star 1 (more massive, at negative x)
    Body star1;
    star1.position = Vector3D(-com_distance, 0.0, 0.0);
    
    // Orbital velocity for circular orbit
    double v_orbital = std::sqrt(total_mass / separation_);
    star1.velocity = Vector3D(0.0, -v_orbital * m2 / total_mass, 0.0);
    
    star1.mass = m1 * 10.0; // Scale up for visibility
    star1.radius = std::cbrt(star1.mass) * 0.3;
    
    universe.add_body(std::move(star1));
    
    // Star 2 (less massive, at positive x)
    Body star2;
    star2.position = Vector3D(separation_ - com_distance, 0.0, 0.0);
    star2.velocity = Vector3D(0.0, v_orbital * m1 / total_mass, 0.0);
    
    star2.mass = m2 * 10.0;
    star2.radius = std::cbrt(star2.mass) * 0.3;
    
    universe.add_body(std::move(star2));
    
    // Planets orbiting the binary
    std::size_t planet_count = std::min(static_cast<std::size_t>(num_planets_), num_bodies - 2);
    
    for (std::size_t i = 0; i < planet_count; ++i) {
        Body body;
        
        // Distance from center of mass (stable orbits are typically > 2x separation)
        double min_distance = separation_ * 2.5;
        double max_distance = separation_ * 8.0;
        double r = min_distance + (max_distance - min_distance) * std::pow(radial_dist(gen), 0.6);
        
        // Random angle
        double angle = angle_dist(gen);
        double inclination = (radial_dist(gen) - 0.5) * 0.3; // Slight inclination
        
        // Position relative to center of mass
        body.position.x = r * std::cos(angle) * std::cos(inclination);
        body.position.y = r * std::sin(angle) * std::cos(inclination);
        body.position.z = r * std::sin(inclination);
        
        // Orbital velocity (approximate, treating binary as point mass)
        double v = std::sqrt(total_mass / r);
        double v_angle = angle + M_PI / 2.0;
        
        body.velocity.x = -v * std::sin(v_angle) * std::cos(inclination);
        body.velocity.y = v * std::cos(v_angle) * std::cos(inclination);
        body.velocity.z = v * std::cos(v_angle) * std::sin(inclination);
        
        // Planet mass (small)
        body.mass = 0.001 + radial_dist(gen) * 0.01;
        body.radius = std::cbrt(body.mass) * 0.15;
        
        universe.add_body(std::move(body));
    }
}

} // namespace unisim

