#include "solar_system.hpp"
#include <cmath>
#include <algorithm>

namespace unisim {

void SolarSystemInitializer::initialize(Universe& universe, std::size_t num_bodies) {
    universe.clear();
    
    // Realistic solar system data (scaled for visualization)
    // Distances in AU, masses relative to Sun, velocities in AU/year
    
    struct Planet {
        const char* name;
        double distance;      // Semi-major axis in AU
        double mass;          // Relative to Sun (Sun = 1.0)
        double eccentricity;  // Orbital eccentricity
        double inclination;   // Orbital inclination in radians
        double arg_periapsis; // Argument of periapsis
    };
    
    // Simplified solar system (inner planets + Jupiter + Saturn)
    Planet planets[] = {
        {"Sun", 0.0, 1.0, 0.0, 0.0, 0.0},
        {"Mercury", 0.39, 1.66e-7, 0.206, 0.122, 0.0},
        {"Venus", 0.72, 2.45e-6, 0.007, 0.059, 0.0},
        {"Earth", 1.0, 3.0e-6, 0.017, 0.0, 0.0},
        {"Mars", 1.52, 3.23e-7, 0.094, 0.032, 0.0},
        {"Jupiter", 5.2, 9.55e-4, 0.049, 0.022, 0.0},
        {"Saturn", 9.5, 2.86e-4, 0.057, 0.043, 0.0},
    };
    
    int num_planets_available = sizeof(planets) / sizeof(planets[0]);
    int planets_to_create = std::min(num_planets_, num_planets_available);
    planets_to_create = std::min(planets_to_create, static_cast<int>(num_bodies));
    
    // If num_bodies is larger and asteroids enabled, add asteroid belt
    if (include_asteroids_ && num_bodies > static_cast<std::size_t>(planets_to_create)) {
        // Add some asteroids in the asteroid belt (between Mars and Jupiter)
        std::size_t asteroids = std::min(num_bodies - static_cast<std::size_t>(planets_to_create), static_cast<std::size_t>(20));
        for (std::size_t i = 0; i < asteroids; ++i) {
            Body body;
            double angle = (2.0 * M_PI * i) / asteroids;
            double distance = 2.5 + (i % 5) * 0.1; // Spread between 2.5-3.0 AU
            
            body.position.x = distance * std::cos(angle) * scale_factor_;
            body.position.y = distance * std::sin(angle) * scale_factor_;
            body.position.z = ((i % 3) - 1) * 0.1 * scale_factor_; // Slight inclination
            
            // Circular orbital velocity
            double v = std::sqrt(1.0 / distance); // Simplified
            body.velocity.x = -v * std::sin(angle);
            body.velocity.y = v * std::cos(angle);
            body.velocity.z = 0.0;
            
            body.mass = 1e-8; // Very small mass
            body.radius = std::cbrt(body.mass) * 0.05;
            
            universe.add_body(std::move(body));
        }
    }
    
    // Add planets
    for (int i = 0; i < planets_to_create; ++i) {
        Body body;
        const Planet& p = planets[i];
        
        if (i == 0) {
            // Sun at center
            body.position = Vector3D(0.0, 0.0, 0.0);
            body.velocity = Vector3D(0.0, 0.0, 0.0);
            body.mass = p.mass * 1000.0; // Scale up for visibility
        } else {
            // Planet in orbit
            // Use mean anomaly = 0 (start at periapsis)
            double r = p.distance * (1.0 - p.eccentricity); // Periapsis distance
            double angle = p.arg_periapsis;
            
            // Apply inclination
            double x = r * std::cos(angle);
            double y = r * std::sin(angle) * std::cos(p.inclination);
            double z = r * std::sin(angle) * std::sin(p.inclination);
            
            body.position = Vector3D(x, y, z) * scale_factor_;
            
            // Orbital velocity (simplified circular approximation)
            double v = std::sqrt(1.0 / p.distance); // Simplified Kepler
            double v_angle = angle + M_PI / 2.0;
            
            body.velocity.x = -v * std::sin(v_angle) * std::cos(p.inclination) * scale_factor_;
            body.velocity.y = v * std::cos(v_angle) * std::cos(p.inclination) * scale_factor_;
            body.velocity.z = v * std::cos(v_angle) * std::sin(p.inclination) * scale_factor_;
            
            body.mass = p.mass * 1000.0; // Scale up for visibility
        }
        
        body.radius = std::cbrt(body.mass) * 0.2; // Larger radius for planets
        universe.add_body(std::move(body));
    }
}

} // namespace unisim

