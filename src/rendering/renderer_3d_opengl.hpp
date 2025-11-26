#pragma once

#include "renderer.hpp"
#include <epoxy/gl.h>
#include <vector>
#include <memory>

namespace unisim {

/**
 * @brief True 3D OpenGL renderer with hardware acceleration
 */
class Renderer3DOpenGL : public Renderer {
public:
    Renderer3DOpenGL();
    ~Renderer3DOpenGL();

    void render(cairo_t* cr, const Universe& universe, int width, int height,
                const std::vector<std::vector<Vector3D>>* trajectories = nullptr) override;

    // OpenGL-specific render method (called from GtkGLArea)
    void render_gl(const Universe& universe, int width, int height,
                   const std::vector<std::vector<Vector3D>>* trajectories = nullptr);

    const char* name() const override {
        return "3D OpenGL";
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

    void set_show_starfield(bool show) {
        show_starfield_ = show;
    }

    void set_show_grid(bool show) {
        show_grid_ = show;
    }

    void set_grid_width(double width) {
        grid_width_ = width;
    }

    // Initialize OpenGL resources (call once when OpenGL context is ready)
    void initialize_gl();

    // Cleanup OpenGL resources
    void cleanup_gl();

private:
    void setup_projection_matrix(int width, int height);
    void setup_view_matrix();
    Vector3D calculate_camera_up() const;
    
    // Sphere rendering
    void create_sphere_mesh();
    void render_sphere(const Vector3D& position, double radius, const Vector3D& color, double glow_intensity);
    void render_trajectory(const std::vector<Vector3D>& trajectory, const Vector3D& color);
    void render_velocity_vector(const Vector3D& position, const Vector3D& velocity, double body_radius, const Vector3D& color);
    
    // Visual enhancements
    void render_starfield();
    void render_grid();
    void get_body_color(std::size_t index, double mass, double& r, double& g, double& b) const;
    
    // Shader compilation
    GLuint compile_shader(GLenum type, const char* source);
    GLuint create_shader_program(const char* vertex_source, const char* fragment_source);
    
    // OpenGL state
    bool gl_initialized_{false};
    GLuint sphere_vao_{0};
    GLuint sphere_vbo_{0};
    GLuint sphere_ebo_{0};
    GLuint shader_program_{0};
    GLuint trajectory_shader_{0};
    GLuint line_shader_{0};
    
    int sphere_index_count_{0};
    
    // Matrices
    float projection_matrix_[16];
    float view_matrix_[16];
    float model_matrix_[16];
    
    // Camera
    Vector3D camera_pos_{0.0, 0.0, 10.0};
    Vector3D camera_target_{0.0, 0.0, 0.0};
    double view_scale_{10.0};
    double fov_{60.0}; // Field of view in degrees
    
    // Rendering options
    bool show_vectors_{true};
    bool show_trajectories_{false};
    bool show_glow_{true};
    double mass_scale_factor_{0.1};
    
    // Visual settings
    bool show_starfield_{true};
    bool show_grid_{true};
    bool show_axes_{true};
    double grid_width_{1.0};
    int num_stars_{1000};
    
    // Cached star positions (generated once)
    std::vector<Vector3D> cached_stars_;
    GLuint stars_vao_{0};
    GLuint stars_vbo_{0};
    bool stars_initialized_{false};
};

} // namespace unisim

