#pragma once

#include <gtk/gtk.h>
#include "../simulation/universe.hpp"
#include "../compute/compute_backend.hpp"
#include "../rendering/renderer.hpp"
#include "../initial_conditions/initializer.hpp"
#include "viewport.hpp"
#include "control_panel.hpp"
#include "metrics_monitor.hpp"
#include "scenario_generator.hpp"
#include <memory>
#include <map>
#include <string>
#include <chrono>

namespace unisim {

/**
 * @brief Main application window
 */
class MainWindow {
public:
    MainWindow(GtkApplication* app);
    ~MainWindow();

    GtkWidget* widget() {
        return window_;
    }

    void show();

private:
    static gboolean on_tick(gpointer user_data);
    static gboolean on_metrics_update(gpointer user_data);
    static void on_scenario_dialog_response(GtkDialog* dialog, int response_id, gpointer user_data);
    void setup_ui();
    void setup_simulation();
    void update_simulation();
    void reset_simulation();
    void update_metrics();
    void update_control_panel_options();
    void update_viewport_info_display();

    GtkApplication* app_;
    GtkWidget* window_;
    Viewport viewport_;
    ControlPanel control_panel_;
    ScenarioGenerator scenario_generator_;

    Universe universe_;
    
    std::map<std::string, std::shared_ptr<ComputeBackend>> backends_;
    std::shared_ptr<ComputeBackend> current_backend_;
    std::string current_backend_name_;
    
    std::map<std::string, std::shared_ptr<Renderer>> renderers_;
    std::shared_ptr<Renderer> renderer_;
    
    std::map<std::string, std::shared_ptr<Initializer>> initializers_;
    std::shared_ptr<Initializer> initializer_;

    // Trajectory tracking
    std::vector<std::vector<Vector3D>> trajectories_;
    static constexpr std::size_t MAX_TRAJECTORY_POINTS = 500;
    bool show_trajectories_{false};
    bool show_glow_{true};

    bool playing_;
    double dt_;
    guint tick_id_;
    guint metrics_update_id_;
    
    MetricsMonitor metrics_monitor_;
    std::chrono::steady_clock::time_point simulation_start_time_;
    uint64_t total_steps_;
};

} // namespace unisim
