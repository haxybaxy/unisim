#include "spiral_galaxy.hpp"
#include <random>
#include <cmath>

namespace unisim {

void SpiralGalaxyInitializer::initialize(Universe& universe, std::size_t num_bodies) {
    universe.clear();
    universe.reserve(num_bodies);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radial_dist(0.0, 1.0);
    
    // Estimate total mass for velocity calculations
    // Bulge: 10% of bodies, mass 2.0-5.0 each -> ~0.35 * num_bodies
    // Arms: 90% of bodies, mass 0.1-1.6 each -> ~0.765 * num_bodies
    double estimated_total_mass = num_bodies * 1.1; // Rough estimate
    double G = 1.0; // Gravitational constant (matches force computation)
    
    // Central bulge (10% of bodies, massive)
    std::size_t bulge_count = num_bodies / 10;
    double bulge_size = galaxy_size_ * 0.15;
    
    for (std::size_t i = 0; i < bulge_count; ++i) {
        Body body;
        
        // Random position in bulge (spherical distribution)
        double r = bulge_size * std::pow(radial_dist(gen), 1.0/3.0); // Uniform in volume
        double theta = angle_dist(gen);
        double phi = std::acos(2.0 * radial_dist(gen) - 1.0); // Uniform on sphere
        
        body.position.x = r * std::sin(phi) * std::cos(theta);
        body.position.y = r * std::sin(phi) * std::sin(theta);
        body.position.z = r * std::cos(phi) * 0.3; // Flattened bulge
        
        // Estimate enclosed mass: M_enc(r) ≈ M_total * (r/R)^1.5 for bulge
        double r_normalized = r / galaxy_size_;
        double M_enc = estimated_total_mass * std::pow(r_normalized, 1.5);
        // Circular velocity: v = sqrt(G * M_enc / r)
        double v_circular = std::sqrt(G * M_enc / std::max(r, 0.1));
        // Add some random component for non-circular orbits
        double v_mag = v_circular * (0.7 + 0.6 * radial_dist(gen));
        double v_theta = angle_dist(gen);
        body.velocity.x = v_mag * std::cos(v_theta);
        body.velocity.y = v_mag * std::sin(v_theta);
        body.velocity.z = (radial_dist(gen) - 0.5) * 0.05;
        
        // Massive bodies in bulge
        body.mass = 2.0 + radial_dist(gen) * 3.0;
        body.radius = std::cbrt(body.mass) * 0.15;
        body.color_r = color_r_;
        body.color_g = color_g_;
        body.color_b = color_b_;
        
        universe.add_body(std::move(body));
    }
    
    // Spiral arms (remaining bodies)
    std::size_t arm_count = num_bodies - bulge_count;
    
    for (std::size_t i = 0; i < arm_count; ++i) {
        Body body;
        
        // Radial distance (exponential falloff)
        double r = galaxy_size_ * (0.2 + 0.8 * std::pow(radial_dist(gen), 0.7));
        
        // Spiral arm assignment
        int arm = i % num_arms_;
        double base_angle = (2.0 * M_PI * arm) / num_arms_;
        
        // Spiral angle: tighter at center, looser at edges
        double spiral_angle = base_angle + arm_tightness_ * std::log(r / (galaxy_size_ * 0.2)) / std::log(galaxy_size_ * 0.8 / (galaxy_size_ * 0.2));
        
        // Add some randomness to spread out the arm
        double angle_offset = (radial_dist(gen) - 0.5) * 0.3;
        double angle = spiral_angle + angle_offset;
        
        // Position in disk plane
        body.position.x = r * std::cos(angle);
        body.position.y = r * std::sin(angle);
        body.position.z = (radial_dist(gen) - 0.5) * galaxy_size_ * 0.05; // Thin disk
        
        // Estimate enclosed mass for disk: M_enc(r) ≈ M_total * (r/R)^1.2
        double r_normalized = r / galaxy_size_;
        double M_enc = estimated_total_mass * std::pow(r_normalized, 1.2);
        // Circular velocity: v = sqrt(G * M_enc / r)
        double v_circular = std::sqrt(G * M_enc / std::max(r, 0.1));
        // Perpendicular to radius for circular orbit
        double v_angle = angle + M_PI / 2.0;
        // Add small random component for slight eccentricity
        double v_random = v_circular * (radial_dist(gen) - 0.5) * 0.15;
        
        body.velocity.x = v_circular * std::cos(v_angle) + v_random * std::cos(angle);
        body.velocity.y = v_circular * std::sin(v_angle) + v_random * std::sin(angle);
        body.velocity.z = (radial_dist(gen) - 0.5) * 0.1;
        
        // Mass decreases with distance from center
        double mass_factor = 1.0 - 0.5 * (r / galaxy_size_);
        body.mass = 0.1 + mass_factor * 1.5;
        body.radius = std::cbrt(body.mass) * 0.1;
        body.color_r = color_r_;
        body.color_g = color_g_;
        body.color_b = color_b_;
        
        universe.add_body(std::move(body));
    }
}

} // namespace unisim

