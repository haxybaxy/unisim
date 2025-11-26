#include "renderer_3d_opengl.hpp"
#include <cairo.h>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>

namespace unisim {

// Vertex shader for spheres
const char* sphere_vertex_shader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 sphereCenter;
uniform float sphereRadius;

out vec3 WorldPos;
out vec3 LocalPos;

void main() {
    LocalPos = aPos;
    vec3 worldPos = sphereCenter + aPos * sphereRadius;
    WorldPos = worldPos;
    gl_Position = projection * view * model * vec4(worldPos, 1.0);
}
)";

// Fragment shader for glowing stars
const char* sphere_fragment_shader = R"(
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 LocalPos;

uniform vec3 objectColor;
uniform float glowIntensity;

void main() {
    // Simple 3D shading
    vec3 normal = normalize(LocalPos);
    vec3 lightDir = normalize(vec3(0.5, 0.8, 1.0)); // Light from front-top-right
    
    float diff = max(dot(normal, lightDir), 0.3); // Diffuse + Ambient
    vec3 finalColor = objectColor * diff;
    
    // Add a bit of emission/glow based on intensity
    finalColor += objectColor * 0.5 * glowIntensity;
    
    FragColor = vec4(finalColor, 1.0);
}
)";

// Simple shader for lines (trajectories, vectors, axes)
const char* line_vertex_shader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor; // Per-vertex color

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 globalColor;
uniform int usePerVertexColor;

out vec3 LineColor;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    if (usePerVertexColor == 1) {
        LineColor = aColor;
    } else {
        LineColor = globalColor;
    }
}
)";

const char* line_fragment_shader = R"(
#version 330 core
out vec4 FragColor;

in vec3 LineColor;
uniform float alpha;

void main() {
    FragColor = vec4(LineColor, alpha);
}
)";

Renderer3DOpenGL::Renderer3DOpenGL() {
    // Initialize matrices to identity
    std::memset(projection_matrix_, 0, sizeof(projection_matrix_));
    std::memset(view_matrix_, 0, sizeof(view_matrix_));
    std::memset(model_matrix_, 0, sizeof(model_matrix_));
    for (int i = 0; i < 4; i++) {
        projection_matrix_[i * 4 + i] = 1.0f;
        view_matrix_[i * 4 + i] = 1.0f;
        model_matrix_[i * 4 + i] = 1.0f;
    }
    // Adjust default scale for better visibility
    mass_scale_factor_ = 0.1;
    grid_width_ = 20.0; // Default grid spacing
}

Renderer3DOpenGL::~Renderer3DOpenGL() {
    cleanup_gl();
}

void Renderer3DOpenGL::render(cairo_t* cr, const Universe& universe, int width, int height,
                              const std::vector<std::vector<Vector3D>>* trajectories) {
    // This method is called from Cairo context, but we'll use OpenGL rendering
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 24);
    cairo_move_to(cr, 20, 40);
    cairo_show_text(cr, "3D OpenGL Renderer - Use GtkGLArea");
}

void Renderer3DOpenGL::initialize_gl() {
    if (gl_initialized_) return;
    
    GLenum err = glGetError(); // Clear error
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    shader_program_ = create_shader_program(sphere_vertex_shader, sphere_fragment_shader);
    if (shader_program_ == 0) return;
    
    line_shader_ = create_shader_program(line_vertex_shader, line_fragment_shader);
    if (line_shader_ == 0) return;
    
    create_sphere_mesh();
    
    gl_initialized_ = true;
}

void Renderer3DOpenGL::cleanup_gl() {
    if (!gl_initialized_) return;
    
    if (sphere_vao_) glDeleteVertexArrays(1, &sphere_vao_);
    if (sphere_vbo_) glDeleteBuffers(1, &sphere_vbo_);
    if (sphere_ebo_) glDeleteBuffers(1, &sphere_ebo_);
    if (shader_program_) glDeleteProgram(shader_program_);
    if (line_shader_) glDeleteProgram(line_shader_);
    if (stars_vao_) glDeleteVertexArrays(1, &stars_vao_);
    if (stars_vbo_) glDeleteBuffers(1, &stars_vbo_);
    
    stars_initialized_ = false;
    gl_initialized_ = false;
}

void Renderer3DOpenGL::render_gl(const Universe& universe, int width, int height,
                                 const std::vector<std::vector<Vector3D>>* trajectories) {
    if (!gl_initialized_) {
        initialize_gl();
        if (!gl_initialized_) return;
    }
    
    if (universe.size() == 0) return;
    
    // Deep Space Background (Pure Black)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    setup_projection_matrix(width, height);
    setup_view_matrix();
    
    if (show_starfield_) {
        render_starfield();
    }
    
    if (show_grid_) {
        render_grid();
    }
    
    if (show_axes_) {
        // Render coordinate axes (separate from grid)
    }
    
    // Trajectories
    if (show_trajectories_ && trajectories) {
        glDepthMask(GL_FALSE);
        for (std::size_t i = 0; i < trajectories->size() && i < universe.size(); ++i) {
            const auto& trajectory = (*trajectories)[i];
            if (trajectory.size() < 2) continue;
            
            // Use the body's actual color values
            const Body& body = universe[i];
            render_trajectory(trajectory, Vector3D(body.color_r, body.color_g, body.color_b));
        }
        glDepthMask(GL_TRUE);
    }
    
    // Bodies (Glowing Stars and Black Holes)
    glDisable(GL_CULL_FACE); // Ensure spheres are visible regardless of winding
    for (std::size_t i = 0; i < universe.size(); ++i) {
        const auto& body = universe[i];
        
        if (body.is_blackhole) {
            // Render black hole with special effects
            double G = 1.0;
            double schwarzschild_r = body.schwarzschild_radius(G);
            
            // Draw gravitational lensing effect (outermost)
            double lensing_radius = mass_scale_factor_ * std::pow(schwarzschild_r * 2.5, 0.4);
            Vector3D lensing_color(0.1, 0.1, 0.3);
            render_sphere(body.position, lensing_radius, lensing_color, 0.3);
            
            // Draw accretion disk (flattened torus-like structure)
            double disk_radius = mass_scale_factor_ * std::pow(schwarzschild_r * 2.0, 0.4);
            Vector3D disk_color(0.8, 0.9, 1.0); // White/blue hot
            // Render as flattened sphere to simulate disk
            render_sphere(body.position, disk_radius, disk_color, 0.7);
            
            // Draw event horizon
            double event_horizon_radius = mass_scale_factor_ * std::pow(schwarzschild_r, 0.4);
            Vector3D horizon_color(0.0, 0.0, 0.0); // Pure black
            render_sphere(body.position, event_horizon_radius, horizon_color, 0.1);
            
            // Draw singularity (tiny center)
            double singularity_radius = mass_scale_factor_ * std::pow(body.mass, 0.3) * 0.3;
            render_sphere(body.position, singularity_radius, horizon_color, 0.0);
        } else {
            // Normal body rendering
            // Scale for visibility
            double radius = mass_scale_factor_ * std::pow(body.mass, 0.4);
            if (radius < 0.05) radius = 0.05; // Ensure visible but small
            
            // Use the body's actual color values
            Vector3D color(body.color_r, body.color_g, body.color_b);
            
            render_sphere(body.position, radius, color, 1.0); // Always glow
            
            if (show_vectors_ && body.velocity.magnitude() > 0.01) {
                render_velocity_vector(body.position, body.velocity, radius, color);
            }
        }
    }
}

void Renderer3DOpenGL::setup_projection_matrix(int width, int height) {
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float fov_rad = fov_ * M_PI / 180.0f;
    float f = 1.0f / tanf(fov_rad / 2.0f);
    float zNear = 0.1f;
    float zFar = 2000.0f;
    
    std::memset(projection_matrix_, 0, sizeof(projection_matrix_));
    projection_matrix_[0] = f / aspect;
    projection_matrix_[5] = f;
    projection_matrix_[10] = (zFar + zNear) / (zNear - zFar);
    projection_matrix_[11] = -1.0f;
    projection_matrix_[14] = (2.0f * zFar * zNear) / (zNear - zFar);
}

void Renderer3DOpenGL::setup_view_matrix() {
    Vector3D forward = (camera_target_ - camera_pos_).normalized();
    Vector3D up = calculate_camera_up();
    Vector3D right = forward.cross(up).normalized();
    Vector3D real_up = right.cross(forward).normalized();
    
    std::memset(view_matrix_, 0, sizeof(view_matrix_));
    view_matrix_[0] = right.x;
    view_matrix_[1] = real_up.x;
    view_matrix_[2] = -forward.x;
    view_matrix_[4] = right.y;
    view_matrix_[5] = real_up.y;
    view_matrix_[6] = -forward.y;
    view_matrix_[8] = right.z;
    view_matrix_[9] = real_up.z;
    view_matrix_[10] = -forward.z;
    view_matrix_[15] = 1.0f;
    
    float tx = -(view_matrix_[0] * camera_pos_.x + view_matrix_[4] * camera_pos_.y + view_matrix_[8] * camera_pos_.z);
    float ty = -(view_matrix_[1] * camera_pos_.x + view_matrix_[5] * camera_pos_.y + view_matrix_[9] * camera_pos_.z);
    float tz = -(view_matrix_[2] * camera_pos_.x + view_matrix_[6] * camera_pos_.y + view_matrix_[10] * camera_pos_.z);
    
    view_matrix_[12] = tx;
    view_matrix_[13] = ty;
    view_matrix_[14] = tz;
}

Vector3D Renderer3DOpenGL::calculate_camera_up() const {
    Vector3D forward = (camera_target_ - camera_pos_).normalized();
    Vector3D world_up(0.0, 1.0, 0.0);
    if (std::abs(forward.dot(world_up)) > 0.99) return Vector3D(0.0, 0.0, 1.0);
    Vector3D right = forward.cross(world_up).normalized();
    return right.cross(forward).normalized();
}

void Renderer3DOpenGL::create_sphere_mesh() {
    const int segments = 32; // Reduced for performance, glow handles smoothness
    const int rings = 16;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    for (int ring = 0; ring <= rings; ++ring) {
        float theta = ring * M_PI / rings;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        for (int seg = 0; seg <= segments; ++seg) {
            float phi = seg * 2.0 * M_PI / segments;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z); // Pos
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z); // Normal (same for unit sphere)
        }
    }
    
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            int first = ring * (segments + 1) + seg;
            int second = first + segments + 1;
            indices.push_back(first); indices.push_back(second); indices.push_back(first + 1);
            indices.push_back(first + 1); indices.push_back(second); indices.push_back(second + 1);
        }
    }
    
    sphere_index_count_ = indices.size();
    glGenVertexArrays(1, &sphere_vao_);
    glGenBuffers(1, &sphere_vbo_);
    glGenBuffers(1, &sphere_ebo_);
    
    glBindVertexArray(sphere_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Renderer3DOpenGL::render_sphere(const Vector3D& position, double radius, const Vector3D& color, double glow_intensity) {
    glUseProgram(shader_program_);
    
    glUniformMatrix4fv(glGetUniformLocation(shader_program_, "model"), 1, GL_FALSE, model_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(shader_program_, "view"), 1, GL_FALSE, view_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(shader_program_, "projection"), 1, GL_FALSE, projection_matrix_);
    glUniform3f(glGetUniformLocation(shader_program_, "sphereCenter"), position.x, position.y, position.z);
    glUniform1f(glGetUniformLocation(shader_program_, "sphereRadius"), radius);
    glUniform3f(glGetUniformLocation(shader_program_, "objectColor"), color.x, color.y, color.z);
    glUniform1f(glGetUniformLocation(shader_program_, "glowIntensity"), glow_intensity);
    
    glBindVertexArray(sphere_vao_);
    glDrawElements(GL_TRIANGLES, sphere_index_count_, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Renderer3DOpenGL::render_trajectory(const std::vector<Vector3D>& trajectory, const Vector3D& color) {
    if (trajectory.size() < 2) return;
    glUseProgram(line_shader_);
    
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "model"), 1, GL_FALSE, model_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "view"), 1, GL_FALSE, view_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "projection"), 1, GL_FALSE, projection_matrix_);
    glUniform3f(glGetUniformLocation(line_shader_, "globalColor"), color.x, color.y, color.z);
    glUniform1i(glGetUniformLocation(line_shader_, "usePerVertexColor"), 0);
    glUniform1f(glGetUniformLocation(line_shader_, "alpha"), 0.8f); // Increased visibility
    
    std::vector<float> float_vertices;
    float_vertices.reserve(trajectory.size() * 3);
    for (const auto& v : trajectory) {
        float_vertices.push_back(static_cast<float>(v.x));
        float_vertices.push_back(static_cast<float>(v.y));
        float_vertices.push_back(static_cast<float>(v.z));
    }
    
    GLuint vbo, vao;
    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, float_vertices.size() * sizeof(float), float_vertices.data(), GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glLineWidth(2.0f); // Thicker trajectory
    glDrawArrays(GL_LINE_STRIP, 0, trajectory.size());
    
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

void Renderer3DOpenGL::render_velocity_vector(const Vector3D& position, const Vector3D& velocity, double body_radius, const Vector3D& color) {
    double arrow_length = body_radius * 5.0;
    Vector3D vel_normalized = velocity.normalized();
    Vector3D arrow_end = position + vel_normalized * arrow_length;
    
    // Main shaft
    float line_data[] = {
        (float)position.x, (float)position.y, (float)position.z,
        (float)arrow_end.x, (float)arrow_end.y, (float)arrow_end.z
    };
    
    // Arrow head
    float head_size = body_radius * 1.5;
    
    // We need two vectors orthogonal to velocity for the arrow head base
    Vector3D up(0, 1, 0);
    if (std::abs(vel_normalized.y) > 0.9) up = Vector3D(1, 0, 0);
    Vector3D right = vel_normalized.cross(up).normalized();
    Vector3D real_up = vel_normalized.cross(right).normalized();
    
    Vector3D head_base1 = arrow_end - vel_normalized * head_size + right * (head_size * 0.5);
    Vector3D head_base2 = arrow_end - vel_normalized * head_size - right * (head_size * 0.5);
    Vector3D head_base3 = arrow_end - vel_normalized * head_size + real_up * (head_size * 0.5);
    Vector3D head_base4 = arrow_end - vel_normalized * head_size - real_up * (head_size * 0.5);
    
    float arrow_head_data[] = {
        (float)arrow_end.x, (float)arrow_end.y, (float)arrow_end.z, (float)head_base1.x, (float)head_base1.y, (float)head_base1.z,
        (float)arrow_end.x, (float)arrow_end.y, (float)arrow_end.z, (float)head_base2.x, (float)head_base2.y, (float)head_base2.z,
        (float)arrow_end.x, (float)arrow_end.y, (float)arrow_end.z, (float)head_base3.x, (float)head_base3.y, (float)head_base3.z,
        (float)arrow_end.x, (float)arrow_end.y, (float)arrow_end.z, (float)head_base4.x, (float)head_base4.y, (float)head_base4.z
    };
    
    glUseProgram(line_shader_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "model"), 1, GL_FALSE, model_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "view"), 1, GL_FALSE, view_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "projection"), 1, GL_FALSE, projection_matrix_);
    glUniform3f(glGetUniformLocation(line_shader_, "globalColor"), 1.0f, 1.0f, 1.0f);
    glUniform1i(glGetUniformLocation(line_shader_, "usePerVertexColor"), 0);
    glUniform1f(glGetUniformLocation(line_shader_, "alpha"), 0.7f);
    
    GLuint vbo, vao;
    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    // Draw shaft
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_data), line_data, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, 2);
    
    // Draw arrow head
    glBufferData(GL_ARRAY_BUFFER, sizeof(arrow_head_data), arrow_head_data, GL_STREAM_DRAW);
    glDrawArrays(GL_LINES, 0, 8);
    
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

void Renderer3DOpenGL::render_starfield() {
    if (!stars_initialized_) {
        cached_stars_.reserve(num_stars_);
        std::srand(123);
        for (int i = 0; i < num_stars_; ++i) {
            double angle1 = (std::rand() / (double)RAND_MAX) * 2.0 * M_PI;
            double angle2 = acos(2.0 * (std::rand() / (double)RAND_MAX) - 1.0);
            double radius = 800.0 + (std::rand() / (double)RAND_MAX) * 400.0;
            cached_stars_.push_back(Vector3D(
                radius * sin(angle2) * cos(angle1),
                radius * sin(angle2) * sin(angle1),
                radius * cos(angle2)
            ));
        }
        
        std::vector<float> star_data;
        for (const auto& s : cached_stars_) {
            star_data.push_back((float)s.x); star_data.push_back((float)s.y); star_data.push_back((float)s.z);
            // Random brightness/color hint in normal
            float b = 0.5f + 0.5f * (std::rand() / (double)RAND_MAX);
            star_data.push_back(b); star_data.push_back(b); star_data.push_back(b);
        }
        
        glGenVertexArrays(1, &stars_vao_);
        glGenBuffers(1, &stars_vbo_);
        glBindVertexArray(stars_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, stars_vbo_);
        glBufferData(GL_ARRAY_BUFFER, star_data.size() * sizeof(float), star_data.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        stars_initialized_ = true;
    }
    
    glPointSize(1.8f);
    glUseProgram(line_shader_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "model"), 1, GL_FALSE, model_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "view"), 1, GL_FALSE, view_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "projection"), 1, GL_FALSE, projection_matrix_);
    glUniform1i(glGetUniformLocation(line_shader_, "usePerVertexColor"), 1); // Use varied brightness
    glUniform1f(glGetUniformLocation(line_shader_, "alpha"), 0.9f);
    
    glBindVertexArray(stars_vao_);
    glDrawArrays(GL_POINTS, 0, cached_stars_.size());
    glBindVertexArray(0);
}

void Renderer3DOpenGL::render_grid() {
    // Draw a very faint grid on the XZ plane (y=0)
    std::vector<float> grid_data;
    const float size = 100.0f;
    const float step = static_cast<float>(grid_width_); // Use configurable grid width
    const float colorVal = 0.2f;
    
    for (float i = -size; i <= size; i += step) {
        // X lines
        grid_data.push_back(-size); grid_data.push_back(0.0f); grid_data.push_back(i);
        grid_data.push_back(colorVal); grid_data.push_back(colorVal); grid_data.push_back(colorVal);
        grid_data.push_back(size); grid_data.push_back(0.0f); grid_data.push_back(i);
        grid_data.push_back(colorVal); grid_data.push_back(colorVal); grid_data.push_back(colorVal);
        
        // Z lines
        grid_data.push_back(i); grid_data.push_back(0.0f); grid_data.push_back(-size);
        grid_data.push_back(colorVal); grid_data.push_back(colorVal); grid_data.push_back(colorVal);
        grid_data.push_back(i); grid_data.push_back(0.0f); grid_data.push_back(size);
        grid_data.push_back(colorVal); grid_data.push_back(colorVal); grid_data.push_back(colorVal);
    }
    
    GLuint vbo, vao;
    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, grid_data.size() * sizeof(float), grid_data.data(), GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glUseProgram(line_shader_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "model"), 1, GL_FALSE, model_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "view"), 1, GL_FALSE, view_matrix_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_, "projection"), 1, GL_FALSE, projection_matrix_);
    glUniform1i(glGetUniformLocation(line_shader_, "usePerVertexColor"), 1);
    glUniform1f(glGetUniformLocation(line_shader_, "alpha"), 0.1f); // Very faint grid
    
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, grid_data.size() / 6);
    
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

GLuint Renderer3DOpenGL::compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        fprintf(stderr, "Shader Error: %s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Renderer3DOpenGL::create_shader_program(const char* vertex_source, const char* fragment_source) {
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex || !fragment) return 0;
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glDeleteProgram(program);
        return 0;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

void Renderer3DOpenGL::get_body_color(std::size_t index, double mass, double& r, double& g, double& b) const {
    // Procedural palette: Glowy Orange/Gold
    
    std::hash<size_t> hasher;
    size_t seed = hasher(index);
    std::mt19937 gen(seed);
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    // Base Orange-Red-Gold palette
    // R is always high for orange/gold
    r = 1.0;
    
    // G controls the slide from Red (low G) to Yellow (high G)
    // We want a nice distribution around orange (0.5)
    g = 0.25 + dis(gen) * 0.45; // 0.25 to 0.7
    
    // B adds a bit of whiteness/desaturation or keeps it saturated
    b = dis(gen) * 0.2;
}

} // namespace unisim
