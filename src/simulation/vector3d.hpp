#pragma once

#include <cmath>
#include <array>

namespace unisim {

/**
 * @brief 3D vector for physics calculations
 */
struct Vector3D {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Vector3D() = default;
    Vector3D(double x, double y, double z) : x(x), y(y), z(z) {}
    
    // 2D constructor (z = 0)
    Vector3D(double x, double y) : x(x), y(y), z(0.0) {}

    Vector3D operator+(const Vector3D& other) const {
        return Vector3D(x + other.x, y + other.y, z + other.z);
    }

    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(x - other.x, y - other.y, z - other.z);
    }

    Vector3D operator*(double scalar) const {
        return Vector3D(x * scalar, y * scalar, z * scalar);
    }

    Vector3D operator/(double scalar) const {
        return Vector3D(x / scalar, y / scalar, z / scalar);
    }

    Vector3D& operator+=(const Vector3D& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vector3D& operator-=(const Vector3D& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vector3D& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    Vector3D& operator/=(double scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    double dot(const Vector3D& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3D cross(const Vector3D& other) const {
        return Vector3D(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    double magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    double magnitude_squared() const {
        return x * x + y * y + z * z;
    }

    Vector3D normalized() const {
        double mag = magnitude();
        if (mag > 0.0) {
            return *this / mag;
        }
        return Vector3D(0.0, 0.0, 0.0);
    }

    void normalize() {
        double mag = magnitude();
        if (mag > 0.0) {
            *this /= mag;
        }
    }
};

inline Vector3D operator*(double scalar, const Vector3D& vec) {
    return vec * scalar;
}

} // namespace unisim

