#include "main_window.hpp"
#include "metrics_monitor.hpp"
#include "../compute/cpu_backend.hpp"
#include "../compute/metal_backend.hpp"
#include "../compute/cuda_backend.hpp"
#include "../rendering/renderer_2d.hpp"
#include "../rendering/renderer_3d.hpp"
#include "../rendering/renderer_3d_opengl.hpp"
#include "../initial_conditions/random.hpp"
#include "../initial_conditions/spiral_galaxy.hpp"
#include "../initial_conditions/elliptical_galaxy.hpp"
#include "../initial_conditions/galaxy_collision.hpp"
#include "../initial_conditions/solar_system.hpp"
#include "../initial_conditions/binary_star.hpp"
#include "../initial_conditions/black_hole.hpp"
#include "scenario_generator.hpp"
#include <string>
#include <memory>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <vector>

namespace unisim {

MainWindow::MainWindow(GtkApplication* app)
    : app_(app), playing_(false), dt_(0.01), tick_id_(0), metrics_update_id_(0), total_steps_(0) {
    
    simulation_start_time_ = std::chrono::steady_clock::now();
    setup_simulation();
    setup_ui();
    
    // Start metrics update timer (update every 500ms)
    metrics_update_id_ = g_timeout_add(500, on_metrics_update, this);
    
    // Initial metrics update
    update_metrics();
}

MainWindow::~MainWindow() {
    if (tick_id_ != 0) {
        g_source_remove(tick_id_);
    }
    if (metrics_update_id_ != 0) {
        g_source_remove(metrics_update_id_);
    }
}

void MainWindow::show() {
    gtk_window_present(GTK_WINDOW(window_));
}

void MainWindow::setup_simulation() {
    // Create backends (platform specific)
    backends_.clear();
    backends_["CPU"] = std::make_shared<CpuBackend>();

#ifdef __APPLE__
    backends_["Metal"] = std::make_shared<MetalBackend>();
#endif

#ifdef UNISIM_USE_CUDA
    backends_["CUDA"] = std::make_shared<CudaBackend>();
#endif
    
    // Initialize backends
    for (auto& [name, backend] : backends_) {
        if (backend) {
            backend->initialize();
        }
    }
    
    auto select_backend = [&](const std::vector<std::string>& priority) -> std::shared_ptr<ComputeBackend> {
        for (const auto& candidate : priority) {
            auto it = backends_.find(candidate);
            if (it != backends_.end() && it->second && it->second->is_available()) {
                current_backend_name_ = candidate;
                return it->second;
            }
        }
        auto cpu_it = backends_.find("CPU");
        if (cpu_it != backends_.end() && cpu_it->second) {
            current_backend_name_ = "CPU";
            return cpu_it->second;
        }
        if (!backends_.empty()) {
            current_backend_name_ = backends_.begin()->first;
            return backends_.begin()->second;
        }
        current_backend_name_ = "CPU";
        return nullptr;
    };
    
    current_backend_ = select_backend({"Metal", "CUDA", "CPU"});
    if (!current_backend_) {
        auto fallback = backends_.find("CPU");
        if (fallback != backends_.end()) {
            current_backend_ = fallback->second;
            current_backend_name_ = "CPU";
        }
    }
    
    // Create renderers
    renderers_["2D"] = std::make_shared<Renderer2D>();
    renderers_["3D"] = std::make_shared<Renderer3D>();
    renderers_["3D OpenGL"] = std::make_shared<Renderer3DOpenGL>();
    
    // Create initializers
    initializers_["Random"] = std::make_shared<RandomInitializer>();
    initializers_["Spiral Galaxy"] = std::make_shared<SpiralGalaxyInitializer>();
    initializers_["Elliptical Galaxy"] = std::make_shared<EllipticalGalaxyInitializer>();
    initializers_["Galaxy Collision"] = std::make_shared<GalaxyCollisionInitializer>();
    initializers_["Solar System"] = std::make_shared<SolarSystemInitializer>();
    initializers_["Binary Star System"] = std::make_shared<BinaryStarInitializer>();
    
    // Set defaults
    renderer_ = renderers_["3D OpenGL"]; // Default to 3D OpenGL
    initializer_ = initializers_["Random"];
    
    // Initialize renderer settings
    if (auto renderer_2d = std::dynamic_pointer_cast<Renderer2D>(renderer_)) {
        renderer_2d->set_show_trajectories(show_trajectories_);
        renderer_2d->set_show_glow(show_glow_);
    }
    if (auto renderer_3d = std::dynamic_pointer_cast<Renderer3D>(renderer_)) {
        renderer_3d->set_show_trajectories(show_trajectories_);
        renderer_3d->set_show_glow(show_glow_);
    }
    if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
        renderer_3d_gl->set_show_trajectories(show_trajectories_);
        renderer_3d_gl->set_show_glow(show_glow_);
    }
    
    // Initialize universe
    initializer_->initialize(universe_, 100);
    
    // Initialize trajectories
    trajectories_.clear();
    trajectories_.resize(universe_.size());
    viewport_.set_trajectories(&trajectories_);
}

void MainWindow::update_viewport_info_display() {
    if (!current_backend_) return;
    
    std::string integrator = "Unknown";
    std::string force_method = "Unknown";
    
    // Get selected integrator
    GtkDropDown* int_drop = GTK_DROP_DOWN(control_panel_.integrator_dropdown());
    if (int_drop) {
        GObject* item = G_OBJECT(gtk_drop_down_get_selected_item(int_drop));
        if (item) {
            integrator = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
        }
    }
    
    // Get selected force method
    GtkDropDown* force_drop = GTK_DROP_DOWN(control_panel_.force_method_dropdown());
    if (force_drop) {
        GObject* item = G_OBJECT(gtk_drop_down_get_selected_item(force_drop));
        if (item) {
            force_method = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
        }
    }
    
    viewport_.set_info(current_backend_->name(), integrator, force_method);
}

void MainWindow::setup_ui() {
    window_ = gtk_application_window_new(app_);
    gtk_window_set_title(GTK_WINDOW(window_), "Unisim - N-Body Simulation");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1400, 900);
    gtk_window_set_resizable(GTK_WINDOW(window_), TRUE);
    
    // Main container with resizable panes (viewport gets 4/5, control panel gets 1/5)
    GtkWidget* paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_hexpand(paned, TRUE);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_window_set_child(GTK_WINDOW(window_), paned);
    
    // Add CSS styling for metrics progress bars and section titles
    GtkCssProvider* css_provider = gtk_css_provider_new();
    const char* css = 
        ".cpu-progress { background: #2e7d32; } "
        ".cpu-progress.medium-usage { background: #f57c00; } "
        ".cpu-progress.high-usage { background: #c62828; } "
        ".gpu-progress { background: #1565c0; } "
        ".gpu-progress.medium-usage { background: #f57c00; } "
        ".gpu-progress.high-usage { background: #c62828; } "
        ".viewport-info { color: white; font-weight: bold; text-shadow: 1px 1px 2px black; } "
        ".nav-instructions { "
        "  background-color: rgba(0, 0, 0, 0.7); "
        "  padding: 8px 12px; "
        "  border-radius: 6px; "
        "  border: 1px solid rgba(176, 176, 176, 0.5); "
        "} "
        ".title { "
        "  font-weight: bold; "
        "  font-size: 1.0em; "
        "  margin-top: 6px; "
        "  margin-bottom: 3px; "
        "} "
        ".backend-option-label { padding: 2px 6px; } "
        ".backend-option-disabled { opacity: 0.45; }";
    
    gtk_css_provider_load_from_string(css_provider, css);
    
    GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window_));
    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(css_provider);

    // Viewport (left pane, gets 4/5 of width)
    fprintf(stderr, "Setting up viewport: universe size = %zu\n", universe_.size());
    viewport_.set_universe(&universe_);
    // Add viewport to paned FIRST, then set renderer (renderer needs widget in hierarchy)
    GtkWidget* viewport_widget = viewport_.widget();
    gtk_paned_set_start_child(GTK_PANED(paned), viewport_widget);
    gtk_widget_set_hexpand(viewport_widget, TRUE);
    gtk_widget_set_vexpand(viewport_widget, TRUE);
    gtk_widget_set_hexpand_set(viewport_widget, TRUE);
    gtk_widget_set_vexpand_set(viewport_widget, TRUE);
    gtk_widget_set_size_request(viewport_widget, 200, 200); // Minimum size
    fprintf(stderr, "Viewport widget added to paned\n");
    
    // Control panel (right pane, gets 1/5 of width, resizable)
    gtk_paned_set_end_child(GTK_PANED(paned), control_panel_.widget());
    gtk_widget_set_hexpand(control_panel_.widget(), FALSE);
    gtk_widget_set_hexpand_set(control_panel_.widget(), FALSE);
    fprintf(stderr, "Control panel widget added to paned\n");
    
    // Set initial position to 1/5 from right (4/5 for viewport) after realization
    g_signal_connect(paned, "realize", G_CALLBACK(+[](GtkWidget* widget, gpointer) {
        GtkPaned* paned = GTK_PANED(widget);
        int width = gtk_widget_get_width(widget);
        if (width > 0) {
            // Set position so control panel is 1/5 of width (viewport gets 4/5)
            gtk_paned_set_position(paned, width * 4 / 5);
        }
    }), nullptr);
    
    // Set renderer after widget is in hierarchy
    viewport_.set_renderer(renderer_);

    // Control panel callbacks
    control_panel_.on_play_pause = [this]() {
        playing_ = !playing_;
        control_panel_.set_playing(playing_);
        if (playing_ && tick_id_ == 0) {
            tick_id_ = g_timeout_add(16, on_tick, this); // ~60 FPS
        } else if (!playing_ && tick_id_ != 0) {
            g_source_remove(tick_id_);
            tick_id_ = 0;
        }
    };

    control_panel_.on_reset = [this]() {
        reset_simulation();
    };

    control_panel_.on_num_bodies_changed = [this](int num) {
        // Trajectories will be reset in reset_simulation
        reset_simulation();
    };

    control_panel_.on_backend_changed = [this](std::string name) {
        if (name == current_backend_name_) {
            return;
        }
        auto it = backends_.find(name);
        if (it == backends_.end() || !it->second) {
            return;
        }
        auto backend = it->second;
        if (!backend->is_available()) {
            GtkWidget* dialog = gtk_message_dialog_new(
                GTK_WINDOW(window_),
                static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "\"%s\" backend is not available on this system.\nCheck the console for initialization errors or driver issues.",
                name.c_str());
            g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* d, int, gpointer) {
                gtk_window_destroy(GTK_WINDOW(d));
            }), nullptr);
            gtk_window_present(GTK_WINDOW(dialog));
            control_panel_.select_backend(current_backend_name_);
            return;
        }
        
        current_backend_ = backend;
        current_backend_name_ = name;
        update_control_panel_options();
        update_viewport_info_display();
    };

    control_panel_.on_integrator_changed = [this](std::string name) {
        if (current_backend_) {
            current_backend_->set_integrator(name);
            update_viewport_info_display();
        }
    };

    control_panel_.on_force_method_changed = [this](std::string name) {
        if (current_backend_) {
            current_backend_->set_force_method(name);
            update_viewport_info_display();
        }
    };

    control_panel_.on_renderer_changed = [this](std::string name) {
        auto it = renderers_.find(name);
        if (it != renderers_.end()) {
            renderer_ = it->second;
            viewport_.set_renderer(renderer_);
            // Sync visibility settings with current renderer
            if (auto renderer_2d = std::dynamic_pointer_cast<Renderer2D>(renderer_)) {
                renderer_2d->set_show_trajectories(show_trajectories_);
                renderer_2d->set_show_glow(show_glow_);
            }
            if (auto renderer_3d = std::dynamic_pointer_cast<Renderer3D>(renderer_)) {
                renderer_3d->set_show_trajectories(show_trajectories_);
                renderer_3d->set_show_glow(show_glow_);
                viewport_.reset_view();
            }
            if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
                renderer_3d_gl->set_show_trajectories(show_trajectories_);
                renderer_3d_gl->set_show_glow(show_glow_);
                viewport_.reset_view();
            }
            viewport_.queue_draw();
        }
    };

    control_panel_.on_initializer_changed = [this](std::string name) {
        auto it = initializers_.find(name);
        if (it != initializers_.end()) {
            initializer_ = it->second;
            reset_simulation();
        }
    };

    control_panel_.on_scenario_generator = [this]() {
        scenario_generator_.show_dialog(GTK_WINDOW(window_));
        g_signal_connect(scenario_generator_.dialog(), "response", 
                        G_CALLBACK(on_scenario_dialog_response), this);
    };

    control_panel_.on_dt_changed = [this](double dt) {
        dt_ = dt;
    };

    control_panel_.on_show_vectors_changed = [this](bool show) {
        if (auto renderer_2d = std::dynamic_pointer_cast<Renderer2D>(renderer_)) {
            renderer_2d->set_show_vectors(show);
        }
        if (auto renderer_3d = std::dynamic_pointer_cast<Renderer3D>(renderer_)) {
            renderer_3d->set_show_vectors(show);
        }
        if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
            renderer_3d_gl->set_show_vectors(show);
        }
        viewport_.queue_draw();
    };

    control_panel_.on_show_trajectories_changed = [this](bool show) {
        show_trajectories_ = show;
        if (auto renderer_2d = std::dynamic_pointer_cast<Renderer2D>(renderer_)) {
            renderer_2d->set_show_trajectories(show);
        }
        if (auto renderer_3d = std::dynamic_pointer_cast<Renderer3D>(renderer_)) {
            renderer_3d->set_show_trajectories(show);
        }
        if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
            renderer_3d_gl->set_show_trajectories(show);
        }
        viewport_.queue_draw();
    };

    control_panel_.on_show_glow_changed = [this](bool show) {
        show_glow_ = show;
        if (auto renderer_2d = std::dynamic_pointer_cast<Renderer2D>(renderer_)) {
            renderer_2d->set_show_glow(show);
        }
        if (auto renderer_3d = std::dynamic_pointer_cast<Renderer3D>(renderer_)) {
            renderer_3d->set_show_glow(show);
        }
        if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
            renderer_3d_gl->set_show_glow(show);
        }
        viewport_.queue_draw();
    };

    control_panel_.on_reset_camera = [this]() {
        viewport_.reset_view();
    };

    control_panel_.on_body_size_changed = [this](double size) {
        if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
            renderer_3d_gl->set_mass_scale_factor(size);
            viewport_.queue_draw();
        }
    };

    control_panel_.on_show_starfield_changed = [this](bool show) {
        if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
            renderer_3d_gl->set_show_starfield(show);
            viewport_.queue_draw();
        }
    };

    control_panel_.on_show_grid_changed = [this](bool show) {
        if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
            renderer_3d_gl->set_show_grid(show);
            viewport_.queue_draw();
        }
    };

    control_panel_.on_grid_width_changed = [this](double width) {
        if (auto renderer_3d_gl = std::dynamic_pointer_cast<Renderer3DOpenGL>(renderer_)) {
            renderer_3d_gl->set_grid_width(width);
            viewport_.queue_draw();
        }
    };

    // Populate control panel options
    
    // Backends (disabled entries shown when unavailable)
    for (const auto& [name, backend] : backends_) {
        if (!backend) {
            continue;
        }
        control_panel_.add_backend_option(name.c_str(), backend->is_available());
    }
    control_panel_.select_backend(current_backend_name_);

    // Renderers
    for (const auto& [name, _] : renderers_) {
        control_panel_.add_renderer_option(name.c_str());
    }
    control_panel_.select_renderer("3D OpenGL");

    // Initializers
    for (const auto& [name, _] : initializers_) {
        control_panel_.add_initializer_option(name.c_str());
    }

    update_control_panel_options();
    update_viewport_info_display(); // Initial update

    fprintf(stderr, "UI Setup complete.\n");
}

void MainWindow::update_control_panel_options() {
    if (!current_backend_) return;
    
    control_panel_.set_integrator_options(current_backend_->get_integrators());
    control_panel_.set_force_method_options(current_backend_->get_force_methods());
    
    // Set defaults to most performant combination
    auto integrators = current_backend_->get_integrators();
    if (!integrators.empty()) {
        // Prefer Verlet (faster than RK4 - only 1 force evaluation vs 4)
        bool found_verlet = false;
        for(const auto& i : integrators) {
            if(i.find("Verlet") != std::string::npos) {
                control_panel_.select_integrator(i);
                current_backend_->set_integrator(i);
                found_verlet = true;
                break;
            }
        }
        // Fallback to first available if Verlet not found
        if (!found_verlet) {
            control_panel_.select_integrator(integrators[0]);
            current_backend_->set_integrator(integrators[0]);
        }
    }
    
    auto force_methods = current_backend_->get_force_methods();
    if (!force_methods.empty()) {
        // Prefer Barnes-Hut (O(N log N) vs O(N²) for brute force)
        bool found_barnes_hut = false;
        for(const auto& fm : force_methods) {
            if(fm.find("Barnes-Hut") != std::string::npos) {
                control_panel_.select_force_method(fm);
                current_backend_->set_force_method(fm);
                found_barnes_hut = true;
                break;
            }
        }
        // Fallback to first available if Barnes-Hut not found
        if (!found_barnes_hut) {
            control_panel_.select_force_method(force_methods[0]);
            current_backend_->set_force_method(force_methods[0]);
        }
    }
    
    update_viewport_info_display();
}

void MainWindow::update_simulation() {
    if (playing_ && current_backend_) {
        // Measure step time for performance metrics
        auto step_start = std::chrono::high_resolution_clock::now();
        
        current_backend_->step(universe_, dt_);
        total_steps_++;
        
        auto step_end = std::chrono::high_resolution_clock::now();
        auto step_duration = std::chrono::duration_cast<std::chrono::microseconds>(step_end - step_start);
        double step_time_ms = step_duration.count() / 1000.0;
        metrics_monitor_.record_step_time(step_time_ms);
        
        // Update trajectories
        for (std::size_t i = 0; i < universe_.size() && i < trajectories_.size(); ++i) {
            trajectories_[i].push_back(universe_[i].position);
            
            // Limit trajectory length
            if (trajectories_[i].size() > MAX_TRAJECTORY_POINTS) {
                trajectories_[i].erase(trajectories_[i].begin());
            }
        }
        
        // Record frame for FPS calculation
        metrics_monitor_.record_frame();
        
        viewport_.queue_draw();
    }
}

void MainWindow::on_scenario_dialog_response(GtkDialog* dialog, int response_id, gpointer user_data) {
    MainWindow* self = static_cast<MainWindow*>(user_data);
    if (response_id == GTK_RESPONSE_ACCEPT || self->scenario_generator_.was_accepted()) {
        std::string scenario = self->scenario_generator_.get_selected_scenario();
        auto params = self->scenario_generator_.get_params();
        
        // Create initializer with parameters
        if (scenario == "Galaxy Collision") {
            self->initializer_ = std::make_shared<GalaxyCollisionInitializer>(
                params.galaxy_size, params.separation,
                params.galaxy1_color_r, params.galaxy1_color_g, params.galaxy1_color_b,
                params.galaxy2_color_r, params.galaxy2_color_g, params.galaxy2_color_b
            );
        } else if (scenario == "Spiral Galaxy") {
            self->initializer_ = std::make_shared<SpiralGalaxyInitializer>(
                20.0, params.num_arms, params.arm_tightness,
                params.spiral_color_r, params.spiral_color_g, params.spiral_color_b
            );
        } else if (scenario == "Elliptical Galaxy") {
            self->initializer_ = std::make_shared<EllipticalGalaxyInitializer>(
                15.0, params.ellipticity,
                params.elliptical_color_r, params.elliptical_color_g, params.elliptical_color_b
            );
        } else if (scenario == "Solar System") {
            self->initializer_ = std::make_shared<SolarSystemInitializer>(
                params.num_planets, params.include_asteroids
            );
        } else if (scenario == "Binary Star System") {
            self->initializer_ = std::make_shared<BinaryStarInitializer>(
                params.binary_separation, params.num_planets_binary
            );
        } else if (scenario == "Black Hole System") {
            self->initializer_ = std::make_shared<BlackHoleInitializer>(
                params.black_hole_mass, params.num_orbiting_bodies, params.system_radius
            );
        } else {
            // Random or other - use default
            auto it = self->initializers_.find(scenario);
            if (it != self->initializers_.end()) {
                self->initializer_ = it->second;
            }
        }
        
        // Update number of bodies if changed (clamped to 100k max)
        int num_bodies = std::min(params.num_bodies, 100000);
        self->control_panel_.set_num_bodies(num_bodies);
        self->reset_simulation();
    }
}

void MainWindow::reset_simulation() {
    int num_bodies = std::min(control_panel_.get_num_bodies(), 100000);
    
    if (initializer_) {
        initializer_->initialize(universe_, num_bodies);
        
        // Clear and resize trajectories
        trajectories_.clear();
        trajectories_.resize(universe_.size());
        
        // Reset simulation metrics
        simulation_start_time_ = std::chrono::steady_clock::now();
        total_steps_ = 0;
        
        viewport_.queue_draw();
    }
}

gboolean MainWindow::on_tick(gpointer user_data) {
    MainWindow* window = static_cast<MainWindow*>(user_data);
    window->update_simulation();
    return G_SOURCE_CONTINUE;
}

gboolean MainWindow::on_metrics_update(gpointer user_data) {
    MainWindow* window = static_cast<MainWindow*>(user_data);
    window->update_metrics();
    return G_SOURCE_CONTINUE;
}

void MainWindow::update_metrics() {
    // Update system metrics
    metrics_monitor_.update();
    
    // Calculate simulation time
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - simulation_start_time_).count();
    metrics_monitor_.set_simulation_time(elapsed);
    metrics_monitor_.set_step_count(total_steps_);
    
    // Update viewport metrics overlay
    viewport_.set_metrics(
        metrics_monitor_.get_fps(),
        metrics_monitor_.get_cpu_usage(),
        metrics_monitor_.get_gpu_usage(),
        metrics_monitor_.get_memory_usage_mb(),
        metrics_monitor_.get_simulation_time(),
        metrics_monitor_.get_step_count(),
        universe_.size(),
        metrics_monitor_.get_avg_step_time_ms(),
        metrics_monitor_.get_steps_per_second(),
        metrics_monitor_.get_bodies_per_second(universe_.size()),
        metrics_monitor_.get_process_thread_count(),
        metrics_monitor_.get_parallel_threads_last(),
        metrics_monitor_.get_parallel_threads_avg(),
        metrics_monitor_.get_parallel_threads_peak(),
        metrics_monitor_.get_logical_cores(),
        metrics_monitor_.get_physical_cores(),
        metrics_monitor_.get_parallel_active_jobs()
    );
    
    // Update viewport system info
    viewport_.set_system_info(
        metrics_monitor_.get_cpu_name(),
        metrics_monitor_.get_gpu_name(),
        metrics_monitor_.get_metal_version(),
        metrics_monitor_.get_cuda_version()
    );
}

} // namespace unisim
