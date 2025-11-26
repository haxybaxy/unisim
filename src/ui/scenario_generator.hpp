#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <functional>
#include <string>
#include <map>

namespace unisim {

/**
 * @brief Scenario generator dialog for configuring initial conditions
 */
class ScenarioGenerator {
public:
    ScenarioGenerator();
    ~ScenarioGenerator();

    // Show the dialog (non-blocking in GTK4)
    void show_dialog(GtkWindow* parent);
    
    // Check if dialog was accepted (call after show_dialog)
    bool was_accepted() const { return dialog_result_; }
    
    // Get the selected scenario name
    std::string get_selected_scenario() const;
    
    GtkWidget* dialog() const { return dialog_; }
    
    // Get parameters for a scenario
    struct ScenarioParams {
        // Common parameters
        int num_bodies{100};
        
        // Galaxy Collision specific
        double galaxy_size{15.0};
        double separation{30.0};
        float galaxy1_color_r{1.0f};
        float galaxy1_color_g{0.3f};
        float galaxy1_color_b{0.0f}; // Red
        float galaxy2_color_r{0.0f};
        float galaxy2_color_g{0.5f};
        float galaxy2_color_b{1.0f}; // Blue
        
        // Spiral Galaxy specific
        int num_arms{2};
        double arm_tightness{0.3};
        float spiral_color_r{1.0f};
        float spiral_color_g{0.5f};
        float spiral_color_b{0.0f}; // Orange
        
        // Elliptical Galaxy specific
        double ellipticity{0.5};
        float elliptical_color_r{0.8f};
        float elliptical_color_g{0.8f};
        float elliptical_color_b{0.2f}; // Yellow
        
        // Solar System specific
        int num_planets{8};
        bool include_asteroids{true};
        
        // Binary Star specific
        double binary_separation{10.0};
        int num_planets_binary{4};
        
        // Black Hole specific
        double black_hole_mass{10000.0};
        int num_orbiting_bodies{150};
        double system_radius{50.0};
    };
    
    ScenarioParams get_params() const;

private:
    static void on_scenario_changed(GObject* object, GParamSpec* pspec, gpointer user_data);
    static void on_generate_clicked(GtkWidget* widget, gpointer user_data);
    static void on_cancel_clicked(GtkWidget* widget, gpointer user_data);
    static void on_color_button_clicked(GtkWidget* widget, gpointer user_data);
    
    void update_parameter_panel();
    void recreate_dialog();
    void setup_galaxy_collision_params();
    void setup_spiral_galaxy_params();
    void setup_elliptical_galaxy_params();
    void setup_solar_system_params();
    void setup_binary_star_params();
    void setup_black_hole_params();
    
    GtkWidget* dialog_{nullptr};
    GtkWidget* scenario_dropdown_;
    GtkWidget* params_box_;
    GtkWidget* num_bodies_spin_;
    
    // Galaxy Collision
    GtkWidget* galaxy_size_spin_;
    GtkWidget* separation_spin_;
    GtkWidget* galaxy1_color_button_;
    GtkWidget* galaxy2_color_button_;
    GdkRGBA galaxy1_color_;
    GdkRGBA galaxy2_color_;
    
    // Spiral Galaxy
    GtkWidget* num_arms_spin_;
    GtkWidget* arm_tightness_spin_;
    GtkWidget* spiral_color_button_;
    GdkRGBA spiral_color_;
    
    // Elliptical Galaxy
    GtkWidget* ellipticity_spin_;
    GtkWidget* elliptical_color_button_;
    GdkRGBA elliptical_color_;
    
    // Solar System
    GtkWidget* num_planets_spin_;
    GtkWidget* include_asteroids_check_;
    
    // Binary Star
    GtkWidget* binary_separation_spin_;
    GtkWidget* num_planets_binary_spin_;
    
    // Black Hole
    GtkWidget* black_hole_mass_spin_;
    GtkWidget* num_orbiting_bodies_spin_;
    GtkWidget* system_radius_spin_;
    
    std::string selected_scenario_;
    bool dialog_result_{false};
};

} // namespace unisim

