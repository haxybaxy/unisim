#pragma once

#include "renderer.hpp"

namespace unisim {

/**
 * @brief 3D renderer with perspective projection and depth cues
 */
class Renderer3D : public Renderer {
public:
    Renderer3D(double view_scale = 1.0, const Vector3D& camera_pos = Vector3D(0.0, 0.0, 10.0))
        : view_scale_(view_scale), camera_pos_(camera_pos) {}

    void render(cairo_t* cr, const Universe& universe, int width, int height,
                const std::vector<std::vector<Vector3D>>* trajectories = nullptr) override;

    const char* name() const override {
        return "3D";
    }

    void set_camera_position(const Vector3D& pos) override {
        camera_pos_ = pos;
    }

    void set_camera_target(const Vector3D& target) override {
        camera_target_ = target;
    }

    void set_view_scale(double scale) {
        view_scale_ = scale;
    }

    void set_perspective(bool enable) {
        use_perspective_ = enable;
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
    void draw_coordinate_axes(cairo_t* cr, double scale, int width, int height);
    void draw_sphere_with_glow(cairo_t* cr, const Vector3D& projected, double radius, double depth, std::size_t index, const Body& body);
    void draw_velocity_arrow(cairo_t* cr, const Vector3D& start_proj, const Vector3D& end_proj, double body_radius);
    void get_body_color(std::size_t index, double& r, double& g, double& b) const;
    Vector3D project_point(const Vector3D& point, double scale, int width, int height) const;
    double calculate_depth_scale(double depth) const;

    double view_scale_;
    Vector3D camera_pos_;
    Vector3D camera_target_{0.0, 0.0, 0.0};
    bool use_perspective_{true};
    double perspective_factor_{500.0}; // Controls perspective strength
    bool show_grid_{true};
    bool show_scale_bar_{true};
    bool show_vectors_{true};
    bool show_trajectories_{false};
    bool show_glow_{true};
    double mass_scale_factor_{0.1}; // Scaling factor for mass to radius conversion
};

} // namespace unisim

