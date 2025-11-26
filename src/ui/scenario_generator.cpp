#include "scenario_generator.hpp"
#include <iostream>
#include <cstdio>
#include <algorithm>

namespace unisim {

ScenarioGenerator::ScenarioGenerator() : dialog_(nullptr) {
    // Create dialog
    dialog_ = gtk_dialog_new();
    if (!dialog_ || !GTK_IS_DIALOG(dialog_)) {
        std::cerr << "Error: Failed to create dialog" << std::endl;
        return;
    }
    
    gtk_window_set_title(GTK_WINDOW(dialog_), "Scenario Generator");
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, 600);
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    
    // Get content area (deprecated in GTK4 but still works)
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
    if (!content_area) {
        std::cerr << "Error: Failed to get dialog content area" << std::endl;
        return;
    }
    
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 15);
    gtk_widget_set_margin_end(main_box, 15);
    gtk_widget_set_margin_top(main_box, 15);
    gtk_widget_set_margin_bottom(main_box, 15);
    gtk_box_append(GTK_BOX(content_area), main_box);
    
    // Scenario selection
    GtkWidget* scenario_label = gtk_label_new("Scenario:");
    gtk_box_append(GTK_BOX(main_box), scenario_label);
    
    GtkStringList* scenario_list = gtk_string_list_new(NULL);
    gtk_string_list_append(scenario_list, "Random");
    gtk_string_list_append(scenario_list, "Spiral Galaxy");
    gtk_string_list_append(scenario_list, "Elliptical Galaxy");
    gtk_string_list_append(scenario_list, "Galaxy Collision");
    gtk_string_list_append(scenario_list, "Solar System");
    gtk_string_list_append(scenario_list, "Binary Star System");
    gtk_string_list_append(scenario_list, "Black Hole System");
    
    scenario_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(scenario_list), NULL);
    g_signal_connect(scenario_dropdown_, "notify::selected-item", G_CALLBACK(on_scenario_changed), this);
    gtk_box_append(GTK_BOX(main_box), scenario_dropdown_);
    
    // Number of bodies (common to all)
    GtkWidget* num_bodies_label = gtk_label_new("Number of Bodies:");
    gtk_box_append(GTK_BOX(main_box), num_bodies_label);
    
    num_bodies_spin_ = gtk_spin_button_new_with_range(1, 100000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_bodies_spin_), 100);
    gtk_box_append(GTK_BOX(main_box), num_bodies_spin_);
    
    // Parameters panel (will be updated based on scenario)
    GtkWidget* params_label = gtk_label_new("Parameters:");
    gtk_label_set_xalign(GTK_LABEL(params_label), 0.0);
    gtk_box_append(GTK_BOX(main_box), params_label);
    
    params_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(params_box_, 10);
    gtk_box_append(GTK_BOX(main_box), params_box_);
    
    // Initialize colors
    galaxy1_color_ = {1.0f, 0.3f, 0.0f, 1.0f};
    galaxy2_color_ = {0.0f, 0.5f, 1.0f, 1.0f};
    spiral_color_ = {1.0f, 0.5f, 0.0f, 1.0f};
    elliptical_color_ = {0.8f, 0.8f, 0.2f, 1.0f};
    
    // Set initial scenario
    selected_scenario_ = "Random";
    update_parameter_panel();
    
    // Buttons
    GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(main_box), button_box);
    
    GtkWidget* generate_button = gtk_button_new_with_label("Generate");
    g_signal_connect(generate_button, "clicked", G_CALLBACK(on_generate_clicked), this);
    gtk_box_append(GTK_BOX(button_box), generate_button);
    
    GtkWidget* cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), this);
    gtk_box_append(GTK_BOX(button_box), cancel_button);
}

ScenarioGenerator::~ScenarioGenerator() {
    if (dialog_ && G_IS_OBJECT(dialog_)) {
        // Disconnect all signals first to avoid callbacks after destruction
        g_signal_handlers_disconnect_by_data(dialog_, this);
        if (GTK_IS_WINDOW(dialog_)) {
            gtk_window_destroy(GTK_WINDOW(dialog_));
        }
        dialog_ = nullptr;
    }
}

void ScenarioGenerator::recreate_dialog() {
    // Clean up old dialog if it exists
    if (dialog_ && G_IS_OBJECT(dialog_)) {
        g_signal_handlers_disconnect_by_data(dialog_, this);
        if (GTK_IS_WINDOW(dialog_)) {
            gtk_window_destroy(GTK_WINDOW(dialog_));
        }
        dialog_ = nullptr;
    }
    
    // Recreate dialog (same code as constructor)
    dialog_ = gtk_dialog_new();
    if (!dialog_ || !GTK_IS_DIALOG(dialog_)) {
        std::cerr << "Error: Failed to recreate dialog" << std::endl;
        return;
    }
    
    gtk_window_set_title(GTK_WINDOW(dialog_), "Scenario Generator");
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, 600);
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    
    // Get content area
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
    if (!content_area) {
        std::cerr << "Error: Failed to get dialog content area" << std::endl;
        return;
    }
    
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 15);
    gtk_widget_set_margin_end(main_box, 15);
    gtk_widget_set_margin_top(main_box, 15);
    gtk_widget_set_margin_bottom(main_box, 15);
    gtk_box_append(GTK_BOX(content_area), main_box);
    
    // Scenario selection
    GtkWidget* scenario_label = gtk_label_new("Scenario:");
    gtk_box_append(GTK_BOX(main_box), scenario_label);
    
    GtkStringList* scenario_list = gtk_string_list_new(NULL);
    gtk_string_list_append(scenario_list, "Random");
    gtk_string_list_append(scenario_list, "Spiral Galaxy");
    gtk_string_list_append(scenario_list, "Elliptical Galaxy");
    gtk_string_list_append(scenario_list, "Galaxy Collision");
    gtk_string_list_append(scenario_list, "Solar System");
    gtk_string_list_append(scenario_list, "Binary Star System");
    gtk_string_list_append(scenario_list, "Black Hole System");
    
    scenario_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(scenario_list), NULL);
    g_signal_connect(scenario_dropdown_, "notify::selected-item", G_CALLBACK(on_scenario_changed), this);
    gtk_box_append(GTK_BOX(main_box), scenario_dropdown_);
    
    // Number of bodies (common to all)
    GtkWidget* num_bodies_label = gtk_label_new("Number of Bodies:");
    gtk_box_append(GTK_BOX(main_box), num_bodies_label);
    
    num_bodies_spin_ = gtk_spin_button_new_with_range(1, 100000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_bodies_spin_), 100);
    gtk_box_append(GTK_BOX(main_box), num_bodies_spin_);
    
    // Parameters panel
    GtkWidget* params_label = gtk_label_new("Parameters:");
    gtk_label_set_xalign(GTK_LABEL(params_label), 0.0);
    gtk_box_append(GTK_BOX(main_box), params_label);
    
    params_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(params_box_, 10);
    gtk_box_append(GTK_BOX(main_box), params_box_);
    
    // Set initial scenario and update parameters
    selected_scenario_ = "Random";
    update_parameter_panel();
    
    // Buttons
    GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(main_box), button_box);
    
    GtkWidget* generate_button = gtk_button_new_with_label("Generate");
    g_signal_connect(generate_button, "clicked", G_CALLBACK(on_generate_clicked), this);
    gtk_box_append(GTK_BOX(button_box), generate_button);
    
    GtkWidget* cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), this);
    gtk_box_append(GTK_BOX(button_box), cancel_button);
}

void ScenarioGenerator::show_dialog(GtkWindow* parent) {
    // Check if dialog was destroyed (GTK4 may destroy it when closed)
    // We need to recreate it if it's invalid
    if (!dialog_ || !G_IS_OBJECT(dialog_)) {
        // Dialog was destroyed, recreate it
        recreate_dialog();
    }
    
    if (!dialog_ || !G_IS_OBJECT(dialog_)) {
        std::cerr << "Error: Failed to create dialog" << std::endl;
        return;
    }
    
    GtkWindow* dialog_window = GTK_WINDOW(dialog_);
    if (!dialog_window) {
        std::cerr << "Error: Failed to cast dialog to GtkWindow" << std::endl;
        return;
    }
    
    if (parent && GTK_IS_WINDOW(parent)) {
        gtk_window_set_transient_for(dialog_window, parent);
    }
    
    dialog_result_ = false;
    gtk_window_set_modal(dialog_window, TRUE);
    gtk_window_present(dialog_window);
}

std::string ScenarioGenerator::get_selected_scenario() const {
    return selected_scenario_;
}

ScenarioGenerator::ScenarioParams ScenarioGenerator::get_params() const {
    ScenarioParams params;
    int num_bodies = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_bodies_spin_));
    // Clamp to maximum of 100,000
    params.num_bodies = std::min(num_bodies, 100000);
    
    if (selected_scenario_ == "Galaxy Collision") {
        params.galaxy_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(galaxy_size_spin_));
        params.separation = gtk_spin_button_get_value(GTK_SPIN_BUTTON(separation_spin_));
        params.galaxy1_color_r = galaxy1_color_.red;
        params.galaxy1_color_g = galaxy1_color_.green;
        params.galaxy1_color_b = galaxy1_color_.blue;
        params.galaxy2_color_r = galaxy2_color_.red;
        params.galaxy2_color_g = galaxy2_color_.green;
        params.galaxy2_color_b = galaxy2_color_.blue;
    } else if (selected_scenario_ == "Spiral Galaxy") {
        params.num_arms = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_arms_spin_));
        params.arm_tightness = gtk_spin_button_get_value(GTK_SPIN_BUTTON(arm_tightness_spin_));
        params.spiral_color_r = spiral_color_.red;
        params.spiral_color_g = spiral_color_.green;
        params.spiral_color_b = spiral_color_.blue;
    } else if (selected_scenario_ == "Elliptical Galaxy") {
        params.ellipticity = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ellipticity_spin_));
        params.elliptical_color_r = elliptical_color_.red;
        params.elliptical_color_g = elliptical_color_.green;
        params.elliptical_color_b = elliptical_color_.blue;
    } else if (selected_scenario_ == "Solar System") {
        params.num_planets = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_planets_spin_));
        params.include_asteroids = gtk_check_button_get_active(GTK_CHECK_BUTTON(include_asteroids_check_));
    } else if (selected_scenario_ == "Binary Star System") {
        params.binary_separation = gtk_spin_button_get_value(GTK_SPIN_BUTTON(binary_separation_spin_));
        params.num_planets_binary = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_planets_binary_spin_));
    } else if (selected_scenario_ == "Black Hole System") {
        params.black_hole_mass = gtk_spin_button_get_value(GTK_SPIN_BUTTON(black_hole_mass_spin_));
        params.num_orbiting_bodies = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_orbiting_bodies_spin_));
        params.system_radius = gtk_spin_button_get_value(GTK_SPIN_BUTTON(system_radius_spin_));
    }
    
    return params;
}

void ScenarioGenerator::on_scenario_changed(GObject* object, GParamSpec* pspec, gpointer user_data) {
    ScenarioGenerator* self = static_cast<ScenarioGenerator*>(user_data);
    
    GtkStringObject* selected = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(GTK_DROP_DOWN(object)));
    if (selected) {
        self->selected_scenario_ = gtk_string_object_get_string(selected);
        self->update_parameter_panel();
    }
}

void ScenarioGenerator::on_generate_clicked(GtkWidget* widget, gpointer user_data) {
    ScenarioGenerator* self = static_cast<ScenarioGenerator*>(user_data);
    if (self && self->dialog_ && G_IS_OBJECT(self->dialog_)) {
        self->dialog_result_ = true;
        // Emit the response signal so the main window can handle it
        gtk_dialog_response(GTK_DIALOG(self->dialog_), GTK_RESPONSE_ACCEPT);
    }
}

void ScenarioGenerator::on_cancel_clicked(GtkWidget* widget, gpointer user_data) {
    ScenarioGenerator* self = static_cast<ScenarioGenerator*>(user_data);
    if (self && self->dialog_ && G_IS_OBJECT(self->dialog_)) {
        self->dialog_result_ = false;
        // Don't destroy the dialog, just hide it so we can reuse it
        gtk_widget_set_visible(self->dialog_, FALSE);
    }
}

struct ColorData {
    ScenarioGenerator* self;
    GdkRGBA* color;
};

void ScenarioGenerator::on_color_button_clicked(GtkWidget* widget, gpointer user_data) {
    ColorData* data = static_cast<ColorData*>(user_data);
    if (!data || !data->self || !data->color) return;
    
    // Cycle through preset colors
    GdkRGBA colors[] = {
        {1.0f, 0.3f, 0.0f, 1.0f}, // Red
        {0.0f, 0.5f, 1.0f, 1.0f}, // Blue
        {0.0f, 1.0f, 0.5f, 1.0f}, // Green
        {1.0f, 0.5f, 0.0f, 1.0f}, // Orange
        {0.8f, 0.8f, 0.2f, 1.0f}, // Yellow
        {1.0f, 0.0f, 1.0f, 1.0f}, // Magenta
        {0.0f, 1.0f, 1.0f, 1.0f}, // Cyan
    };
    
    // Find current color index
    int current_index = 0;
    for (int i = 0; i < 7; ++i) {
        float dr = data->color->red - colors[i].red;
        float dg = data->color->green - colors[i].green;
        float db = data->color->blue - colors[i].blue;
        if ((dr < 0.0f ? -dr : dr) < 0.01f &&
            (dg < 0.0f ? -dg : dg) < 0.01f &&
            (db < 0.0f ? -db : db) < 0.01f) {
            current_index = i;
            break;
        }
    }
    
    // Cycle to next color
    int next_index = (current_index + 1) % 7;
    *data->color = colors[next_index];
    
    // Update button label to show color name
    const char* color_names[] = {"Red", "Blue", "Green", "Orange", "Yellow", "Magenta", "Cyan"};
    char label[64];
    std::sprintf(label, "Color: %s", color_names[next_index]);
    gtk_button_set_label(GTK_BUTTON(widget), label);
}

void ScenarioGenerator::update_parameter_panel() {
    // Clear existing parameters
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(params_box_)) != NULL) {
        gtk_box_remove(GTK_BOX(params_box_), child);
    }
    
    if (selected_scenario_ == "Galaxy Collision") {
        setup_galaxy_collision_params();
    } else if (selected_scenario_ == "Spiral Galaxy") {
        setup_spiral_galaxy_params();
    } else if (selected_scenario_ == "Elliptical Galaxy") {
        setup_elliptical_galaxy_params();
    } else if (selected_scenario_ == "Solar System") {
        setup_solar_system_params();
    } else if (selected_scenario_ == "Binary Star System") {
        setup_binary_star_params();
    } else if (selected_scenario_ == "Black Hole System") {
        setup_black_hole_params();
    }
}

void ScenarioGenerator::setup_galaxy_collision_params() {
    // Galaxy size
    GtkWidget* label = gtk_label_new("Galaxy Size:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    galaxy_size_spin_ = gtk_spin_button_new_with_range(5.0, 50.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(galaxy_size_spin_), 15.0);
    gtk_box_append(GTK_BOX(params_box_), galaxy_size_spin_);
    
    // Separation
    label = gtk_label_new("Separation:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    separation_spin_ = gtk_spin_button_new_with_range(10.0, 100.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(separation_spin_), 30.0);
    gtk_box_append(GTK_BOX(params_box_), separation_spin_);
    
    // Galaxy 1 color
    label = gtk_label_new("Galaxy 1 Color:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    galaxy1_color_button_ = gtk_button_new_with_label("Color: Red");
    ColorData* data1 = new ColorData{this, &galaxy1_color_};
    g_signal_connect(galaxy1_color_button_, "clicked", G_CALLBACK(on_color_button_clicked), data1);
    gtk_box_append(GTK_BOX(params_box_), galaxy1_color_button_);
    
    // Galaxy 2 color
    label = gtk_label_new("Galaxy 2 Color:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    galaxy2_color_button_ = gtk_button_new_with_label("Color: Blue");
    ColorData* data2 = new ColorData{this, &galaxy2_color_};
    g_signal_connect(galaxy2_color_button_, "clicked", G_CALLBACK(on_color_button_clicked), data2);
    gtk_box_append(GTK_BOX(params_box_), galaxy2_color_button_);
}

void ScenarioGenerator::setup_spiral_galaxy_params() {
    // Number of arms (specific to spiral galaxies)
    GtkWidget* label = gtk_label_new("Number of Arms:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    num_arms_spin_ = gtk_spin_button_new_with_range(1, 8, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_arms_spin_), 2);
    gtk_box_append(GTK_BOX(params_box_), num_arms_spin_);
    
    // Arm tightness (specific to spiral galaxies)
    label = gtk_label_new("Arm Tightness:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    arm_tightness_spin_ = gtk_spin_button_new_with_range(0.1, 1.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(arm_tightness_spin_), 0.3);
    gtk_box_append(GTK_BOX(params_box_), arm_tightness_spin_);
    
    // Color (specific to spiral galaxies)
    label = gtk_label_new("Color:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    spiral_color_button_ = gtk_button_new_with_label("Color: Orange");
    ColorData* data = new ColorData{this, &spiral_color_};
    g_signal_connect(spiral_color_button_, "clicked", G_CALLBACK(on_color_button_clicked), data);
    gtk_box_append(GTK_BOX(params_box_), spiral_color_button_);
}

void ScenarioGenerator::setup_elliptical_galaxy_params() {
    // Ellipticity
    GtkWidget* label = gtk_label_new("Ellipticity:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    ellipticity_spin_ = gtk_spin_button_new_with_range(0.0, 1.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ellipticity_spin_), 0.5);
    gtk_box_append(GTK_BOX(params_box_), ellipticity_spin_);
    
    // Color
    label = gtk_label_new("Color:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    elliptical_color_button_ = gtk_button_new_with_label("Color: Yellow");
    ColorData* data = new ColorData{this, &elliptical_color_};
    g_signal_connect(elliptical_color_button_, "clicked", G_CALLBACK(on_color_button_clicked), data);
    gtk_box_append(GTK_BOX(params_box_), elliptical_color_button_);
}

void ScenarioGenerator::setup_solar_system_params() {
    // Number of planets
    GtkWidget* label = gtk_label_new("Number of Planets:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    num_planets_spin_ = gtk_spin_button_new_with_range(1, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_planets_spin_), 8);
    gtk_box_append(GTK_BOX(params_box_), num_planets_spin_);
    
    // Include asteroids
    include_asteroids_check_ = gtk_check_button_new_with_label("Include Asteroid Belt");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(include_asteroids_check_), TRUE);
    gtk_box_append(GTK_BOX(params_box_), include_asteroids_check_);
}

void ScenarioGenerator::setup_binary_star_params() {
    // Binary separation
    GtkWidget* label = gtk_label_new("Star Separation:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    binary_separation_spin_ = gtk_spin_button_new_with_range(5.0, 50.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(binary_separation_spin_), 10.0);
    gtk_box_append(GTK_BOX(params_box_), binary_separation_spin_);
    
    // Number of planets
    label = gtk_label_new("Number of Planets:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    num_planets_binary_spin_ = gtk_spin_button_new_with_range(0, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_planets_binary_spin_), 4);
    gtk_box_append(GTK_BOX(params_box_), num_planets_binary_spin_);
}

void ScenarioGenerator::setup_black_hole_params() {
    // Black hole mass
    GtkWidget* label = gtk_label_new("Black Hole Mass:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    black_hole_mass_spin_ = gtk_spin_button_new_with_range(1000.0, 100000.0, 1000.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(black_hole_mass_spin_), 10000.0);
    gtk_box_append(GTK_BOX(params_box_), black_hole_mass_spin_);
    
    // Number of orbiting bodies
    label = gtk_label_new("Number of Orbiting Bodies:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    num_orbiting_bodies_spin_ = gtk_spin_button_new_with_range(10, 1000, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_orbiting_bodies_spin_), 150);
    gtk_box_append(GTK_BOX(params_box_), num_orbiting_bodies_spin_);
    
    // System radius
    label = gtk_label_new("System Radius:");
    gtk_box_append(GTK_BOX(params_box_), label);
    
    system_radius_spin_ = gtk_spin_button_new_with_range(10.0, 200.0, 5.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(system_radius_spin_), 50.0);
    gtk_box_append(GTK_BOX(params_box_), system_radius_spin_);
}

} // namespace unisim

