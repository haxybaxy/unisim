#include "black_hole.hpp"
#include <random>
#include <cmath>
#include <algorithm>

namespace unisim {

void BlackHoleInitializer::initialize(Universe& universe, std::size_t num_bodies) {
    universe.clear();
    
    // Ensure we have at least 1 body (the black hole) + some orbiting bodies
    int actual_orbiting = std::min(static_cast<int>(num_bodies - 1), num_orbiting_bodies_);
    actual_orbiting = std::max(actual_orbiting, 10); // Minimum 10 orbiting bodies for cool effect
    
    std::size_t total_bodies = static_cast<std::size_t>(actual_orbiting) + 1;
    universe.reserve(total_bodies);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radial_dist(0.0, 1.0);
    std::uniform_real_distribution<double> mass_dist(0.3, 1.0);

    // Create central black hole
    Body blackhole;
    blackhole.position = Vector3D(0.0, 0.0, 0.0);
    blackhole.velocity = Vector3D(0.0, 0.0, 0.0);
    blackhole.mass = black_hole_mass_;
    blackhole.radius = 0.15; // Small visual radius
    blackhole.is_blackhole = true;
    blackhole.color_r = 0.0f; // Pure black
    blackhole.color_g = 0.0f;
    blackhole.color_b = 0.0f;
    universe.add_body(std::move(blackhole));

    // Calculate Schwarzschild radius for reference
    double G = 1.0;
    double schwarzschild_r = 2.0 * G * black_hole_mass_;
    
    // Create orbiting bodies in multiple layers
    double inner_radius = std::max(schwarzschild_r * 3.0, 5.0); // Well outside event horizon
    double outer_radius = system_radius_;
    
    // Layer 1: Accretion disk (inner hot region)
    int accretion_count = actual_orbiting / 3;
    for (int i = 0; i < accretion_count; ++i) {
        Body body;
        
        // Radial distance in accretion disk (thin, hot region)
        double r = inner_radius + (outer_radius * 0.3 - inner_radius) * std::pow(radial_dist(gen), 0.5);
        
        // Position in disk plane (minimal z variation)
        double theta = angle_dist(gen);
        body.position.x = r * std::cos(theta);
        body.position.y = r * std::sin(theta);
        body.position.z = (radial_dist(gen) - 0.5) * 0.5; // Very thin disk
        
        // Circular orbit velocity: v = sqrt(G * M / r)
        double v_circular = std::sqrt(G * black_hole_mass_ / std::max(r, inner_radius));
        
        // Velocity perpendicular to radius in disk plane
        double v_angle = theta + M_PI / 2.0;
        body.velocity.x = v_circular * std::cos(v_angle);
        body.velocity.y = v_circular * std::sin(v_angle);
        body.velocity.z = (radial_dist(gen) - 0.5) * 0.05;
        
        // Hot, massive bodies in accretion disk
        body.mass = 1.0 + mass_dist(gen) * 4.0;
        body.radius = std::cbrt(body.mass) * 0.12;
        
        // Hot colors (white/blue/cyan for accretion disk)
        double heat_factor = 1.0 - (r - inner_radius) / (outer_radius * 0.3 - inner_radius);
        body.color_r = static_cast<float>(0.7 + 0.3 * heat_factor);
        body.color_g = static_cast<float>(0.8 + 0.2 * heat_factor);
        body.color_b = static_cast<float>(0.9 + 0.1 * heat_factor);
        
        universe.add_body(std::move(body));
    }
    
    // Layer 2: Orbital debris and stars (intermediate region)
    int debris_count = actual_orbiting / 3;
    for (int i = 0; i < debris_count; ++i) {
        Body body;
        
        // Radial distance in intermediate region
        double r = outer_radius * 0.3 + (outer_radius * 0.7 - outer_radius * 0.3) * std::pow(radial_dist(gen), 0.7);
        
        // Random 3D position with slight preference for disk plane
        double theta = angle_dist(gen);
        double phi = std::acos(2.0 * radial_dist(gen) - 1.0); // Uniform on sphere
        
        body.position.x = r * std::sin(phi) * std::cos(theta);
        body.position.y = r * std::sin(phi) * std::sin(theta);
        body.position.z = r * std::cos(phi) * 0.4; // Flattened but 3D
        
        // Orbital velocity
        double v_circular = std::sqrt(G * black_hole_mass_ / std::max(r, 1.0));
        
        // Create perpendicular velocity vector for 3D orbit
        Vector3D radial_dir = body.position.normalized();
        
        // Find perpendicular vectors for velocity
        Vector3D perp1 = (std::abs(radial_dir.x) < 0.9) ? 
                         Vector3D(1.0, 0.0, 0.0).cross(radial_dir).normalized() :
                         Vector3D(0.0, 1.0, 0.0).cross(radial_dir).normalized();
        Vector3D perp2 = radial_dir.cross(perp1).normalized();
        
        // Random angle around the perpendicular plane
        double v_angle = angle_dist(gen);
        Vector3D velocity_dir = perp1 * std::cos(v_angle) + perp2 * std::sin(v_angle);
        
        // Add some random variation
        double v_mag = v_circular * (0.85 + 0.3 * radial_dist(gen));
        body.velocity = velocity_dir * v_mag;
        
        // Medium mass objects
        body.mass = 0.5 + mass_dist(gen) * 2.5;
        body.radius = std::cbrt(body.mass) * 0.1;
        
        // Cooler colors (yellow/orange/red)
        double cool_factor = (r - outer_radius * 0.3) / (outer_radius * 0.4);
        body.color_r = static_cast<float>(1.0 - 0.2 * cool_factor);
        body.color_g = static_cast<float>(0.7 - 0.3 * cool_factor);
        body.color_b = static_cast<float>(0.3 - 0.2 * cool_factor);
        
        universe.add_body(std::move(body));
    }
    
    // Layer 3: Outer stellar population (spiral pattern)
    int outer_count = actual_orbiting - accretion_count - debris_count;
    for (int i = 0; i < outer_count; ++i) {
        Body body;
        
        // Radial distance in outer region
        double r = outer_radius * 0.7 + outer_radius * 0.3 * std::pow(radial_dist(gen), 0.8);
        
        // Create spiral arm pattern (2 arms)
        int arm = i % 2;
        double base_angle = M_PI * arm;
        double spiral_tightness = 2.5;
        double spiral_angle = base_angle + spiral_tightness * std::log((r - outer_radius * 0.7) / (outer_radius * 0.3) + 1.0);
        
        // Add randomness to spread the arm
        double angle_offset = (radial_dist(gen) - 0.5) * 0.4;
        double theta = spiral_angle + angle_offset;
        
        // Slight vertical variation
        double phi = M_PI / 2.0 + (radial_dist(gen) - 0.5) * 0.3;
        
        body.position.x = r * std::sin(phi) * std::cos(theta);
        body.position.y = r * std::sin(phi) * std::sin(theta);
        body.position.z = r * std::cos(phi) * 0.2;
        
        // Orbital velocity
        double v_circular = std::sqrt(G * black_hole_mass_ / std::max(r, 1.0));
        
        // Mostly circular with slight radial component for spiral
        double v_radial = v_circular * 0.05 * (radial_dist(gen) - 0.5);
        double v_angle = theta + M_PI / 2.0;
        
        body.velocity.x = v_circular * std::cos(v_angle) + v_radial * std::cos(theta);
        body.velocity.y = v_circular * std::sin(v_angle) + v_radial * std::sin(theta);
        body.velocity.z = (radial_dist(gen) - 0.5) * 0.08;
        
        // Smaller masses in outer region
        body.mass = 0.3 + mass_dist(gen) * 1.2;
        body.radius = std::cbrt(body.mass) * 0.08;
        
        // Cooler, dimmer colors (red/orange)
        body.color_r = static_cast<float>(0.8 + 0.2 * radial_dist(gen));
        body.color_g = static_cast<float>(0.4 + 0.2 * radial_dist(gen));
        body.color_b = static_cast<float>(0.1 + 0.1 * radial_dist(gen));
        
        universe.add_body(std::move(body));
    }
}

} // namespace unisim

