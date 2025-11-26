#pragma once

#include "renderer.hpp"

namespace unisim {

/**
 * @brief 2D renderer for top-down or side view
 */
class Renderer2D : public Renderer {
public:
    Renderer2D(double view_scale = 1.0, const Vector3D& center = Vector3D(0.0, 0.0, 0.0))
        : view_scale_(view_scale), center_(center) {}

    void render(cairo_t* cr, const Universe& universe, int width, int height,
                const std::vector<std::vector<Vector3D>>* trajectories = nullptr) override;

    const char* name() const override {
        return "2D";
    }

    void set_view_scale(double scale) {
        view_scale_ = scale;
    }

    void set_center(const Vector3D& center) {
        center_ = center;
    }

    void set_show_grid(bool show) {
        show_grid_ = show;
    }

    void set_show_scale_bar(bool show) {
        show_scale_bar_ = show;
    }

    void set_show_vectors(bool show) {
        show_vectors_ = show;
    }

    void set_show_trajectories(bool show) {
        show_trajectories_ = show;
    }

    void set_show_glow(bool show) {
        show_glow_ = show;
    }

    void set_mass_scale_factor(double factor) {
        mass_scale_factor_ = factor;
    }

private:
    double calculate_radius_from_mass(double mass) const {
        // Volume is proportional to r^3, so r is proportional to mass^(1/3)
        return mass_scale_factor_ * std::cbrt(mass);
    }
    void draw_grid(cairo_t* cr, double scale, int width, int height);
    void draw_scale_bar(cairo_t* cr, double scale, int width, int height);
    void draw_coordinate_axes(cairo_t* cr);
    void draw_body_with_glow(cairo_t* cr, const Body& body, std::size_t index);
    void draw_velocity_arrow(cairo_t* cr, const Body& body, double radius);
    void get_body_color(std::size_t index, double& r, double& g, double& b) const;

    double view_scale_;
    Vector3D center_;
    bool show_grid_{true};
    bool show_scale_bar_{true};
    bool show_vectors_{true};
    bool show_trajectories_{false};
    bool show_glow_{true};
    double mass_scale_factor_{0.1}; // Scaling factor for mass to radius conversion
};

} // namespace unisim

