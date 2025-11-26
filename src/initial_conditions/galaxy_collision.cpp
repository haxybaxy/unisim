#include "galaxy_collision.hpp"
#include <random>
#include <cmath>

namespace unisim {

void GalaxyCollisionInitializer::initialize(Universe& universe, std::size_t num_bodies) {
    universe.clear();
    universe.reserve(num_bodies);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radial_dist(0.0, 1.0);
    
    // Split bodies between two galaxies
    std::size_t bodies_per_galaxy = num_bodies / 2;
    
    // Estimate mass per galaxy (same as spiral galaxy)
    double estimated_galaxy_mass = bodies_per_galaxy * 1.1;
    double G = 1.0; // Gravitational constant
    
    // Galaxy 1 (left, moving right)
    Vector3D center1(-separation_ / 2.0, 0.0, 0.0);
    Vector3D velocity1(0.3, 0.1, 0.0); // Approaching velocity
    
    for (std::size_t i = 0; i < bodies_per_galaxy; ++i) {
        Body body;
        
        // Bulge or disk
        bool in_bulge = (i < bodies_per_galaxy / 10);
        
        if (in_bulge) {
            // Central bulge
            double r = galaxy_size_ * 0.15 * std::pow(radial_dist(gen), 1.0/3.0);
            double theta = angle_dist(gen);
            double phi = std::acos(2.0 * radial_dist(gen) - 1.0);
            
            body.position = center1 + Vector3D(
                r * std::sin(phi) * std::cos(theta),
                r * std::sin(phi) * std::sin(theta),
                r * std::cos(phi) * 0.3
            );
            
            // Estimate enclosed mass: M_enc(r) ≈ M_total * (r/R)^1.5 for bulge
            double r_normalized = r / galaxy_size_;
            double M_enc = estimated_galaxy_mass * std::pow(r_normalized, 1.5);
            double v_circular = std::sqrt(G * M_enc / std::max(r, 0.1));
            double v_mag = v_circular * (0.7 + 0.6 * radial_dist(gen));
            double v_theta = angle_dist(gen);
            body.velocity = velocity1 + Vector3D(
                v_mag * std::cos(v_theta),
                v_mag * std::sin(v_theta),
                (radial_dist(gen) - 0.5) * 0.05
            );
            
            body.mass = 2.0 + radial_dist(gen) * 3.0;
        } else {
            // Spiral arm (simplified)
            double r = galaxy_size_ * (0.2 + 0.8 * std::pow(radial_dist(gen), 0.7));
            double angle = angle_dist(gen) + 0.3 * std::log(r / (galaxy_size_ * 0.2));
            double angle_offset = (radial_dist(gen) - 0.5) * 0.3;
            angle += angle_offset;
            
            body.position = center1 + Vector3D(
                r * std::cos(angle),
                r * std::sin(angle),
                (radial_dist(gen) - 0.5) * galaxy_size_ * 0.05
            );
            
            // Estimate enclosed mass: M_enc(r) ≈ M_total * (r/R)^1.2 for disk
            double r_normalized = r / galaxy_size_;
            double M_enc = estimated_galaxy_mass * std::pow(r_normalized, 1.2);
            double v_circular = std::sqrt(G * M_enc / std::max(r, 0.1));
            double v_angle = angle + M_PI / 2.0;
            double v_random = v_circular * (radial_dist(gen) - 0.5) * 0.15;
            
            body.velocity = velocity1 + Vector3D(
                v_circular * std::cos(v_angle) + v_random * std::cos(angle),
                v_circular * std::sin(v_angle) + v_random * std::sin(angle),
                (radial_dist(gen) - 0.5) * 0.1
            );
            
            double mass_factor = 1.0 - 0.5 * (r / galaxy_size_);
            body.mass = 0.1 + mass_factor * 1.5;
        }
        
        body.radius = std::cbrt(body.mass) * 0.1;
        body.color_r = galaxy1_r_;
        body.color_g = galaxy1_g_;
        body.color_b = galaxy1_b_;
        universe.add_body(std::move(body));
    }
    
    // Galaxy 2 (right, moving left)
    Vector3D center2(separation_ / 2.0, 0.0, 0.0);
    Vector3D velocity2(-0.3, -0.1, 0.0); // Approaching velocity
    
    for (std::size_t i = 0; i < bodies_per_galaxy; ++i) {
        Body body;
        
        bool in_bulge = (i < bodies_per_galaxy / 10);
        
        if (in_bulge) {
            double r = galaxy_size_ * 0.15 * std::pow(radial_dist(gen), 1.0/3.0);
            double theta = angle_dist(gen);
            double phi = std::acos(2.0 * radial_dist(gen) - 1.0);
            
            body.position = center2 + Vector3D(
                r * std::sin(phi) * std::cos(theta),
                r * std::sin(phi) * std::sin(theta),
                r * std::cos(phi) * 0.3
            );
            
            // Estimate enclosed mass: M_enc(r) ≈ M_total * (r/R)^1.5 for bulge
            double r_normalized = r / galaxy_size_;
            double M_enc = estimated_galaxy_mass * std::pow(r_normalized, 1.5);
            double v_circular = std::sqrt(G * M_enc / std::max(r, 0.1));
            double v_mag = v_circular * (0.7 + 0.6 * radial_dist(gen));
            double v_theta = angle_dist(gen);
            body.velocity = velocity2 + Vector3D(
                v_mag * std::cos(v_theta),
                v_mag * std::sin(v_theta),
                (radial_dist(gen) - 0.5) * 0.05
            );
            
            body.mass = 2.0 + radial_dist(gen) * 3.0;
        } else {
            double r = galaxy_size_ * (0.2 + 0.8 * std::pow(radial_dist(gen), 0.7));
            double angle = angle_dist(gen) - 0.3 * std::log(r / (galaxy_size_ * 0.2)); // Opposite spiral direction
            double angle_offset = (radial_dist(gen) - 0.5) * 0.3;
            angle += angle_offset;
            
            body.position = center2 + Vector3D(
                r * std::cos(angle),
                r * std::sin(angle),
                (radial_dist(gen) - 0.5) * galaxy_size_ * 0.05
            );
            
            // Estimate enclosed mass: M_enc(r) ≈ M_total * (r/R)^1.2 for disk
            double r_normalized = r / galaxy_size_;
            double M_enc = estimated_galaxy_mass * std::pow(r_normalized, 1.2);
            double v_circular = std::sqrt(G * M_enc / std::max(r, 0.1));
            double v_angle = angle + M_PI / 2.0;
            double v_random = v_circular * (radial_dist(gen) - 0.5) * 0.15;
            
            body.velocity = velocity2 + Vector3D(
                v_circular * std::cos(v_angle) + v_random * std::cos(angle),
                v_circular * std::sin(v_angle) + v_random * std::sin(angle),
                (radial_dist(gen) - 0.5) * 0.1
            );
            
            double mass_factor = 1.0 - 0.5 * (r / galaxy_size_);
            body.mass = 0.1 + mass_factor * 1.5;
        }
        
        body.radius = std::cbrt(body.mass) * 0.1;
        body.color_r = galaxy2_r_;
        body.color_g = galaxy2_g_;
        body.color_b = galaxy2_b_;
        universe.add_body(std::move(body));
    }
}

} // namespace unisim

