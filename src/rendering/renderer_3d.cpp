#include "renderer_3d.hpp"
#include <cairo.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdio>

namespace unisim {

void Renderer3D::render(cairo_t* cr, const Universe& universe, int width, int height,
                       const std::vector<std::vector<Vector3D>>* trajectories) {
    // Save the current state
    cairo_save(cr);
    
    // Clear background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    // Base scale for orthographic projection
    double base_scale = std::min(width, height) / (2.0 * view_scale_);
    
    // Calculate camera direction
    Vector3D camera_dir = (camera_target_ - camera_pos_).normalized();
    double camera_distance = (camera_target_ - camera_pos_).magnitude();

    // Sort bodies by depth for proper rendering (back to front)
    std::vector<std::pair<double, const Body*>> sorted_bodies;
    for (const auto& body : universe) {
        Vector3D to_body = body.position - camera_pos_;
        double depth = to_body.dot(camera_dir);
        sorted_bodies.push_back({depth, &body});
    }
    
    std::sort(sorted_bodies.begin(), sorted_bodies.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; }); // Far to near

    // Draw coordinate axes first (always visible for debugging)
    draw_coordinate_axes(cr, base_scale, width, height);
    
    // Draw grid before bodies
    if (show_grid_) {
        draw_grid(cr, base_scale, width, height);
    }

    // Draw trajectories before bodies
    if (show_trajectories_ && trajectories) {
        for (std::size_t i = 0; i < trajectories->size() && i < universe.size(); ++i) {
            const auto& trajectory = (*trajectories)[i];
            if (trajectory.size() < 2) continue;
            
            // Use the body's actual color values
            const Body& body = universe[i];
            double r = body.color_r;
            double g = body.color_g;
            double b = body.color_b;
            
            cairo_new_path(cr);
            cairo_set_source_rgba(cr, r, g, b, 0.4);
            cairo_set_line_width(cr, 0.03);
            
            bool first = true;
            for (const auto& point : trajectory) {
                Vector3D projected = project_point(point, base_scale, width, height);
                if (first) {
                    cairo_move_to(cr, projected.x, projected.y);
                    first = false;
                } else {
                    cairo_line_to(cr, projected.x, projected.y);
                }
            }
            cairo_stroke(cr);
        }
    }

    // Draw bodies with perspective and depth cues
    std::size_t index = 0;
    for (const auto& [depth, body] : sorted_bodies) {
        Vector3D to_body = body->position - camera_pos_;
        double depth_dist = to_body.dot(camera_dir);
        
        // Skip bodies behind the camera
        if (depth_dist < 0.1) {
            index++;
            continue;
        }
        
        // Project point to screen
        Vector3D projected = project_point(body->position, base_scale, width, height);
        
        // Skip if projected outside reasonable bounds
        if (projected.x < -width || projected.x > width * 2 || 
            projected.y < -height || projected.y > height * 2) {
            index++;
            continue;
        }
        
        // Calculate radius from mass (proportional to mass^(1/3))
        double base_radius = calculate_radius_from_mass(body->mass);
        
        // Calculate size based on depth (perspective scaling)
        double size_scale = calculate_depth_scale(depth_dist);
        double world_radius = base_radius * size_scale;
        
        // Convert radius from world units to screen pixels
        // Apply perspective to radius as well (using depth_dist which is screen_z)
        double perspective_scale = 1.0;
        if (use_perspective_ && depth_dist > 0.1) {
            perspective_scale = perspective_factor_ / (perspective_factor_ + depth_dist);
        }
        double radius = world_radius * base_scale * perspective_scale;
        
        // Ensure minimum visible size
        if (radius < 0.5) {
            radius = 0.5;
        }
        
        // Special rendering for black holes
        if (body->is_blackhole) {
            draw_black_hole(cr, body->position, projected, radius, depth_dist, index, *body, base_scale, perspective_scale);
        } else {
            // Draw sphere with glow for normal bodies
            draw_sphere_with_glow(cr, projected, radius, depth_dist, index, universe[index]);
        }
        
        // Draw velocity vector as arrow
        if (show_vectors_ && body->velocity.magnitude() > 0.01) {
            double arrow_length = base_radius * 2.0 * size_scale; // Arrow length is 2x body radius
            Vector3D vel_normalized = body->velocity.normalized();
            Vector3D vel_end_3d = body->position + vel_normalized * (arrow_length / size_scale);
            Vector3D vel_projected = project_point(vel_end_3d, base_scale, width, height);
            draw_velocity_arrow(cr, projected, vel_projected, radius);
        }
        index++;
    }

    // Restore transformation for UI elements
    cairo_restore(cr);
    
    // Draw scale bar (in screen coordinates)
    if (show_scale_bar_) {
        draw_scale_bar(cr, base_scale, width, height);
    }
}

Vector3D Renderer3D::project_point(const Vector3D& point, double scale, int width, int height) const {
    Vector3D to_point = point - camera_pos_;
    Vector3D camera_dir = (camera_target_ - camera_pos_).normalized();
    
    // Build camera coordinate system
    // Forward is camera direction (towards target)
    Vector3D forward = camera_dir;
    
    // Right vector: use world up (0,1,0) to compute
    Vector3D world_up = Vector3D(0, 1, 0);
    Vector3D right = forward.cross(world_up);
    
    // If camera is looking straight up/down, use alternative up vector
    if (right.magnitude() < 0.1) {
        Vector3D world_right = Vector3D(1, 0, 0);
        right = world_up.cross(forward);
        if (right.magnitude() < 0.1) {
            right = world_right.cross(forward);
        }
    }
    right = right.normalized();
    
    // Up vector: perpendicular to both forward and right
    Vector3D up = right.cross(forward).normalized();
    
    // Project onto camera plane
    double screen_x = to_point.dot(right);
    double screen_y = to_point.dot(up);
    double screen_z = to_point.dot(forward);
    
    // Apply perspective if enabled
    if (use_perspective_ && screen_z > 0.1) {
        double perspective = perspective_factor_ / (perspective_factor_ + screen_z);
        screen_x *= perspective;
        screen_y *= perspective;
    }
    
    // Transform to screen coordinates
    double x = width / 2.0 + screen_x * scale;
    double y = height / 2.0 - screen_y * scale; // Flip Y axis
    
    return Vector3D(x, y, screen_z);
}

double Renderer3D::calculate_depth_scale(double depth) const {
    if (!use_perspective_) {
        return 1.0;
    }
    
    // Scale size based on depth (closer = larger)
    double scale = perspective_factor_ / (perspective_factor_ + depth);
    return std::clamp(scale, 0.1, 2.0); // Limit scaling
}

void Renderer3D::draw_grid(cairo_t* cr, double scale, int width, int height) {
    // Draw a simple grid on the XY plane (z=0)
    // Use adaptive spacing: aim for ~10-20 grid lines visible
    double target_spacing = view_scale_ / 10.0;
    
    // Find the order of magnitude
    double magnitude = std::pow(10.0, std::floor(std::log10(target_spacing)));
    double normalized = target_spacing / magnitude;
    
    // Round to nearest "nice" number (1, 2, 5, 10)
    double nice_normalized;
    if (normalized <= 1.5) {
        nice_normalized = 1.0;
    } else if (normalized <= 3.0) {
        nice_normalized = 2.0;
    } else if (normalized <= 7.0) {
        nice_normalized = 5.0;
    } else {
        nice_normalized = 10.0;
    }
    
    double grid_spacing = nice_normalized * magnitude;
    double half_view = view_scale_;
    
    cairo_set_source_rgba(cr, 0.2, 0.2, 0.3, 0.4);
    cairo_set_line_width(cr, 0.02);
    
    // Project grid lines
    for (double x = -half_view; x <= half_view; x += grid_spacing) {
        Vector3D p1 = project_point(Vector3D(x, -half_view, 0), scale, width, height);
        Vector3D p2 = project_point(Vector3D(x, half_view, 0), scale, width, height);
        cairo_new_path(cr);
        cairo_move_to(cr, p1.x, p1.y);
        cairo_line_to(cr, p2.x, p2.y);
        cairo_stroke(cr);
    }
    
    for (double y = -half_view; y <= half_view; y += grid_spacing) {
        Vector3D p1 = project_point(Vector3D(-half_view, y, 0), scale, width, height);
        Vector3D p2 = project_point(Vector3D(half_view, y, 0), scale, width, height);
        cairo_new_path(cr);
        cairo_move_to(cr, p1.x, p1.y);
        cairo_line_to(cr, p2.x, p2.y);
        cairo_stroke(cr);
    }
}

void Renderer3D::draw_coordinate_axes(cairo_t* cr, double scale, int width, int height) {
    // Draw coordinate axes from origin
    // X axis: Red
    // Y axis: Green  
    // Z axis: Blue
    
    double axis_length = view_scale_ * 0.5; // Half the view scale
    
    // Origin
    Vector3D origin = project_point(Vector3D(0, 0, 0), scale, width, height);
    
    // X axis (red)
    Vector3D x_end = project_point(Vector3D(axis_length, 0, 0), scale, width, height);
    cairo_new_path(cr);
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); // Red
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, origin.x, origin.y);
    cairo_line_to(cr, x_end.x, x_end.y);
    cairo_stroke(cr);
    
    // Y axis (green)
    Vector3D y_end = project_point(Vector3D(0, axis_length, 0), scale, width, height);
    cairo_new_path(cr);
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0); // Green
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, origin.x, origin.y);
    cairo_line_to(cr, y_end.x, y_end.y);
    cairo_stroke(cr);
    
    // Z axis (blue)
    Vector3D z_end = project_point(Vector3D(0, 0, axis_length), scale, width, height);
    cairo_new_path(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0); // Blue
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, origin.x, origin.y);
    cairo_line_to(cr, z_end.x, z_end.y);
    cairo_stroke(cr);
    
    // Draw labels
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    
    // X label
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_move_to(cr, x_end.x + 5, x_end.y);
    cairo_show_text(cr, "X");
    
    // Y label
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    cairo_move_to(cr, y_end.x + 5, y_end.y);
    cairo_show_text(cr, "Y");
    
    // Z label
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
    cairo_move_to(cr, z_end.x + 5, z_end.y);
    cairo_show_text(cr, "Z");
}

void Renderer3D::draw_scale_bar(cairo_t* cr, double scale, int width, int height) {
    // Draw scale bar in bottom-left corner (screen coordinates)
    double bar_length = view_scale_; // Length in world units
    double bar_pixels = bar_length * scale; // Length in pixels
    
    // Position: 20 pixels from left, 20 pixels from bottom
    double x = 20;
    double y = height - 20;
    
    // Draw bar
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_new_path(cr);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + bar_pixels, y);
    cairo_stroke(cr);
    
    // Draw tick marks
    cairo_new_path(cr);
    cairo_move_to(cr, x, y - 5);
    cairo_line_to(cr, x, y + 5);
    cairo_move_to(cr, x + bar_pixels, y - 5);
    cairo_line_to(cr, x + bar_pixels, y + 5);
    cairo_stroke(cr);
    
    // Draw label
    char label[64];
    if (bar_length >= 1.0) {
        snprintf(label, sizeof(label), "%.1f", bar_length);
    } else {
        snprintf(label, sizeof(label), "%.2f", bar_length);
    }
    
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, label, &extents);
    cairo_move_to(cr, x + bar_pixels / 2 - extents.width / 2, y - 8);
    cairo_show_text(cr, label);
}

void Renderer3D::draw_sphere_with_glow(cairo_t* cr, const Vector3D& projected, double radius, double depth, std::size_t index, const Body& body) {
    // Use the body's actual color values
    double r = body.color_r;
    double g = body.color_g;
    double b = body.color_b;
    
    // Adjust brightness based on depth
    double normalized_depth = depth / (view_scale_ * 2.0);
    normalized_depth = std::clamp(normalized_depth, -1.0, 1.0);
    double brightness = 0.4 + 0.6 * (1.0 - (normalized_depth + 1.0) / 2.0);
    brightness = std::clamp(brightness, 0.3, 1.0);
    
    r *= brightness;
    g *= brightness;
    b *= brightness;
    
    // Draw glow effect (outer glow) if enabled - draw BEFORE sphere so it shows around edges
    if (show_glow_) {
        double glow_radius = radius * 2.5;
        // Start glow from center for better visibility
        cairo_pattern_t* glow_pattern = cairo_pattern_create_radial(
            projected.x, projected.y, radius * 0.5,
            projected.x, projected.y, glow_radius
        );
        // Make glow more visible with higher opacity
        cairo_pattern_add_color_stop_rgba(glow_pattern, 0.0, r, g, b, 0.4);
        cairo_pattern_add_color_stop_rgba(glow_pattern, 0.4, r, g, b, 0.5);
        cairo_pattern_add_color_stop_rgba(glow_pattern, 0.7, r, g, b, 0.3);
        cairo_pattern_add_color_stop_rgba(glow_pattern, 1.0, r, g, b, 0.0);
        
        cairo_new_path(cr);
        cairo_set_source(cr, glow_pattern);
        cairo_arc(cr, projected.x, projected.y, glow_radius, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(glow_pattern);
    }
    
    // Draw sphere with 3D shading effect
    // Simulate sphere lighting from top-left
    double light_x = projected.x - radius * 0.4;
    double light_y = projected.y - radius * 0.4;
    
    cairo_pattern_t* sphere_pattern = cairo_pattern_create_radial(
        light_x, light_y, 0,
        projected.x, projected.y, radius
    );
    
    // Bright highlight
    cairo_pattern_add_color_stop_rgb(sphere_pattern, 0.0, 
        std::min(1.0, r * 1.5), std::min(1.0, g * 1.5), std::min(1.0, b * 1.5));
    // Mid-tone
    cairo_pattern_add_color_stop_rgb(sphere_pattern, 0.6, r, g, b);
    // Darker edge
    cairo_pattern_add_color_stop_rgb(sphere_pattern, 1.0, r * 0.6, g * 0.6, b * 0.6);
    
    cairo_new_path(cr);
    cairo_set_source(cr, sphere_pattern);
    cairo_arc(cr, projected.x, projected.y, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(sphere_pattern);
}

void Renderer3D::draw_black_hole(cairo_t* cr, const Vector3D& world_pos, const Vector3D& projected, double radius, double depth, std::size_t index, const Body& body, double base_scale, double perspective_scale) {
    // Calculate Schwarzschild radius
    double G = 1.0;
    double schwarzschild_r = body.schwarzschild_radius(G);
    
    // Calculate event horizon size on screen
    double size_scale = calculate_depth_scale(depth);
    double event_horizon_world = schwarzschild_r * size_scale;
    double event_horizon_screen = event_horizon_world * base_scale * perspective_scale;
    
    // Ensure minimum visible size for event horizon
    if (event_horizon_screen < radius * 1.2) {
        event_horizon_screen = radius * 1.2;
    }
    
    // Draw gravitational lensing effect (outermost glow - warped space visualization)
    double lensing_radius = event_horizon_screen * 2.5;
    cairo_pattern_t* lensing_pattern = cairo_pattern_create_radial(
        projected.x, projected.y, event_horizon_screen,
        projected.x, projected.y, lensing_radius
    );
    // Dark blue/purple glow representing warped spacetime
    cairo_pattern_add_color_stop_rgba(lensing_pattern, 0.0, 0.1, 0.1, 0.3, 0.6);
    cairo_pattern_add_color_stop_rgba(lensing_pattern, 0.5, 0.05, 0.05, 0.2, 0.4);
    cairo_pattern_add_color_stop_rgba(lensing_pattern, 1.0, 0.0, 0.0, 0.1, 0.0);
    
    cairo_new_path(cr);
    cairo_set_source(cr, lensing_pattern);
    cairo_arc(cr, projected.x, projected.y, lensing_radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(lensing_pattern);
    
    // Draw accretion disk (glowing ring around event horizon)
    double disk_inner = event_horizon_screen * 1.3;
    double disk_outer = event_horizon_screen * 2.0;
    
    // Inner hot region (white/blue)
    cairo_pattern_t* disk_pattern = cairo_pattern_create_radial(
        projected.x, projected.y, disk_inner,
        projected.x, projected.y, disk_outer
    );
    cairo_pattern_add_color_stop_rgba(disk_pattern, 0.0, 1.0, 1.0, 1.0, 0.9); // White hot center
    cairo_pattern_add_color_stop_rgba(disk_pattern, 0.3, 0.8, 0.9, 1.0, 0.8); // Blue-white
    cairo_pattern_add_color_stop_rgba(disk_pattern, 0.6, 0.6, 0.7, 0.9, 0.6); // Blue
    cairo_pattern_add_color_stop_rgba(disk_pattern, 1.0, 0.3, 0.4, 0.6, 0.3); // Dark blue/red
    
    cairo_new_path(cr);
    cairo_set_source(cr, disk_pattern);
    // Draw disk as filled ring
    cairo_arc(cr, projected.x, projected.y, disk_outer, 0, 2 * M_PI);
    cairo_arc_negative(cr, projected.x, projected.y, disk_inner, 2 * M_PI, 0);
    cairo_fill(cr);
    cairo_pattern_destroy(disk_pattern);
    
    // Draw event horizon (pure black sphere with subtle edge glow)
    // Outer edge glow (very subtle)
    cairo_pattern_t* horizon_glow = cairo_pattern_create_radial(
        projected.x, projected.y, event_horizon_screen * 0.9,
        projected.x, projected.y, event_horizon_screen * 1.1
    );
    cairo_pattern_add_color_stop_rgba(horizon_glow, 0.0, 0.0, 0.0, 0.0, 0.0); // Transparent
    cairo_pattern_add_color_stop_rgba(horizon_glow, 0.5, 0.05, 0.05, 0.15, 0.3); // Very subtle dark glow
    cairo_pattern_add_color_stop_rgba(horizon_glow, 1.0, 0.0, 0.0, 0.0, 0.0); // Transparent
    
    cairo_new_path(cr);
    cairo_set_source(cr, horizon_glow);
    cairo_arc(cr, projected.x, projected.y, event_horizon_screen * 1.1, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(horizon_glow);
    
    // Event horizon itself (pure black)
    cairo_new_path(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_arc(cr, projected.x, projected.y, event_horizon_screen, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Draw black hole singularity (tiny dark center, even darker than event horizon)
    double singularity_radius = std::max(radius * 0.3, 2.0);
    cairo_new_path(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_arc(cr, projected.x, projected.y, singularity_radius, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Add subtle inner glow for dramatic effect (simulating photon sphere)
    double photon_sphere = event_horizon_screen * 1.1;
    cairo_pattern_t* photon_pattern = cairo_pattern_create_radial(
        projected.x, projected.y, event_horizon_screen,
        projected.x, projected.y, photon_sphere
    );
    cairo_pattern_add_color_stop_rgba(photon_pattern, 0.0, 0.0, 0.0, 0.0, 0.0);
    cairo_pattern_add_color_stop_rgba(photon_pattern, 1.0, 0.2, 0.3, 0.5, 0.4); // Subtle blue glow
    
    cairo_new_path(cr);
    cairo_set_source(cr, photon_pattern);
    cairo_arc(cr, projected.x, projected.y, photon_sphere, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_pattern_destroy(photon_pattern);
}

void Renderer3D::get_body_color(std::size_t index, double& r, double& g, double& b) const {
    // Generate colors using a color wheel approach (same as 2D)
    double hue = (index * 137.508) / 360.0; // Golden angle approximation
    hue = hue - std::floor(hue); // Normalize to [0, 1)
    
    // Convert HSV to RGB
    double c = 0.8; // Chroma (saturation)
    double x = c * (1.0 - std::abs(std::fmod(hue * 6.0, 2.0) - 1.0));
    double m = 0.2; // Minimum brightness
    
    if (hue < 1.0/6.0) {
        r = c + m; g = x + m; b = m;
    } else if (hue < 2.0/6.0) {
        r = x + m; g = c + m; b = m;
    } else if (hue < 3.0/6.0) {
        r = m; g = c + m; b = x + m;
    } else if (hue < 4.0/6.0) {
        r = m; g = x + m; b = c + m;
    } else if (hue < 5.0/6.0) {
        r = x + m; g = m; b = c + m;
    } else {
        r = c + m; g = m; b = x + m;
    }
}

void Renderer3D::draw_velocity_arrow(cairo_t* cr, const Vector3D& start_proj, const Vector3D& end_proj, double body_radius) {
    // Calculate arrow direction
    double dx = end_proj.x - start_proj.x;
    double dy = end_proj.y - start_proj.y;
    double length = std::sqrt(dx * dx + dy * dy);
    
    if (length < 0.01) return;
    
    // Normalize direction
    double dir_x = dx / length;
    double dir_y = dy / length;
    
    // Arrow head size proportional to body radius
    double arrow_head_size = body_radius * 0.3;
    
    // Draw arrow shaft
    cairo_new_path(cr);
    cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.8);
    cairo_set_line_width(cr, body_radius * 0.15);
    cairo_move_to(cr, start_proj.x, start_proj.y);
    cairo_line_to(cr, end_proj.x, end_proj.y);
    cairo_stroke(cr);
    
    // Draw arrow head
    double perp_x = -dir_y;
    double perp_y = dir_x;
    
    double head_left_x = end_proj.x - dir_x * arrow_head_size + perp_x * arrow_head_size * 0.5;
    double head_left_y = end_proj.y - dir_y * arrow_head_size + perp_y * arrow_head_size * 0.5;
    double head_right_x = end_proj.x - dir_x * arrow_head_size - perp_x * arrow_head_size * 0.5;
    double head_right_y = end_proj.y - dir_y * arrow_head_size - perp_y * arrow_head_size * 0.5;
    
    cairo_new_path(cr);
    cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.8);
    cairo_move_to(cr, end_proj.x, end_proj.y);
    cairo_line_to(cr, head_left_x, head_left_y);
    cairo_line_to(cr, head_right_x, head_right_y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

} // namespace unisim

