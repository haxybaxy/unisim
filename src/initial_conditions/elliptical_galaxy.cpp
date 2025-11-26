#include "elliptical_galaxy.hpp"
#include <random>
#include <cmath>

namespace unisim {

void EllipticalGalaxyInitializer::initialize(Universe& universe, std::size_t num_bodies) {
    universe.clear();
    universe.reserve(num_bodies);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radial_dist(0.0, 1.0);
    
    // Estimate total mass for velocity calculations
    // Core: 5% of bodies, mass 3.0-7.0 each -> ~0.25 * num_bodies
    // Halo: 95% of bodies, mass 0.2-2.0 each -> ~1.045 * num_bodies
    double estimated_total_mass = num_bodies * 1.3; // Rough estimate
    double G = 1.0; // Gravitational constant
    
    // Central massive core (5% of bodies)
    std::size_t core_count = num_bodies / 20;
    double core_size = galaxy_size_ * 0.1;
    
    for (std::size_t i = 0; i < core_count; ++i) {
        Body body;
        
        double r = core_size * std::pow(radial_dist(gen), 1.0/3.0);
        double theta = angle_dist(gen);
        double phi = std::acos(2.0 * radial_dist(gen) - 1.0);
        
        body.position.x = r * std::sin(phi) * std::cos(theta);
        body.position.y = r * std::sin(phi) * std::sin(theta);
        body.position.z = r * std::cos(phi);
        
        // Estimate enclosed mass: M_enc(r) ≈ M_total * (r/R)^1.8 for elliptical core
        double r_normalized = r / galaxy_size_;
        double M_enc = estimated_total_mass * std::pow(r_normalized, 1.8);
        // Circular velocity: v = sqrt(G * M_enc / r)
        double v_circular = std::sqrt(G * M_enc / std::max(r, 0.1));
        // Add random component for non-circular orbits
        double v_mag = v_circular * (0.6 + 0.8 * radial_dist(gen));
        double v_theta = angle_dist(gen);
        double v_phi = std::acos(2.0 * radial_dist(gen) - 1.0);
        body.velocity.x = v_mag * std::sin(v_phi) * std::cos(v_theta);
        body.velocity.y = v_mag * std::sin(v_phi) * std::sin(v_theta);
        body.velocity.z = v_mag * std::cos(v_phi);
        
        body.mass = 3.0 + radial_dist(gen) * 4.0;
        body.radius = std::cbrt(body.mass) * 0.15;
        body.color_r = color_r_;
        body.color_g = color_g_;
        body.color_b = color_b_;
        
        universe.add_body(std::move(body));
    }
    
    // Elliptical distribution (remaining bodies)
    std::size_t halo_count = num_bodies - core_count;
    
    for (std::size_t i = 0; i < halo_count; ++i) {
        Body body;
        
        // Radial distance with falloff
        double r = galaxy_size_ * std::pow(radial_dist(gen), 0.4);
        
        // Ellipsoidal distribution
        double theta = angle_dist(gen);
        double phi = std::acos(2.0 * radial_dist(gen) - 1.0);
        
        // Apply ellipticity (flatten along z-axis)
        double a = r; // Semi-major axis
        double b = a * (1.0 - ellipticity_); // Semi-minor axis
        
        body.position.x = a * std::sin(phi) * std::cos(theta);
        body.position.y = a * std::sin(phi) * std::sin(theta);
        body.position.z = b * std::cos(phi);
        
        // Estimate enclosed mass: M_enc(r) ≈ M_total * (r/R)^1.5 for elliptical halo
        double r_normalized = r / galaxy_size_;
        double M_enc = estimated_total_mass * std::pow(r_normalized, 1.5);
        // Circular velocity: v = sqrt(G * M_enc / r)
        double v_circular = std::sqrt(G * M_enc / std::max(r, 0.1));
        // Add variation for less organized orbits
        double v_mag = v_circular * (0.7 + 0.6 * radial_dist(gen));
        double v_theta = angle_dist(gen);
        double v_phi = std::acos(2.0 * radial_dist(gen) - 1.0);
        body.velocity.x = v_mag * std::sin(v_phi) * std::cos(v_theta);
        body.velocity.y = v_mag * std::sin(v_phi) * std::sin(v_theta);
        body.velocity.z = v_mag * std::cos(v_phi) * 0.5; // Less motion along z
        
        // Mass decreases with distance
        double mass_factor = 1.0 - 0.6 * (r / galaxy_size_);
        body.mass = 0.2 + mass_factor * 1.8;
        body.radius = std::cbrt(body.mass) * 0.1;
        body.color_r = color_r_;
        body.color_g = color_g_;
        body.color_b = color_b_;
        
        universe.add_body(std::move(body));
    }
}

} // namespace unisim

