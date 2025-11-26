#include "renderer_2d.hpp"
#include <cairo.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace unisim {

void Renderer2D::render(cairo_t* cr, const Universe& universe, int width, int height,
                       const std::vector<std::vector<Vector3D>>* trajectories) {
    // Save the current state
    cairo_save(cr);
    
    // Clear background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    // Set up coordinate transformation
    // Map from world coordinates to screen coordinates
    double scale = std::min(width, height) / (2.0 * view_scale_);
    
    cairo_translate(cr, width / 2.0, height / 2.0);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -center_.x, -center_.y);

    // Draw grid before bodies
    if (show_grid_) {
        draw_grid(cr, scale, width, height);
    }

    // Draw coordinate axes
    draw_coordinate_axes(cr);

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
                if (first) {
                    cairo_move_to(cr, point.x, point.y);
                    first = false;
                } else {
                    cairo_line_to(cr, point.x, point.y);
                }
            }
            cairo_stroke(cr);
        }
    }

    // Draw bodies
    std::size_t index = 0;
    for (const auto& body : universe) {
        draw_body_with_glow(cr, body, index);
        
        // Draw velocity vector as arrow
        if (show_vectors_ && body.velocity.magnitude() > 0.01) {
            double radius = calculate_radius_from_mass(body.mass);
            draw_velocity_arrow(cr, body, radius);
        }
        index++;
    }

    // Restore transformation for UI elements
    cairo_restore(cr);
    
    // Draw scale bar (in screen coordinates)
    if (show_scale_bar_) {
        draw_scale_bar(cr, scale, width, height);
    }
}

void Renderer2D::draw_grid(cairo_t* cr, double scale, int width, int height) {
    // Calculate grid spacing based on view scale
    // Use adaptive spacing: aim for ~10-20 grid lines visible
    // Find a "nice" grid spacing that's a power of 10 times 1, 2, or 5
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
    
    // Find the range of visible coordinates
    double half_view = view_scale_;
    double min_x = center_.x - half_view;
    double max_x = center_.x + half_view;
    double min_y = center_.y - half_view;
    double max_y = center_.y + half_view;
    
    // Round to grid
    double start_x = std::floor(min_x / grid_spacing) * grid_spacing;
    double start_y = std::floor(min_y / grid_spacing) * grid_spacing;
    
    cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.5);
    cairo_set_line_width(cr, 0.02);
    
    // Vertical lines
    for (double x = start_x; x <= max_x; x += grid_spacing) {
        cairo_new_path(cr);
        cairo_move_to(cr, x, min_y);
        cairo_line_to(cr, x, max_y);
        cairo_stroke(cr);
    }
    
    // Horizontal lines
    for (double y = start_y; y <= max_y; y += grid_spacing) {
        cairo_new_path(cr);
        cairo_move_to(cr, min_x, y);
        cairo_line_to(cr, max_x, y);
        cairo_stroke(cr);
    }
}

void Renderer2D::draw_coordinate_axes(cairo_t* cr) {
    cairo_set_line_width(cr, 0.03);
    
    // X axis (red)
    cairo_new_path(cr);
    cairo_set_source_rgb(cr, 0.8, 0.2, 0.2);
    cairo_move_to(cr, -view_scale_, 0);
    cairo_line_to(cr, view_scale_, 0);
    cairo_stroke(cr);
    
    // Y axis (green)
    cairo_new_path(cr);
    cairo_set_source_rgb(cr, 0.2, 0.8, 0.2);
    cairo_move_to(cr, 0, -view_scale_);
    cairo_line_to(cr, 0, view_scale_);
    cairo_stroke(cr);
    
    // Origin marker
    cairo_new_path(cr);
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_arc(cr, 0, 0, 0.1, 0, 2 * M_PI);
    cairo_fill(cr);
}

void Renderer2D::draw_scale_bar(cairo_t* cr, double scale, int width, int height) {
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

void Renderer2D::draw_body_with_glow(cairo_t* cr, const Body& body, std::size_t index) {
    // Use the body's actual color values
    double r = body.color_r;
    double g = body.color_g;
    double b = body.color_b;
    
    // Calculate radius from mass (proportional to mass^(1/3))
    double radius = calculate_radius_from_mass(body.mass);
    
    // Draw glow effect (outer glow) if enabled
    if (show_glow_) {
        double glow_radius = radius * 2.5;
        cairo_pattern_t* glow_pattern = cairo_pattern_create_radial(
            body.position.x, body.position.y, radius,
            body.position.x, body.position.y, glow_radius
        );
        cairo_pattern_add_color_stop_rgba(glow_pattern, 0.0, r, g, b, 0.6);
        cairo_pattern_add_color_stop_rgba(glow_pattern, 0.5, r, g, b, 0.3);
        cairo_pattern_add_color_stop_rgba(glow_pattern, 1.0, r, g, b, 0.0);
        
        cairo_new_path(cr);
        cairo_set_source(cr, glow_pattern);
        cairo_arc(cr, body.position.x, body.position.y, glow_radius, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(glow_pattern);
    }
    
    // Draw main body (circle)
    cairo_pattern_t* body_pattern = cairo_pattern_create_radial(
        body.position.x - radius * 0.3, body.position.y - radius * 0.3, 0,
        body.position.x, body.position.y, radius
    );
    cairo_pattern_add_color_stop_rgb(body_pattern, 0.0, r * 1.3, g * 1.3, b * 1.3); // Bright center
    cairo_pattern_add_color_stop_rgb(body_pattern, 1.0, r, g, b); // Normal edge
    
    cairo_new_path(cr);
    cairo_set_source(cr, body_pattern);
    cairo_arc(cr, body.position.x, body.position.y, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(body_pattern);
}

void Renderer2D::draw_velocity_arrow(cairo_t* cr, const Body& body, double radius) {
    if (body.velocity.magnitude() < 0.01) return;
    
    // Scale vector length to be proportional to body size (smaller than before)
    double arrow_length = radius * 2.0; // Arrow length is 2x body radius
    Vector3D vel_normalized = body.velocity.normalized();
    Vector3D arrow_end = body.position + vel_normalized * arrow_length;
    
    // Arrow head size proportional to body radius
    double arrow_head_size = radius * 0.3;
    
    // Draw arrow shaft
    cairo_new_path(cr);
    cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.8);
    cairo_set_line_width(cr, radius * 0.15);
    cairo_move_to(cr, body.position.x, body.position.y);
    cairo_line_to(cr, arrow_end.x, arrow_end.y);
    cairo_stroke(cr);
    
    // Draw arrow head
    Vector3D perp = Vector3D(-vel_normalized.y, vel_normalized.x, 0.0); // Perpendicular vector
    
    Vector3D head_left = arrow_end - vel_normalized * arrow_head_size + perp * arrow_head_size * 0.5;
    Vector3D head_right = arrow_end - vel_normalized * arrow_head_size - perp * arrow_head_size * 0.5;
    
    cairo_new_path(cr);
    cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.8);
    cairo_move_to(cr, arrow_end.x, arrow_end.y);
    cairo_line_to(cr, head_left.x, head_left.y);
    cairo_line_to(cr, head_right.x, head_right.y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

void Renderer2D::get_body_color(std::size_t index, double& r, double& g, double& b) const {
    // Generate colors using a color wheel approach
    // Use golden angle for good color distribution
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

} // namespace unisim

