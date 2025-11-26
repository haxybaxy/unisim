#include "viewport.hpp"
#include "../rendering/renderer_2d.hpp"
#include "../rendering/renderer_3d.hpp"
#include "../rendering/renderer_3d_opengl.hpp"
#include <cairo.h>
#include <gdk/gdkevents.h>
#include <epoxy/gl.h>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace unisim {

Viewport::Viewport() : universe_(nullptr), container_(nullptr), gl_area_(nullptr), using_opengl_(false) {
    // Create overlay as the main widget
    overlay_ = gtk_overlay_new();
    gtk_widget_set_hexpand(overlay_, TRUE);
    gtk_widget_set_vexpand(overlay_, TRUE);
    
    // Create container to hold rendering widgets
    container_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(container_, TRUE);
    gtk_widget_set_vexpand(container_, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(overlay_), container_);
    
    // Create info label
    info_label_ = gtk_label_new("");
    gtk_widget_add_css_class(info_label_, "viewport-info");
    gtk_label_set_xalign(GTK_LABEL(info_label_), 0.0);
    gtk_widget_set_halign(info_label_, GTK_ALIGN_START);
    gtk_widget_set_valign(info_label_, GTK_ALIGN_START);
    gtk_widget_set_margin_start(info_label_, 10);
    gtk_widget_set_margin_top(info_label_, 10);
    
    // Add label to overlay
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay_), info_label_);
    
    // Create metrics overlay drawing area (top-right corner)
    metrics_area_ = gtk_drawing_area_new();
    gtk_widget_set_halign(metrics_area_, GTK_ALIGN_END);
    gtk_widget_set_valign(metrics_area_, GTK_ALIGN_START);
    gtk_widget_set_margin_end(metrics_area_, 10);
    gtk_widget_set_margin_top(metrics_area_, 10);
    gtk_widget_set_size_request(metrics_area_, 230, 360); // Fixed size for metrics panel (larger for extended metrics)
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(metrics_area_),
        on_draw_metrics,
        this,
        nullptr
    );
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay_), metrics_area_);
    
    // Create navigation instructions label (bottom-left corner)
    nav_instructions_label_ = gtk_label_new("");
    gtk_widget_add_css_class(nav_instructions_label_, "nav-instructions");
    gtk_label_set_xalign(GTK_LABEL(nav_instructions_label_), 0.0);
    gtk_widget_set_halign(nav_instructions_label_, GTK_ALIGN_START);
    gtk_widget_set_valign(nav_instructions_label_, GTK_ALIGN_END);
    gtk_widget_set_margin_start(nav_instructions_label_, 10);
    gtk_widget_set_margin_bottom(nav_instructions_label_, 10);
    gtk_widget_set_visible(nav_instructions_label_, FALSE); // Initially hidden, shown when using 3D
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay_), nav_instructions_label_);
    
    // Create drawing area for Cairo rendering
    drawing_area_ = gtk_drawing_area_new();
    gtk_widget_set_can_focus(drawing_area_, TRUE);
    gtk_widget_set_hexpand(drawing_area_, TRUE);
    gtk_widget_set_vexpand(drawing_area_, TRUE);
    gtk_box_append(GTK_BOX(container_), drawing_area_);
    
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(drawing_area_),
        on_draw,
        this,
        nullptr
    );
    
    // Create GL area for OpenGL rendering (initially hidden)
    gl_area_ = gtk_gl_area_new();
    gtk_widget_set_can_focus(gl_area_, TRUE);
    gtk_widget_set_hexpand(gl_area_, TRUE);
    gtk_widget_set_vexpand(gl_area_, TRUE);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl_area_), TRUE);
    gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(gl_area_), FALSE);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(gl_area_), TRUE); // Enable automatic rendering
    gtk_box_append(GTK_BOX(container_), gl_area_);
    gtk_widget_set_visible(gl_area_, FALSE); // Initially hidden
    
    g_signal_connect(gl_area_, "render", G_CALLBACK(on_render_gl), this);
    g_signal_connect(gl_area_, "realize", G_CALLBACK(on_realize_gl), this);
    
    // Setup event controllers for overlay (captures events for children)
    setup_event_controllers(overlay_);
}

void Viewport::setup_event_controllers(GtkWidget* widget) {
    GtkGestureClick* click_controller = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    g_signal_connect(click_controller, "pressed", G_CALLBACK(on_button_press), this);
    g_signal_connect(click_controller, "released", G_CALLBACK(on_button_release), this);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click_controller));
    
    GtkEventController* motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "motion", G_CALLBACK(on_motion_notify), this);
    gtk_widget_add_controller(widget, motion_controller);
    
    GtkEventController* scroll_controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll_controller, "scroll", G_CALLBACK(on_scroll), this);
    gtk_widget_add_controller(widget, scroll_controller);
    
    GtkEventController* key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_press), this);
    gtk_widget_add_controller(widget, key_controller);
}

Viewport::~Viewport() = default;

void Viewport::set_renderer(std::shared_ptr<Renderer> renderer) {
    bool was_opengl = using_opengl_;
    using_opengl_ = (std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer) != nullptr);
    
    renderer_ = renderer;
    
    // Switch widget visibility if renderer type changed
    if (was_opengl != using_opengl_) {
        if (using_opengl_) {
            fprintf(stderr, "Switching to OpenGL renderer\n");
            gtk_widget_set_visible(drawing_area_, FALSE);
            gtk_widget_set_visible(gl_area_, TRUE);
            // Force realization of GL area if not already realized
            if (!gtk_widget_get_realized(gl_area_)) {
                // Note: In GTK4, we shouldn't manually realize if not in toplevel, 
                // but setting visible usually triggers realization when parent is visible.
            }
        } else {
            fprintf(stderr, "Switching to Cairo renderer\n");
            gtk_widget_set_visible(gl_area_, FALSE);
            gtk_widget_set_visible(drawing_area_, TRUE);
        }
    }
    
    update_renderer_view();
    queue_draw();
}

void Viewport::set_info(const std::string& backend, const std::string& integrator, const std::string& force_method) {
    backend_name_ = backend;
    integrator_name_ = integrator;
    force_name_ = force_method;
    
    std::stringstream ss;
    ss << "Backend: " << backend_name_ << "\n";
    ss << "Integrator: " << integrator_name_ << "\n";
    ss << "Force Method: " << force_name_;
    
    gtk_label_set_text(GTK_LABEL(info_label_), ss.str().c_str());
}

void Viewport::set_metrics(double fps, double cpu_usage, double gpu_usage, double memory_mb, 
                           double sim_time, uint64_t step_count, size_t num_bodies,
                           double avg_step_time_ms, double steps_per_sec, double bodies_per_sec,
                           uint32_t process_threads, uint32_t worker_threads,
                           double avg_worker_threads, uint32_t peak_worker_threads,
                           uint32_t logical_cores, uint32_t physical_cores,
                           uint32_t active_jobs) {
    metrics_fps_ = fps;
    metrics_cpu_ = cpu_usage;
    metrics_gpu_ = gpu_usage;
    metrics_memory_ = memory_mb;
    metrics_sim_time_ = sim_time;
    metrics_step_count_ = step_count;
    metrics_num_bodies_ = num_bodies;
    metrics_avg_step_time_ms_ = avg_step_time_ms;
    metrics_steps_per_sec_ = steps_per_sec;
    metrics_bodies_per_sec_ = bodies_per_sec;
    metrics_process_threads_ = process_threads;
    metrics_worker_threads_ = worker_threads;
    metrics_avg_worker_threads_ = avg_worker_threads;
    metrics_peak_worker_threads_ = peak_worker_threads;
    metrics_logical_cores_ = logical_cores;
    metrics_physical_cores_ = physical_cores;
    metrics_parallel_jobs_ = active_jobs;
    
    gtk_widget_queue_draw(metrics_area_);
}

void Viewport::set_system_info(const std::string& cpu_name, const std::string& gpu_name,
                               const std::string& metal_version, const std::string& cuda_version) {
    system_cpu_name_ = cpu_name;
    system_gpu_name_ = gpu_name;
    system_metal_version_ = metal_version;
    system_cuda_version_ = cuda_version;
    
    gtk_widget_queue_draw(metrics_area_);
}

void Viewport::queue_draw() {
    if (using_opengl_) {
        gtk_widget_queue_draw(gl_area_);
        // Also queue render for GL area
        gtk_gl_area_queue_render(GTK_GL_AREA(gl_area_));
    } else {
        gtk_widget_queue_draw(drawing_area_);
    }
}

void Viewport::reset_view() {
    view_scale_ = 10.0;
    pan_offset_ = Vector3D(0.0, 0.0, 0.0);
    camera_target_ = Vector3D(0.0, 0.0, 0.0);
    
    // Set isometric view angle
    camera_rotation_y_ = M_PI / 4.0;
    camera_rotation_x_ = std::atan(1.0 / std::sqrt(2.0));
    
    // Calculate initial camera position
    double distance = 10.0;
    double x = distance * std::sin(camera_rotation_y_) * std::cos(camera_rotation_x_);
    double y = distance * std::sin(camera_rotation_x_);
    double z = distance * std::cos(camera_rotation_y_) * std::cos(camera_rotation_x_);
    camera_pos_ = Vector3D(x, y, z);
    
    update_renderer_view();
    queue_draw();
}

void Viewport::zoom_in() {
    view_scale_ *= 0.9; // Zoom in (smaller scale = zoomed in)
    // Clamp zoom for 3D to prevent extreme values
    if (using_opengl_) {
        view_scale_ = std::clamp(view_scale_, 0.1, 1000.0);
    }
    update_renderer_view();
    queue_draw();
}

void Viewport::zoom_out() {
    view_scale_ *= 1.1; // Zoom out
    // Clamp zoom for 3D to prevent extreme values
    if (using_opengl_) {
        view_scale_ = std::clamp(view_scale_, 0.1, 1000.0);
    }
    update_renderer_view();
    queue_draw();
}

void Viewport::pan(double dx, double dy) {
    pan_offset_.x += dx;
    pan_offset_.y += dy;
    update_renderer_view();
    queue_draw();
}

void Viewport::update_nav_instructions() {
    if (!using_opengl_) {
        return;
    }
    
    // Create formatted instructions text with Pango markup
    std::string instructions = 
        "<span size='small' foreground='#E0E0E0'>"
        "<b>Navigation:</b>\n"
        "• <b>Left Drag</b> - Rotate camera\n"
        "• <b>Shift + Left Drag</b> - Pan\n"
        "• <b>Scroll</b> - Zoom\n"
        "• <b>Arrow Keys</b> - Rotate\n"
        "• <b>+/-</b> - Zoom\n"
        "• <b>R</b> - Reset view"
        "</span>";
    
    gtk_label_set_markup(GTK_LABEL(nav_instructions_label_), instructions.c_str());
}

void Viewport::update_renderer_view() {
    if (!renderer_) return;
    
    // Update 2D renderer
    if (auto renderer_2d = std::dynamic_pointer_cast<Renderer2D>(renderer_)) {
        renderer_2d->set_view_scale(view_scale_);
        renderer_2d->set_center(pan_offset_);
    }
    
    // Update 3D renderer
    if (auto renderer_3d = std::dynamic_pointer_cast<Renderer3D>(renderer_)) {
        renderer_3d->set_view_scale(view_scale_);
        renderer_3d->set_camera_target(camera_target_);
        
        // Calculate camera position based on rotation
        double distance = 10.0; // Fixed distance
        double x = distance * std::sin(camera_rotation_y_) * std::cos(camera_rotation_x_);
        double y = distance * std::sin(camera_rotation_x_);
        double z = distance * std::cos(camera_rotation_y_) * std::cos(camera_rotation_x_);
        
        Vector3D new_camera_pos = camera_target_ + Vector3D(x, y, z);
        renderer_3d->set_camera_position(new_camera_pos);
    }
    
    // Update OpenGL 3D renderer
    if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
        renderer_3d_gl->set_view_scale(view_scale_);
        
        // Calculate camera distance based on zoom (view_scale_)
        // Smaller view_scale_ = zoomed in = closer camera
        // Larger view_scale_ = zoomed out = farther camera
        double base_distance = 10.0;
        double distance = base_distance * (10.0 / view_scale_); // Inverse relationship
        
        // Calculate camera position based on rotation (spherical coordinates)
        double x = distance * std::sin(camera_rotation_y_) * std::cos(camera_rotation_x_);
        double y = distance * std::sin(camera_rotation_x_);
        double z = distance * std::cos(camera_rotation_y_) * std::cos(camera_rotation_x_);
        
        Vector3D new_camera_pos = camera_target_ + Vector3D(x, y, z);
        renderer_3d_gl->set_camera_position(new_camera_pos);
        renderer_3d_gl->set_camera_target(camera_target_);
    }
}

Vector3D Viewport::screen_to_world(double screen_x, double screen_y, int width, int height) const {
    double scale = std::min(width, height) / (2.0 * view_scale_);
    double world_x = (screen_x - width / 2.0) / scale + pan_offset_.x;
    double world_y = (height / 2.0 - screen_y) / scale + pan_offset_.y; // Flip Y
    return Vector3D(world_x, world_y, 0.0);
}

void Viewport::on_draw(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data) {
    static int draw_count = 0;
    draw_count++;
    
    Viewport* viewport = static_cast<Viewport*>(user_data);
    
    if (draw_count == 1) {
        fprintf(stderr, "on_draw called (Cairo) - count: %d\n", draw_count);
    }
    
    if (!viewport->universe_ || !viewport->renderer_) {
        return;
    }

    if (width <= 0 || height <= 0) {
        return;
    }
    
    // Render the universe directly using the Cairo context
    viewport->renderer_->render(cr, *viewport->universe_, width, height, viewport->trajectories_);
}

void Viewport::on_render_gl(GtkGLArea* area, GdkGLContext* context, gpointer user_data) {
    static int render_count = 0;
    render_count++;
    
    Viewport* viewport = static_cast<Viewport*>(user_data);
    
    if (!viewport->universe_ || !viewport->renderer_) {
        return;
    }
    
    // Check for errors
    if (gtk_gl_area_get_error(area) != nullptr) {
        fprintf(stderr, "  GL area error: %s\n", gtk_gl_area_get_error(area)->message);
        return;
    }
    
    int width = gtk_widget_get_width(GTK_WIDGET(area));
    int height = gtk_widget_get_height(GTK_WIDGET(area));
    
    if (width <= 0 || height <= 0) {
        return;
    }
    
    // Make the context current
    gtk_gl_area_make_current(area);
    
    // Check if OpenGL is initialized
    if (auto gl_renderer = std::dynamic_pointer_cast<Renderer3DOpenGL>(viewport->renderer_)) {
        // Ensure OpenGL is initialized
        gl_renderer->initialize_gl();
        
        // Render using OpenGL
        gl_renderer->render_gl(*viewport->universe_, width, height, viewport->trajectories_);
        
        // Check for OpenGL errors
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            // fprintf(stderr, "OpenGL error during render: 0x%x\n", err);
        }
    }
}

void Viewport::on_realize_gl(GtkGLArea* area, gpointer user_data) {
    Viewport* viewport = static_cast<Viewport*>(user_data);
    
    // Check for errors
    if (gtk_gl_area_get_error(area) != nullptr) {
        fprintf(stderr, "OpenGL context error: %s\n", gtk_gl_area_get_error(area)->message);
        return;
    }
    
    gtk_gl_area_make_current(area);
    
    // Check OpenGL version
    const char* version = (const char*)glGetString(GL_VERSION);
    if (version) {
        fprintf(stderr, "OpenGL version: %s\n", version);
    }
    
    // Initialize OpenGL renderer when context is ready
    if (auto gl_renderer = std::dynamic_pointer_cast<Renderer3DOpenGL>(viewport->renderer_)) {
        gl_renderer->initialize_gl();
    }
}

void Viewport::on_button_press(GtkGestureClick* gesture, int n_press, double x, double y, gpointer user_data) {
    Viewport* viewport = static_cast<Viewport*>(user_data);
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    GdkModifierType state = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
    
    if (button == GDK_BUTTON_PRIMARY) {
        // Left click
        viewport->is_dragging_ = true;
        viewport->last_mouse_x_ = x;
        viewport->last_mouse_y_ = y;
        
        // For 3D: Shift+Left = pan, Left = rotate
        if (viewport->using_opengl_) {
            viewport->is_rotating_ = !(state & GDK_SHIFT_MASK);
        }
    } else if (button == GDK_BUTTON_SECONDARY) {
        // Right click - rotate (for 3D) or pan (for 2D)
        if (viewport->using_opengl_) {
            viewport->is_rotating_ = true;
        } else {
            viewport->is_dragging_ = true;
        }
        viewport->last_mouse_x_ = x;
        viewport->last_mouse_y_ = y;
    }
}

void Viewport::on_button_release(GtkGestureClick* gesture, int n_press, double x, double y, gpointer user_data) {
    Viewport* viewport = static_cast<Viewport*>(user_data);
    viewport->is_dragging_ = false;
    viewport->is_rotating_ = false;
}

void Viewport::on_motion_notify(GtkEventControllerMotion* controller, double x, double y, gpointer user_data) {
    Viewport* viewport = static_cast<Viewport*>(user_data);
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
    
    if (viewport->is_dragging_ || viewport->is_rotating_) {
        if (viewport->using_opengl_) {
            // 3D navigation
            double dx = x - viewport->last_mouse_x_;
            double dy = y - viewport->last_mouse_y_;
            
            if (viewport->is_rotating_) {
                // Rotate camera around target (orbit)
                viewport->camera_rotation_y_ += dx * 0.01;
                viewport->camera_rotation_x_ += dy * 0.01;
                
                // Clamp X rotation to avoid gimbal lock
                viewport->camera_rotation_x_ = std::clamp(viewport->camera_rotation_x_, -M_PI / 2 + 0.1, M_PI / 2 - 0.1);
            } else {
                // Pan: move camera target perpendicular to view direction
                double pan_speed = viewport->view_scale_ * 0.01;
                
                // Calculate camera forward direction
                double distance = 10.0 * (10.0 / viewport->view_scale_);
                double fx = std::sin(viewport->camera_rotation_y_) * std::cos(viewport->camera_rotation_x_);
                double fy = std::sin(viewport->camera_rotation_x_);
                double fz = std::cos(viewport->camera_rotation_y_) * std::cos(viewport->camera_rotation_x_);
                Vector3D forward(fx, fy, fz);
                
                // Calculate right and up vectors
                Vector3D world_up(0, 1, 0);
                Vector3D right = world_up.cross(forward).normalized();
                Vector3D up = forward.cross(right).normalized();
                
                // Pan in screen space
                viewport->camera_target_ = viewport->camera_target_ + right * (-dx * pan_speed) + up * (dy * pan_speed);
            }
            
            viewport->update_renderer_view();
            viewport->queue_draw();
        } else {
            // 2D panning
            int width = gtk_widget_get_width(widget);
            int height = gtk_widget_get_height(widget);
            
            // Invert dx: dragging right should move world left (negative pan offset)
            // Invert dy: dragging down should move world up (negative pan offset)
            double dx = -(x - viewport->last_mouse_x_) / (std::min(width, height) / (2.0 * viewport->view_scale_));
            double dy = -(y - viewport->last_mouse_y_) / (std::min(width, height) / (2.0 * viewport->view_scale_));
            
            viewport->pan(dx, dy);
        }
        
        viewport->last_mouse_x_ = x;
        viewport->last_mouse_y_ = y;
    }
}

gboolean Viewport::on_scroll(GtkEventControllerScroll* controller, double dx, double dy, gpointer user_data) {
    Viewport* viewport = static_cast<Viewport*>(user_data);
    
    if (dy < 0) {
        viewport->zoom_in();
        return TRUE;
    } else if (dy > 0) {
        viewport->zoom_out();
        return TRUE;
    }
    
    return FALSE;
}

gboolean Viewport::on_key_press(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    Viewport* viewport = static_cast<Viewport*>(user_data);
    
    double pan_speed = viewport->view_scale_ * 0.1;
    
    switch (keyval) {
        case GDK_KEY_plus:
        case GDK_KEY_equal:
        case GDK_KEY_KP_Add:
            viewport->zoom_in();
            return TRUE;
            
        case GDK_KEY_minus:
        case GDK_KEY_underscore:
        case GDK_KEY_KP_Subtract:
            viewport->zoom_out();
            return TRUE;
            
        case GDK_KEY_Left:
        case GDK_KEY_h:
            if (viewport->using_opengl_) {
                // Rotate camera left
                viewport->camera_rotation_y_ -= 0.1;
                viewport->update_renderer_view();
            } else {
                viewport->pan(-pan_speed, 0);
            }
            return TRUE;
            
        case GDK_KEY_Right:
        case GDK_KEY_l:
            if (viewport->using_opengl_) {
                // Rotate camera right
                viewport->camera_rotation_y_ += 0.1;
                viewport->update_renderer_view();
            } else {
                viewport->pan(pan_speed, 0);
            }
            return TRUE;
            
        case GDK_KEY_Up:
        case GDK_KEY_k:
            if (viewport->using_opengl_) {
                // Rotate camera up
                viewport->camera_rotation_x_ += 0.1;
                viewport->camera_rotation_x_ = std::clamp(viewport->camera_rotation_x_, -M_PI / 2 + 0.1, M_PI / 2 - 0.1);
                viewport->update_renderer_view();
            } else {
                viewport->pan(0, pan_speed);
            }
            return TRUE;
            
        case GDK_KEY_Down:
        case GDK_KEY_j:
            if (viewport->using_opengl_) {
                // Rotate camera down
                viewport->camera_rotation_x_ -= 0.1;
                viewport->camera_rotation_x_ = std::clamp(viewport->camera_rotation_x_, -M_PI / 2 + 0.1, M_PI / 2 - 0.1);
                viewport->update_renderer_view();
            } else {
                viewport->pan(0, -pan_speed);
            }
            return TRUE;
            
        case GDK_KEY_r:
        case GDK_KEY_Home:
            viewport->reset_view();
            return TRUE;
            
        default:
            return FALSE;
    }
}

void Viewport::on_draw_metrics(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data) {
    Viewport* viewport = static_cast<Viewport*>(user_data);
    
    const double padding = 10.0;
    const double corner_radius = 6.0;
    const double border_width = 1.0;
    
    // Helper function to draw rounded rectangle
    auto draw_rounded_rect = [&](double x, double y, double w, double h, double radius) {
        cairo_new_sub_path(cr);
        cairo_arc(cr, x + w - radius, y + radius, radius, -M_PI / 2, 0);
        cairo_arc(cr, x + w - radius, y + h - radius, radius, 0, M_PI / 2);
        cairo_arc(cr, x + radius, y + h - radius, radius, M_PI / 2, M_PI);
        cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);
    };
    
    cairo_save(cr);
    
    // Outer glow (slightly larger)
    cairo_set_source_rgba(cr, 0.0, 0.8, 1.0, 0.1);
    draw_rounded_rect(padding - 2, padding - 2, width - 2 * padding + 4, height - 2 * padding + 4, corner_radius + 2);
    cairo_fill(cr);
    
    // Dark background
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.1, 0.9);
    draw_rounded_rect(padding, padding, width - 2 * padding, height - 2 * padding, corner_radius);
    cairo_fill(cr);
    
    // Border
    cairo_set_source_rgba(cr, 0.0, 0.9, 1.0, 0.6);
    cairo_set_line_width(cr, border_width);
    draw_rounded_rect(padding, padding, width - 2 * padding, height - 2 * padding, corner_radius);
    cairo_stroke(cr);
    
    cairo_restore(cr);
    
    // Text rendering
    cairo_save(cr);
    cairo_select_font_face(cr, "Monaco", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.5);
    
    double y = padding + 18.0;
    const double line_height = 16.0;
    const double label_x = padding + 8.0;
    const double value_x = width - padding - 8.0;
    
    // Helper lambda for drawing metric line with proper alignment
    auto draw_metric = [&](const char* label, const char* value, double r, double g, double b) {
        cairo_text_extents_t extents;
        
        // Label (left-aligned)
        cairo_set_source_rgba(cr, 0.7, 0.85, 1.0, 0.95);
        cairo_text_extents(cr, label, &extents);
        cairo_move_to(cr, label_x, y + extents.height);
        cairo_show_text(cr, label);
        
        // Value (right-aligned)
        cairo_set_source_rgba(cr, r, g, b, 1.0);
        cairo_text_extents(cr, value, &extents);
        cairo_move_to(cr, value_x - extents.width, y + extents.height);
        cairo_show_text(cr, value);
        
        y += line_height;
    };
    
    // Format metrics
    char fps_str[32], cpu_str[32], gpu_str[32], mem_str[32], time_str[32], steps_str[32], bodies_str[32];
    char step_time_str[32], steps_sec_str[32], bodies_sec_str[32];
    char proc_thr_str[32], worker_str[32], avg_worker_str[32], peak_worker_str[32], cores_str[32], jobs_str[32];
    
    snprintf(fps_str, sizeof(fps_str), "%.1f FPS", viewport->metrics_fps_);
    snprintf(cpu_str, sizeof(cpu_str), "%.1f%%", viewport->metrics_cpu_);
    snprintf(gpu_str, sizeof(gpu_str), "%.1f%%", viewport->metrics_gpu_);
    snprintf(mem_str, sizeof(mem_str), "%.1f MB", viewport->metrics_memory_);
    
    int hours = (int)(viewport->metrics_sim_time_ / 3600);
    int minutes = (int)((viewport->metrics_sim_time_ - hours * 3600) / 60);
    int seconds = (int)(viewport->metrics_sim_time_ - hours * 3600 - minutes * 60);
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hours, minutes, seconds);
    
    snprintf(steps_str, sizeof(steps_str), "%llu", (unsigned long long)viewport->metrics_step_count_);
    snprintf(bodies_str, sizeof(bodies_str), "%zu", viewport->metrics_num_bodies_);
    
    // Performance metrics
    if (viewport->metrics_avg_step_time_ms_ > 0.0) {
        snprintf(step_time_str, sizeof(step_time_str), "%.2f ms", viewport->metrics_avg_step_time_ms_);
    } else {
        snprintf(step_time_str, sizeof(step_time_str), "N/A");
    }
    
    if (viewport->metrics_steps_per_sec_ > 0.0) {
        snprintf(steps_sec_str, sizeof(steps_sec_str), "%.1f", viewport->metrics_steps_per_sec_);
    } else {
        snprintf(steps_sec_str, sizeof(steps_sec_str), "N/A");
    }
    
    if (viewport->metrics_bodies_per_sec_ > 0.0) {
        if (viewport->metrics_bodies_per_sec_ >= 1000000.0) {
            snprintf(bodies_sec_str, sizeof(bodies_sec_str), "%.1fM", viewport->metrics_bodies_per_sec_ / 1000000.0);
        } else if (viewport->metrics_bodies_per_sec_ >= 1000.0) {
            snprintf(bodies_sec_str, sizeof(bodies_sec_str), "%.1fK", viewport->metrics_bodies_per_sec_ / 1000.0);
        } else {
            snprintf(bodies_sec_str, sizeof(bodies_sec_str), "%.1f", viewport->metrics_bodies_per_sec_);
        }
    } else {
        snprintf(bodies_sec_str, sizeof(bodies_sec_str), "N/A");
    }
    
    // Thread/core metrics formatting
    if (viewport->metrics_process_threads_ > 0) {
        snprintf(proc_thr_str, sizeof(proc_thr_str), "%u", viewport->metrics_process_threads_);
    } else {
        snprintf(proc_thr_str, sizeof(proc_thr_str), "N/A");
    }
    
    if (viewport->metrics_worker_threads_ > 0) {
        snprintf(worker_str, sizeof(worker_str), "%u", viewport->metrics_worker_threads_);
    } else {
        snprintf(worker_str, sizeof(worker_str), "N/A");
    }
    
    if (viewport->metrics_avg_worker_threads_ > 0.0) {
        snprintf(avg_worker_str, sizeof(avg_worker_str), "%.1f", viewport->metrics_avg_worker_threads_);
    } else {
        snprintf(avg_worker_str, sizeof(avg_worker_str), "N/A");
    }
    
    if (viewport->metrics_peak_worker_threads_ > 0) {
        snprintf(peak_worker_str, sizeof(peak_worker_str), "%u", viewport->metrics_peak_worker_threads_);
    } else {
        snprintf(peak_worker_str, sizeof(peak_worker_str), "N/A");
    }
    
    if (viewport->metrics_logical_cores_ > 0) {
        snprintf(cores_str, sizeof(cores_str), "%uL/%uP",
                 viewport->metrics_logical_cores_,
                 viewport->metrics_physical_cores_ ? viewport->metrics_physical_cores_ : viewport->metrics_logical_cores_);
    } else {
        snprintf(cores_str, sizeof(cores_str), "N/A");
    }
    
    if (viewport->metrics_parallel_jobs_ > 0) {
        snprintf(jobs_str, sizeof(jobs_str), "%u", viewport->metrics_parallel_jobs_);
    } else {
        snprintf(jobs_str, sizeof(jobs_str), "0");
    }
    
    // Draw metrics with color coding
    draw_metric("FPS", fps_str, 0.0, 1.0, 0.5); // Green
    draw_metric("CPU", cpu_str, 1.0, 0.6, 0.0); // Orange
    draw_metric("GPU", gpu_str, 0.4, 0.8, 1.0); // Light blue
    draw_metric("MEM", mem_str, 0.8, 0.4, 1.0); // Purple
    draw_metric("TIME", time_str, 1.0, 1.0, 0.4); // Yellow
    draw_metric("STEPS", steps_str, 0.6, 1.0, 0.8); // Mint
    draw_metric("BODIES", bodies_str, 1.0, 0.8, 0.2); // Gold
    
    // Separator before performance metrics
    y += 6.0;
    cairo_set_source_rgba(cr, 0.0, 0.9, 1.0, 0.25);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, label_x, y);
    cairo_line_to(cr, value_x, y);
    cairo_stroke(cr);
    y += 10.0;
    
    // Performance section header
    cairo_set_font_size(cr, 9.5);
    cairo_text_extents_t perf_extents;
    cairo_set_source_rgba(cr, 0.5, 0.8, 1.0, 0.9);
    cairo_text_extents(cr, "PERFORMANCE", &perf_extents);
    cairo_move_to(cr, label_x, y + perf_extents.height);
    cairo_show_text(cr, "PERFORMANCE");
    y += line_height + 2.0;
    
    // Performance metrics
    draw_metric("STEP", step_time_str, 0.8, 1.0, 0.4); // Light green
    draw_metric("STEPS/S", steps_sec_str, 0.4, 1.0, 0.8); // Cyan
    draw_metric("BODIES/S", bodies_sec_str, 1.0, 0.4, 0.8); // Pink
    
    // Thread metrics section
    y += 6.0;
    cairo_set_source_rgba(cr, 0.0, 0.9, 1.0, 0.25);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, label_x, y);
    cairo_line_to(cr, value_x, y);
    cairo_stroke(cr);
    y += 10.0;
    
    cairo_set_font_size(cr, 9.5);
    cairo_text_extents_t threads_extents;
    cairo_set_source_rgba(cr, 0.5, 0.8, 1.0, 0.9);
    cairo_text_extents(cr, "THREADS", &threads_extents);
    cairo_move_to(cr, label_x, y + threads_extents.height);
    cairo_show_text(cr, "THREADS");
    y += line_height + 2.0;
    
    draw_metric("PROC THR", proc_thr_str, 1.0, 0.7, 0.4); // Orange
    draw_metric("WORKERS", worker_str, 0.4, 0.9, 1.0); // Light blue
    draw_metric("AVG WRK", avg_worker_str, 0.6, 1.0, 0.4); // Lime
    draw_metric("PEAK WRK", peak_worker_str, 1.0, 0.5, 0.8); // Pink
    draw_metric("CORES", cores_str, 0.9, 0.9, 0.5); // Soft yellow
    draw_metric("ACTIVE", jobs_str, 0.8, 0.6, 1.0); // Purple
    
    // Separator line
    y += 6.0;
    cairo_set_source_rgba(cr, 0.0, 0.9, 1.0, 0.25);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, label_x, y);
    cairo_line_to(cr, value_x, y);
    cairo_stroke(cr);
    y += 10.0;
    
    // System information section
    cairo_set_font_size(cr, 9.5);
    cairo_text_extents_t extents;
    cairo_set_source_rgba(cr, 0.5, 0.8, 1.0, 0.9);
    cairo_text_extents(cr, "SYSTEM", &extents);
    cairo_move_to(cr, label_x, y + extents.height);
    cairo_show_text(cr, "SYSTEM");
    y += line_height + 2.0;
    
    // Helper for system info (smaller font, truncate long names)
    auto draw_system_info = [&](const char* label, const std::string& value, double r, double g, double b) {
        // Truncate long values
        std::string display_value = value.empty() ? "N/A" : value;
        if (display_value.length() > 20) {
            display_value = display_value.substr(0, 17) + "...";
        }
        
        cairo_text_extents_t label_extents, value_extents;
        
        // Label
        cairo_set_source_rgba(cr, 0.6, 0.75, 1.0, 0.85);
        cairo_text_extents(cr, label, &label_extents);
        cairo_move_to(cr, label_x, y + label_extents.height);
        cairo_show_text(cr, label);
        
        // Value
        cairo_set_source_rgba(cr, r, g, b, 0.95);
        cairo_text_extents(cr, display_value.c_str(), &value_extents);
        cairo_move_to(cr, value_x - value_extents.width, y + value_extents.height);
        cairo_show_text(cr, display_value.c_str());
        
        y += line_height - 1.0;
    };
    
    draw_system_info("CPU", viewport->system_cpu_name_, 0.9, 0.95, 1.0); // Light blue-white
    draw_system_info("GPU", viewport->system_gpu_name_, 0.95, 0.8, 1.0); // Light purple
    draw_system_info("METAL", viewport->system_metal_version_, 0.6, 1.0, 0.8); // Mint
    draw_system_info("CUDA", viewport->system_cuda_version_, 1.0, 0.7, 0.5); // Orange
    
    cairo_restore(cr);
}

} // namespace unisim
