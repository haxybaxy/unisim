#include "control_panel.hpp"
#include <algorithm>

namespace unisim {

ControlPanel::ControlPanel() : playing_(false) {
    // Create scrolled window to handle overflow
    scrolled_window_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window_),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scrolled_window_), TRUE);
    
    // Create content box with padding (reduced for more compact layout)
    content_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(content_box_, 8);
    gtk_widget_set_margin_end(content_box_, 8);
    gtk_widget_set_margin_top(content_box_, 8);
    gtk_widget_set_margin_bottom(content_box_, 8);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window_), content_box_);
    
    // Keep reference to panel_ for backward compatibility
    panel_ = content_box_;

    // ===== SIMULATION CONTROL SECTION =====
    GtkWidget* sim_control_label = gtk_label_new("Simulation Control");
    gtk_label_set_xalign(GTK_LABEL(sim_control_label), 0.0);
    gtk_widget_add_css_class(sim_control_label, "title");
    gtk_box_append(GTK_BOX(panel_), sim_control_label);
    
    // Play/Pause button
    play_pause_button_ = gtk_button_new_with_label("Play");
    gtk_widget_add_css_class(play_pause_button_, "suggested-action");
    g_signal_connect(play_pause_button_, "clicked", G_CALLBACK(on_play_pause_clicked), this);
    gtk_box_append(GTK_BOX(panel_), play_pause_button_);

    // Reset button
    reset_button_ = gtk_button_new_with_label("Reset");
    g_signal_connect(reset_button_, "clicked", G_CALLBACK(on_reset_clicked), this);
    gtk_box_append(GTK_BOX(panel_), reset_button_);

    // Time step slider
    GtkWidget* dt_label = gtk_label_new("Time Step:");
    gtk_label_set_xalign(GTK_LABEL(dt_label), 0.0);
    gtk_box_append(GTK_BOX(panel_), dt_label);
    
    // Create a box for slider and value label
    GtkWidget* dt_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(panel_), dt_box);
    
    // Slider (logarithmic scale for better control)
    dt_scale_ = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.001, 1.0, 0.001);
    gtk_range_set_value(GTK_RANGE(dt_scale_), 0.01);
    gtk_widget_set_hexpand(dt_scale_, TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(dt_scale_), FALSE); // We'll show value in label
    g_signal_connect(dt_scale_, "value-changed", G_CALLBACK(on_dt_changed_cb), this);
    gtk_box_append(GTK_BOX(dt_box), dt_scale_);
    
    // Value label showing current time step (reduced width)
    dt_value_label_ = gtk_label_new("0.01");
    gtk_widget_set_size_request(dt_value_label_, 40, -1);
    gtk_label_set_xalign(GTK_LABEL(dt_value_label_), 1.0);
    gtk_box_append(GTK_BOX(dt_box), dt_value_label_);
    
    // Separator after simulation control
    GtkWidget* separator_sim = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(panel_), separator_sim);

    // ===== SIMULATION SETTINGS SECTION =====
    GtkWidget* sim_settings_label = gtk_label_new("Simulation Settings");
    gtk_label_set_xalign(GTK_LABEL(sim_settings_label), 0.0);
    gtk_widget_add_css_class(sim_settings_label, "title");
    gtk_box_append(GTK_BOX(panel_), sim_settings_label);

    // Number of bodies
    GtkWidget* num_bodies_label = gtk_label_new("Number of Bodies:");
    gtk_box_append(GTK_BOX(panel_), num_bodies_label);
    
    num_bodies_spin_ = gtk_spin_button_new_with_range(1, 100000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_bodies_spin_), 100);
    g_signal_connect(num_bodies_spin_, "value-changed", G_CALLBACK(on_num_bodies_changed_cb), this);
    gtk_box_append(GTK_BOX(panel_), num_bodies_spin_);

    // Backend selection
    GtkWidget* backend_label = gtk_label_new("Backend:");
    gtk_box_append(GTK_BOX(panel_), backend_label);
    
    backend_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
    GtkListItemFactory* backend_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(backend_factory, "setup", G_CALLBACK(backend_item_setup), this);
    g_signal_connect(backend_factory, "bind", G_CALLBACK(backend_item_bind), this);
    gtk_drop_down_set_factory(GTK_DROP_DOWN(backend_dropdown_), backend_factory);
    g_object_unref(backend_factory);
    g_signal_connect(backend_dropdown_, "notify::selected-item", G_CALLBACK(on_backend_changed_cb), this);
    gtk_box_append(GTK_BOX(panel_), backend_dropdown_);

    // Integrator selection
    GtkWidget* integrator_label = gtk_label_new("Integrator:");
    gtk_box_append(GTK_BOX(panel_), integrator_label);
    
    integrator_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
    g_signal_connect(integrator_dropdown_, "notify::selected-item", G_CALLBACK(on_integrator_changed_cb), this);
    gtk_box_append(GTK_BOX(panel_), integrator_dropdown_);

    // Force method selection
    GtkWidget* force_label = gtk_label_new("Force Method:");
    gtk_box_append(GTK_BOX(panel_), force_label);
    
    force_method_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
    g_signal_connect(force_method_dropdown_, "notify::selected-item", G_CALLBACK(on_force_method_changed_cb), this);
    gtk_box_append(GTK_BOX(panel_), force_method_dropdown_);

    // Renderer selection
    GtkWidget* renderer_label = gtk_label_new("Renderer:");
    gtk_box_append(GTK_BOX(panel_), renderer_label);
    
    renderer_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
    g_signal_connect(renderer_dropdown_, "notify::selected-item", G_CALLBACK(on_renderer_changed_cb), this);
    gtk_box_append(GTK_BOX(panel_), renderer_dropdown_);

    // Initializer selection
    GtkWidget* initializer_label = gtk_label_new("Initial Conditions:");
    gtk_box_append(GTK_BOX(panel_), initializer_label);
    
    initializer_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
    g_signal_connect(initializer_dropdown_, "notify::selected-item", G_CALLBACK(on_initializer_changed_cb), this);
    gtk_box_append(GTK_BOX(panel_), initializer_dropdown_);

    // Scenario Generator button
    GtkWidget* scenario_button = gtk_button_new_with_label("Scenario Generator");
    g_signal_connect(scenario_button, "clicked", G_CALLBACK(on_scenario_generator_clicked), this);
    gtk_box_append(GTK_BOX(panel_), scenario_button);
    
    // Separator for visualization controls
    GtkWidget* separator_viz_start = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(panel_), separator_viz_start);

    // ===== VISUALIZATION CONTROL SECTION =====
    GtkWidget* viz_control_label = gtk_label_new("Visualization");
    gtk_label_set_xalign(GTK_LABEL(viz_control_label), 0.0);
    gtk_widget_add_css_class(viz_control_label, "title");
    gtk_box_append(GTK_BOX(panel_), viz_control_label);

    // Show vectors checkbox
    GtkWidget* show_vectors_check = gtk_check_button_new_with_label("Show Velocity Vectors");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(show_vectors_check), TRUE);
    g_signal_connect(show_vectors_check, "toggled", G_CALLBACK(on_show_vectors_toggled_cb), this);
    gtk_box_append(GTK_BOX(panel_), show_vectors_check);

    // Show trajectories checkbox
    GtkWidget* show_trajectories_check = gtk_check_button_new_with_label("Show Trajectories");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(show_trajectories_check), FALSE);
    g_signal_connect(show_trajectories_check, "toggled", G_CALLBACK(on_show_trajectories_toggled_cb), this);
    gtk_box_append(GTK_BOX(panel_), show_trajectories_check);

    // Show glow checkbox
    GtkWidget* show_glow_check = gtk_check_button_new_with_label("Show Glow");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(show_glow_check), TRUE);
    g_signal_connect(show_glow_check, "toggled", G_CALLBACK(on_show_glow_toggled_cb), this);
    gtk_box_append(GTK_BOX(panel_), show_glow_check);
    
    // Separator for 3D-specific controls
    GtkWidget* separator_3d = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(panel_), separator_3d);
    
    // ===== 3D VIEW CONTROL SECTION =====
    GtkWidget* view3d_control_label = gtk_label_new("3D View");
    gtk_label_set_xalign(GTK_LABEL(view3d_control_label), 0.0);
    gtk_widget_add_css_class(view3d_control_label, "title");
    gtk_box_append(GTK_BOX(panel_), view3d_control_label);
    
    // Reset camera button
    GtkWidget* reset_camera_button = gtk_button_new_with_label("Reset Camera");
    g_signal_connect(reset_camera_button, "clicked", G_CALLBACK(on_reset_camera_clicked), this);
    gtk_box_append(GTK_BOX(panel_), reset_camera_button);
    
    // Body size control
    GtkWidget* body_size_label = gtk_label_new("Body Size:");
    gtk_box_append(GTK_BOX(panel_), body_size_label);
    
    body_size_spin_ = gtk_spin_button_new_with_range(0.01, 10.0, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(body_size_spin_), 0.1);
    g_signal_connect(body_size_spin_, "value-changed", G_CALLBACK(on_body_size_changed_cb), this);
    gtk_box_append(GTK_BOX(panel_), body_size_spin_);
    
    // Show starfield checkbox
    GtkWidget* show_starfield_check = gtk_check_button_new_with_label("Show Star Background");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(show_starfield_check), TRUE);
    g_signal_connect(show_starfield_check, "toggled", G_CALLBACK(on_show_starfield_toggled_cb), this);
    gtk_box_append(GTK_BOX(panel_), show_starfield_check);
    
    // Show grid checkbox
    GtkWidget* show_grid_check = gtk_check_button_new_with_label("Show Grid");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(show_grid_check), TRUE);
    g_signal_connect(show_grid_check, "toggled", G_CALLBACK(on_show_grid_toggled_cb), this);
    gtk_box_append(GTK_BOX(panel_), show_grid_check);
    
    // Grid width control
    GtkWidget* grid_width_label = gtk_label_new("Grid Width:");
    gtk_box_append(GTK_BOX(panel_), grid_width_label);
    
    grid_width_spin_ = gtk_spin_button_new_with_range(0.1, 100.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(grid_width_spin_), 20.0);
    g_signal_connect(grid_width_spin_, "value-changed", G_CALLBACK(on_grid_width_changed_cb), this);
    gtk_box_append(GTK_BOX(panel_), grid_width_spin_);
}

ControlPanel::~ControlPanel() = default;

void ControlPanel::set_playing(bool playing) {
    playing_ = playing;
    gtk_button_set_label(GTK_BUTTON(play_pause_button_), playing_ ? "Pause" : "Play");
}

void ControlPanel::set_num_bodies(int num) {
    // Clamp to maximum of 100,000
    int clamped_num = std::min(num, 100000);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(num_bodies_spin_), clamped_num);
}

void ControlPanel::set_dt(double dt) {
    gtk_range_set_value(GTK_RANGE(dt_scale_), dt);
    // Update value label
    char label_text[32];
    snprintf(label_text, sizeof(label_text), "%.3f", dt);
    gtk_label_set_text(GTK_LABEL(dt_value_label_), label_text);
}

void ControlPanel::append_to_dropdown(GtkWidget* dropdown, const char* text) {
    GtkStringList* model = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(dropdown)));
    if (!model) return;
    gtk_string_list_append(model, text);
}

void ControlPanel::set_dropdown_items(GtkWidget* dropdown, const std::vector<std::string>& items) {
    GtkStringList* model = gtk_string_list_new(NULL);
    for (const auto& item : items) {
        gtk_string_list_append(model, item.c_str());
    }
    // set_model takes ownership of our reference? 
    // Documentation says: "The model is reffed."
    // So we need to unref our local reference.
    gtk_drop_down_set_model(GTK_DROP_DOWN(dropdown), G_LIST_MODEL(model));
    g_object_unref(model);
}

void ControlPanel::select_in_dropdown(GtkWidget* dropdown, const std::string& text) {
    GListModel* model = gtk_drop_down_get_model(GTK_DROP_DOWN(dropdown));
    if (!model) return;
    
    guint n_items = g_list_model_get_n_items(model);
    for (guint i = 0; i < n_items; ++i) {
        GObject* item = (GObject*)g_list_model_get_item(model, i);
        const char* str = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
        if (str && text == str) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), i);
            g_object_unref(item);
            return;
        }
        g_object_unref(item);
    }
}

bool ControlPanel::is_backend_enabled(const std::string& name) const {
    for (const auto& option : backend_options_) {
        if (option.name == name) {
            return option.enabled;
        }
    }
    return true;
}

void ControlPanel::backend_item_setup(GtkListItemFactory*, GtkListItem* list_item, gpointer) {
    GtkWidget* label = gtk_label_new("");
    gtk_widget_add_css_class(label, "backend-option-label");
    gtk_list_item_set_child(list_item, label);
}

void ControlPanel::backend_item_bind(GtkListItemFactory*, GtkListItem* list_item, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    GtkWidget* label = gtk_list_item_get_child(list_item);
    GtkStringObject* string_object = GTK_STRING_OBJECT(gtk_list_item_get_item(list_item));
    const char* text = string_object ? gtk_string_object_get_string(string_object) : "";
    if (label && text) {
        gtk_label_set_text(GTK_LABEL(label), text);
    }
    bool enabled = panel ? panel->is_backend_enabled(text ? std::string(text) : std::string()) : true;
    if (label) {
        gtk_widget_set_sensitive(label, enabled);
        if (enabled) {
            gtk_widget_remove_css_class(label, "backend-option-disabled");
        } else {
            gtk_widget_add_css_class(label, "backend-option-disabled");
        }
    }
}

void ControlPanel::add_backend_option(const char* name, bool enabled) {
    backend_options_.push_back({name ? std::string(name) : std::string(), enabled});
    append_to_dropdown(backend_dropdown_, name);
}

void ControlPanel::set_integrator_options(const std::vector<std::string>& names) {
    set_dropdown_items(integrator_dropdown_, names);
}

void ControlPanel::set_force_method_options(const std::vector<std::string>& names) {
    set_dropdown_items(force_method_dropdown_, names);
}

void ControlPanel::add_integrator_option(const char* name) {
    append_to_dropdown(integrator_dropdown_, name);
}

void ControlPanel::add_force_method_option(const char* name) {
    append_to_dropdown(force_method_dropdown_, name);
}

void ControlPanel::add_renderer_option(const char* name) {
    append_to_dropdown(renderer_dropdown_, name);
}

void ControlPanel::add_initializer_option(const char* name) {
    append_to_dropdown(initializer_dropdown_, name);
}

void ControlPanel::select_backend(const std::string& name) {
    select_in_dropdown(backend_dropdown_, name);
}

void ControlPanel::select_integrator(const std::string& name) {
    select_in_dropdown(integrator_dropdown_, name);
}

void ControlPanel::select_force_method(const std::string& name) {
    select_in_dropdown(force_method_dropdown_, name);
}

void ControlPanel::select_renderer(const std::string& name) {
    select_in_dropdown(renderer_dropdown_, name);
}

void ControlPanel::on_play_pause_clicked(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    if (panel->on_play_pause) {
        panel->on_play_pause();
    }
}

void ControlPanel::on_reset_clicked(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    if (panel->on_reset) {
        panel->on_reset();
    }
}

void ControlPanel::on_num_bodies_changed_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    if (panel->on_num_bodies_changed) {
        int value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
        // Clamp to maximum of 100,000
        value = std::min(value, 100000);
        if (value != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget))) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), value);
        }
        panel->on_num_bodies_changed(value);
    }
}

void ControlPanel::on_backend_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    GtkWidget* widget = GTK_WIDGET(object);
    if (panel->on_backend_changed) {
        gpointer selected = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget));
        if (selected) {
            GObject* item = G_OBJECT(selected);
            const char* text = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
            panel->on_backend_changed(std::string(text));
        }
    }
}

void ControlPanel::on_integrator_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    GtkWidget* widget = GTK_WIDGET(object);
    if (panel->on_integrator_changed) {
        gpointer selected = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget));
        if (selected) {
            GObject* item = G_OBJECT(selected);
            const char* text = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
            panel->on_integrator_changed(std::string(text));
        }
    }
}

void ControlPanel::on_force_method_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    GtkWidget* widget = GTK_WIDGET(object);
    if (panel->on_force_method_changed) {
        GObject* item = G_OBJECT(gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget)));
        if (item) {
            const char* text = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
            panel->on_force_method_changed(std::string(text));
        }
    }
}

void ControlPanel::on_renderer_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    GtkWidget* widget = GTK_WIDGET(object);
    if (panel->on_renderer_changed) {
        GObject* item = G_OBJECT(gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget)));
        if (item) {
            const char* text = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
            panel->on_renderer_changed(std::string(text));
        }
    }
}

void ControlPanel::on_initializer_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    GtkWidget* widget = GTK_WIDGET(object);
    if (panel->on_initializer_changed) {
        GObject* item = G_OBJECT(gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget)));
        if (item) {
            const char* text = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
            panel->on_initializer_changed(std::string(text));
        }
    }
}

void ControlPanel::on_dt_changed_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    if (panel->on_dt_changed) {
        double value = gtk_range_get_value(GTK_RANGE(widget));
        panel->on_dt_changed(value);
        
        // Update value label
        char label_text[32];
        snprintf(label_text, sizeof(label_text), "%.3f", value);
        gtk_label_set_text(GTK_LABEL(panel->dt_value_label_), label_text);
    }
}

void ControlPanel::on_show_vectors_toggled_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    if (panel->on_show_vectors_changed) {
        bool active = gtk_check_button_get_active(GTK_CHECK_BUTTON(widget));
        panel->on_show_vectors_changed(active);
    }
}

void ControlPanel::on_show_trajectories_toggled_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    if (panel->on_show_trajectories_changed) {
        bool active = gtk_check_button_get_active(GTK_CHECK_BUTTON(widget));
        panel->on_show_trajectories_changed(active);
    }
}

void ControlPanel::on_show_glow_toggled_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    if (panel->on_show_glow_changed) {
        bool active = gtk_check_button_get_active(GTK_CHECK_BUTTON(widget));
        panel->on_show_glow_changed(active);
    }
}

void ControlPanel::on_reset_camera_clicked(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    if (panel->on_reset_camera) {
        panel->on_reset_camera();
    }
}

void ControlPanel::on_body_size_changed_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    double value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
    if (panel->on_body_size_changed) {
        panel->on_body_size_changed(value);
    }
}

void ControlPanel::on_show_starfield_toggled_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    bool active = gtk_check_button_get_active(GTK_CHECK_BUTTON(widget));
    if (panel->on_show_starfield_changed) {
        panel->on_show_starfield_changed(active);
    }
}

void ControlPanel::on_show_grid_toggled_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    bool active = gtk_check_button_get_active(GTK_CHECK_BUTTON(widget));
    if (panel->on_show_grid_changed) {
        panel->on_show_grid_changed(active);
    }
}

void ControlPanel::on_grid_width_changed_cb(GtkWidget* widget, gpointer user_data) {
    ControlPanel* panel = static_cast<ControlPanel*>(user_data);
    double value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
    if (panel->on_grid_width_changed) {
        panel->on_grid_width_changed(value);
    }
}

void ControlPanel::on_scenario_generator_clicked(GtkWidget* widget, gpointer user_data) {
    ControlPanel* self = static_cast<ControlPanel*>(user_data);
    if (self->on_scenario_generator) {
        self->on_scenario_generator();
    }
}

} // namespace unisim
