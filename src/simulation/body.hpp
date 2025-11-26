#pragma once

#include "vector3d.hpp"

namespace unisim {

/**
 * @brief Represents a physical body in the n-body simulation
 */
struct Body {
    Vector3D position;
    Vector3D velocity;
    Vector3D acceleration{0.0, 0.0, 0.0};
    double mass{1.0};
    double radius{0.1}; // For visualization
    
    // Color for visualization (RGB, 0.0-1.0)
    float color_r{1.0f};
    float color_g{0.5f};
    float color_b{0.0f}; // Default orange

    Body() = default;
    Body(const Vector3D& pos, const Vector3D& vel, double m, double r = 0.1)
        : position(pos), velocity(vel), mass(m), radius(r) {}
    
    Body(const Vector3D& pos, const Vector3D& vel, double m, double r, float r_col, float g_col, float b_col)
        : position(pos), velocity(vel), mass(m), radius(r), color_r(r_col), color_g(g_col), color_b(b_col) {}
};

} // namespace unisim

