#pragma once

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "../simulation/universe.hpp"
#include "../rendering/renderer.hpp"
#include "../simulation/vector3d.hpp"
#include <memory>
#include <string>

namespace unisim {

/**
 * @brief GTK widget for rendering the simulation viewport with navigation
 */
class Viewport {
public:
    Viewport();
    ~Viewport();

    GtkWidget* widget() {
        return overlay_;
    }

    void set_universe(const Universe* universe) {
        universe_ = universe;
    }

    void set_renderer(std::shared_ptr<Renderer> renderer);

    void set_trajectories(const std::vector<std::vector<Vector3D>>* trajectories) {
        trajectories_ = trajectories;
    }
    
    void set_info(const std::string& backend, const std::string& integrator, const std::string& force_method);
    
    void set_metrics(double fps, double cpu_usage, double gpu_usage, double memory_mb, 
                     double sim_time, uint64_t step_count, size_t num_bodies,
                     double avg_step_time_ms = 0.0, double steps_per_sec = 0.0, double bodies_per_sec = 0.0);
    
    void set_system_info(const std::string& cpu_name, const std::string& gpu_name,
                        const std::string& metal_version, const std::string& cuda_version);

    void queue_draw();

    // Navigation controls
    void reset_view();
    void zoom_in();
    void zoom_out();
    void pan(double dx, double dy); // In world coordinates

private:
    static void on_draw(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
    static void on_render_gl(GtkGLArea* area, GdkGLContext* context, gpointer user_data);
    static void on_realize_gl(GtkGLArea* area, gpointer user_data);
    static void on_button_press(GtkGestureClick* gesture, int n_press, double x, double y, gpointer user_data);
    static void on_button_release(GtkGestureClick* gesture, int n_press, double x, double y, gpointer user_data);
    static void on_motion_notify(GtkEventControllerMotion* controller, double x, double y, gpointer user_data);
    static gboolean on_scroll(GtkEventControllerScroll* controller, double dx, double dy, gpointer user_data);
    static gboolean on_key_press(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
    
    void update_renderer_view();
    void setup_event_controllers(GtkWidget* widget);
    void update_nav_instructions();
    Vector3D screen_to_world(double screen_x, double screen_y, int width, int height) const;
    
    GtkWidget* overlay_; // Main overlay container
    GtkWidget* container_; // Container holding both widgets (inside overlay)
    GtkWidget* drawing_area_;
    GtkWidget* gl_area_; // For OpenGL rendering
    GtkWidget* info_label_; // Info overlay label
    GtkWidget* metrics_area_; // Metrics overlay drawing area
    GtkWidget* nav_instructions_label_; // Navigation instructions overlay
    
    bool using_opengl_{false};
    const Universe* universe_;
    std::shared_ptr<Renderer> renderer_;
    const std::vector<std::vector<Vector3D>>* trajectories_{nullptr};
    
    // Navigation state
    double view_scale_{10.0};
    Vector3D pan_offset_{0.0, 0.0, 0.0};
    
    // Mouse interaction state
    bool is_dragging_{false};
    double last_mouse_x_{0.0};
    double last_mouse_y_{0.0};
    bool is_rotating_{false}; // For 3D rotation
    
    // 3D camera state
    Vector3D camera_pos_{0.0, 0.0, 10.0};
    Vector3D camera_target_{0.0, 0.0, 0.0};
    double camera_rotation_x_{0.0}; // Rotation around X axis
    double camera_rotation_y_{0.0}; // Rotation around Y axis
    
    // Info strings
    std::string backend_name_;
    std::string integrator_name_;
    std::string force_name_;
    
    // Metrics data
    double metrics_fps_{0.0};
    double metrics_cpu_{0.0};
    double metrics_gpu_{0.0};
    double metrics_memory_{0.0};
    double metrics_sim_time_{0.0};
    uint64_t metrics_step_count_{0};
    size_t metrics_num_bodies_{0};
    double metrics_avg_step_time_ms_{0.0};
    double metrics_steps_per_sec_{0.0};
    double metrics_bodies_per_sec_{0.0};
    
    // System info
    std::string system_cpu_name_;
    std::string system_gpu_name_;
    std::string system_metal_version_;
    std::string system_cuda_version_;
    
    static void on_draw_metrics(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
};

} // namespace unisim
