#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <functional>
#include <string>
#include <vector>

namespace unisim {

/**
 * @brief Control panel widget for simulation controls
 */
class ControlPanel {
public:
    ControlPanel();
    ~ControlPanel();

    GtkWidget* widget() {
        return scrolled_window_;
    }

    // Signal callbacks
    std::function<void()> on_play_pause;
    std::function<void()> on_reset;
    std::function<void(int)> on_num_bodies_changed;
    std::function<void(std::string)> on_backend_changed;
    std::function<void(std::string)> on_integrator_changed;
    std::function<void(std::string)> on_force_method_changed;
    std::function<void(std::string)> on_renderer_changed;
    std::function<void(std::string)> on_initializer_changed;
    std::function<void(double)> on_dt_changed;
    std::function<void(bool)> on_show_vectors_changed;
    std::function<void(bool)> on_show_trajectories_changed;
    std::function<void(bool)> on_show_glow_changed;
    std::function<void()> on_reset_camera;
    std::function<void(double)> on_body_size_changed;
    std::function<void(bool)> on_show_starfield_changed;
    std::function<void(bool)> on_show_grid_changed;
    std::function<void(double)> on_grid_width_changed;
    std::function<void()> on_scenario_generator;

    void set_playing(bool playing);
    void set_num_bodies(int num);
    void set_dt(double dt);
    
    void add_backend_option(const char* name, bool enabled);
    void set_integrator_options(const std::vector<std::string>& names);
    void set_force_method_options(const std::vector<std::string>& names);
    
    void add_integrator_option(const char* name);
    void add_force_method_option(const char* name);
    void add_renderer_option(const char* name);
    void add_initializer_option(const char* name);

    // Select an item by string in a dropdown
    void select_backend(const std::string& name);
    void select_integrator(const std::string& name);
    void select_force_method(const std::string& name);
    void select_renderer(const std::string& name);

    GtkWidget* renderer_dropdown() const {
        return renderer_dropdown_;
    }

private:
    static void on_play_pause_clicked(GtkWidget* widget, gpointer user_data);
    static void on_reset_clicked(GtkWidget* widget, gpointer user_data);
    static void on_num_bodies_changed_cb(GtkWidget* widget, gpointer user_data);
    static void on_backend_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data);
    static void on_integrator_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data);
    static void on_force_method_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data);
    static void on_renderer_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data);
    static void on_initializer_changed_cb(GObject* object, GParamSpec* pspec, gpointer user_data);
    static void on_dt_changed_cb(GtkWidget* widget, gpointer user_data);
    static void on_show_vectors_toggled_cb(GtkWidget* widget, gpointer user_data);
    static void on_show_trajectories_toggled_cb(GtkWidget* widget, gpointer user_data);
    static void on_show_glow_toggled_cb(GtkWidget* widget, gpointer user_data);
    static void on_reset_camera_clicked(GtkWidget* widget, gpointer user_data);
    static void on_body_size_changed_cb(GtkWidget* widget, gpointer user_data);
    static void on_show_starfield_toggled_cb(GtkWidget* widget, gpointer user_data);
    static void on_show_grid_toggled_cb(GtkWidget* widget, gpointer user_data);
    static void on_grid_width_changed_cb(GtkWidget* widget, gpointer user_data);
    static void on_scenario_generator_clicked(GtkWidget* widget, gpointer user_data);

    // Helper to append string to dropdown list
    void append_to_dropdown(GtkWidget* dropdown, const char* text);
    void set_dropdown_items(GtkWidget* dropdown, const std::vector<std::string>& items);
    void select_in_dropdown(GtkWidget* dropdown, const std::string& text);
    bool is_backend_enabled(const std::string& name) const;

    GtkWidget* panel_;
    GtkWidget* play_pause_button_;
    GtkWidget* reset_button_;
    GtkWidget* num_bodies_spin_;
    GtkWidget* dt_scale_;
    GtkWidget* dt_value_label_;
    GtkWidget* backend_dropdown_;
    GtkWidget* integrator_dropdown_;
    GtkWidget* force_method_dropdown_;
    GtkWidget* renderer_dropdown_;
    GtkWidget* initializer_dropdown_;
    GtkWidget* body_size_spin_;
    GtkWidget* grid_width_spin_;
    bool playing_;

    GtkWidget* scrolled_window_;
    GtkWidget* content_box_;
    struct BackendOption {
        std::string name;
        bool enabled;
    };
    std::vector<BackendOption> backend_options_;
    static void backend_item_setup(GtkListItemFactory* factory, GtkListItem* list_item, gpointer user_data);
    static void backend_item_bind(GtkListItemFactory* factory, GtkListItem* list_item, gpointer user_data);

public:
    int get_num_bodies() const {
        return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(num_bodies_spin_));
    }

    GtkWidget* integrator_dropdown() const {
        return integrator_dropdown_;
    }
    
    GtkWidget* force_method_dropdown() const {
        return force_method_dropdown_;
    }
};

} // namespace unisim
