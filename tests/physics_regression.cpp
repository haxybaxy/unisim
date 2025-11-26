#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <random>
#include <functional>
#include <cstdlib>
#include <cctype>

#include "../src/simulation/universe.hpp"
#include "../src/simulation/body.hpp"
#include "../src/simulation/vector3d.hpp"
#include "../src/simulation/force_computers/brute_force.hpp"
#include "../src/simulation/force_computers/fast_multipole.hpp"
#include "../src/simulation/force_computers/barnes_hut.hpp"
#include "../src/simulation/integrators/euler.hpp"
#include "../src/simulation/integrators/verlet.hpp"
#include "../src/simulation/integrators/leapfrog.hpp"
#include "../src/simulation/integrators/runge_kutta.hpp"

#ifdef __APPLE__
#include "../src/compute/metal_backend.hpp"
#endif

#ifdef UNISIM_USE_CUDA
#include "../src/compute/cuda_backend.hpp"
#endif

using namespace unisim;

struct Scenario {
    std::string name;
    Universe universe;
    double dt;
};

struct ForceConfig {
    std::string name;
    std::function<std::shared_ptr<ForceComputer>()> factory;
    double force_tolerance;
    double pos_tolerance_scale;
    double vel_tolerance_scale;
};

#ifdef __APPLE__
struct MetalConfig {
    std::string label;
    std::string integrator;
    std::string force_method;
    double pos_tolerance;
    double vel_tolerance;
};
#endif

#ifdef UNISIM_USE_CUDA
struct CudaConfig {
    std::string label;
    std::string integrator;
    std::string force_method;
    double pos_tolerance;
    double vel_tolerance;
};
#endif

Scenario make_two_body() {
    Scenario s;
    s.name = "two_body";
    s.dt = 0.01;

    Body a;
    a.position = {-5.0, 0.0, 0.0};
    a.velocity = {0.0, -0.2, 0.0};
    a.mass = 10.0;

    Body b;
    b.position = {5.0, 0.0, 0.0};
    b.velocity = {0.0, 0.2, 0.0};
    b.mass = 10.0;

    s.universe.add_body(a);
    s.universe.add_body(b);
    return s;
}

Scenario make_three_body_L() {
    Scenario s;
    s.name = "three_body_L";
    s.dt = 0.02;

    Body a;
    a.position = {0.0, 0.0, 0.0};
    a.mass = 5.0;

    Body b;
    b.position = {3.0, 0.0, 0.0};
    b.mass = 2.5;

    Body c;
    c.position = {0.0, 4.0, 0.0};
    c.mass = 7.5;

    s.universe.add_body(a);
    s.universe.add_body(b);
    s.universe.add_body(c);
    return s;
}

Scenario make_seeded_random_small() {
    Scenario s;
    s.name = "seeded_random_small";
    s.dt = 0.005;

    std::mt19937 gen(42);
    std::uniform_real_distribution<double> pos(-2.0, 2.0);
    std::uniform_real_distribution<double> vel(-0.1, 0.1);
    std::uniform_real_distribution<double> mass(0.5, 3.0);

    for (int i = 0; i < 6; ++i) {
        Body b;
        b.position = {pos(gen), pos(gen), pos(gen)};
        b.velocity = {vel(gen), vel(gen), vel(gen)};
        b.mass = mass(gen);
        s.universe.add_body(b);
    }
    return s;
}

Scenario make_seeded_cloud(const std::string& name, std::size_t count, unsigned seed,
                           double extent, double max_velocity, double dt) {
    Scenario s;
    s.name = name;
    s.dt = dt;
    s.universe.clear();
    s.universe.reserve(count);

    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> pos_dist(-extent, extent);
    std::uniform_real_distribution<double> vel_dist(-max_velocity, max_velocity);
    std::uniform_real_distribution<double> mass_dist(0.2, 5.0);

    for (std::size_t i = 0; i < count; ++i) {
        Body b;
        b.position = {pos_dist(gen), pos_dist(gen), pos_dist(gen)};
        b.velocity = {vel_dist(gen), vel_dist(gen), vel_dist(gen)};
        b.mass = mass_dist(gen);
        b.radius = std::cbrt(b.mass) * 0.1;
        s.universe.add_body(std::move(b));
    }
    return s;
}

Scenario make_ring_system(const std::string& name, std::size_t count, double radius,
                          double orbital_speed, double dt) {
    Scenario s;
    s.name = name;
    s.dt = dt;
    s.universe.clear();
    s.universe.reserve(count);

    const double mass = 1.0;
    for (std::size_t i = 0; i < count; ++i) {
        double angle = (2.0 * M_PI * static_cast<double>(i)) / static_cast<double>(count);
        Body b;
        b.position = {radius * std::cos(angle), radius * std::sin(angle), 0.0};
        // Tangential velocity for circular motion
        b.velocity = {-orbital_speed * std::sin(angle), orbital_speed * std::cos(angle), 0.0};
        b.mass = mass;
        b.radius = 0.2;
        s.universe.add_body(std::move(b));
    }
    return s;
}

bool should_run_stress() {
    const char* env = std::getenv("UNISIM_REGRESSION_STRESS");
    if (!env) return false;
    std::string value(env);
    for (auto& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return (value == "1" || value == "true" || value == "on" || value == "yes");
}

std::vector<Scenario> build_scenarios(bool stress_mode) {
    std::vector<Scenario> scenarios;
    scenarios.push_back(make_two_body());
    scenarios.push_back(make_three_body_L());
    scenarios.push_back(make_seeded_cloud("random_8", 8, 2024, 4.0, 0.2, 0.005));
    scenarios.push_back(make_seeded_random_small());
    scenarios.push_back(make_seeded_cloud("random_32", 32, 1337, 6.0, 0.2, 0.004));
    scenarios.push_back(make_ring_system("ring_64", 64, 5.0, 0.3, 0.004));
    scenarios.push_back(make_seeded_cloud("random_256", 256, 9001, 15.0, 0.5, 0.0025));

    if (stress_mode) {
        scenarios.push_back(make_seeded_cloud("random_1024", 1024, 4242, 20.0, 0.4, 0.002));
        scenarios.push_back(make_seeded_cloud("random_3000", 3000, 7777, 30.0, 0.3, 0.001));
    }

    return scenarios;
}

std::pair<double, double> get_integrator_tolerance(const std::string& integrator_name) {
    if (integrator_name == "Euler") return {1e-3, 1e-3};
    if (integrator_name == "Verlet") return {1e-3, 1e-2};
    if (integrator_name == "Leapfrog") return {5e-5, 5e-5};
    if (integrator_name == "Runge-Kutta") return {1e-6, 1e-6};
    return {1e-4, 1e-4};
}

std::shared_ptr<Integrator> create_integrator(const std::string& name,
                                              const std::shared_ptr<ForceComputer>& fc) {
    if (name == "Euler") return std::make_shared<EulerIntegrator>(fc);
    if (name == "Verlet") return std::make_shared<VerletIntegrator>(fc);
    if (name == "Leapfrog") return std::make_shared<LeapfrogIntegrator>(fc);
    if (name == "Runge-Kutta") return std::make_shared<RungeKuttaIntegrator>(fc);
    return nullptr;
}

const std::vector<ForceConfig> kCpuForceConfigs = {
    {"Brute Force",
     []() { return std::make_shared<BruteForce>(); },
     1e-8,
     1.0,
     1.0},
    {"Barnes-Hut",
     []() {
         auto fc = std::make_shared<BarnesHut>();
         fc->set_theta(0.2);
         return fc;
     },
     1e-2,
     10.0,
     10.0},
    {"Fast Multipole",
     []() {
         auto fc = std::make_shared<FastMultipole>();
         fc->set_theta(0.25);
         fc->set_max_leaf_size(16);
         return fc;
     },
     5e-3,
     4.0,
     4.0}
};

const std::vector<std::string> kIntegratorNames = {"Euler", "Verlet", "Leapfrog", "Runge-Kutta"};
constexpr std::size_t kAccurateForceBodyLimit = 16;

const ForceConfig* find_force_config(const std::string& name) {
    for (const auto& cfg : kCpuForceConfigs) {
        if (cfg.name == name) return &cfg;
    }
    return nullptr;
}

std::vector<Vector3D> capture_accelerations(ForceComputer& fc, const Universe& snapshot) {
    Universe copy = snapshot;
    fc.compute_forces(copy);
    std::vector<Vector3D> result;
    result.reserve(copy.size());
    for (std::size_t i = 0; i < copy.size(); ++i) {
        result.push_back(copy[i].acceleration);
    }
    return result;
}

struct IntegratorResult {
    std::vector<Vector3D> positions;
    std::vector<Vector3D> velocities;
};

IntegratorResult capture_state(const Universe& universe) {
    IntegratorResult result;
    result.positions.reserve(universe.size());
    result.velocities.reserve(universe.size());
    for (std::size_t i = 0; i < universe.size(); ++i) {
        result.positions.push_back(universe[i].position);
        result.velocities.push_back(universe[i].velocity);
    }
    return result;
}

IntegratorResult advance_with(Integrator& integrator, const Universe& snapshot, double dt) {
    Universe copy = snapshot;
    integrator.step(copy, dt);
    return capture_state(copy);
}

double max_abs_diff(const std::vector<Vector3D>& ref, const std::vector<Vector3D>& test) {
    double max_err = 0.0;
    for (std::size_t i = 0; i < ref.size(); ++i) {
        Vector3D d = test[i] - ref[i];
        max_err = std::max(max_err, std::fabs(d.x));
        max_err = std::max(max_err, std::fabs(d.y));
        max_err = std::max(max_err, std::fabs(d.z));
    }
    return max_err;
}

template <typename T>
void print_vector(const std::string& label, const T& value) {
    std::cout << "    " << label << ": " << value << "\n";
}

int main() {
    bool stress_mode = should_run_stress();
    auto scenarios = build_scenarios(stress_mode);
    if (stress_mode) {
        std::cout << "Stress scenarios enabled (UNISIM_REGRESSION_STRESS)\n";
    }

#ifdef __APPLE__
    std::unique_ptr<MetalBackend> metal_backend = std::make_unique<MetalBackend>();
    bool metal_ready = metal_backend->initialize();
    std::vector<MetalConfig> metal_configs;
    if (metal_ready) {
        const double metal_relax = 500.0;
        auto available_forces = metal_backend->get_force_methods();
        auto available_integrators = metal_backend->get_integrators();
        for (const auto& force_name : available_forces) {
            auto force_cfg = find_force_config(force_name);
            double force_pos_scale = force_cfg ? force_cfg->pos_tolerance_scale : 1.0;
            double force_vel_scale = force_cfg ? force_cfg->vel_tolerance_scale : 1.0;
            for (const auto& integrator_name : available_integrators) {
                auto base_tol = get_integrator_tolerance(integrator_name);
                metal_configs.push_back({
                    "Metal " + force_name + " + " + integrator_name,
                    integrator_name,
                    force_name,
                    base_tol.first * metal_relax * force_pos_scale,
                    base_tol.second * metal_relax * force_vel_scale
                });
            }
        }
    } else {
        std::cout << "Metal backend initialization failed, skipping Metal tests\n";
        metal_backend.reset();
    }
#endif

#ifdef UNISIM_USE_CUDA
    std::unique_ptr<CudaBackend> cuda_backend = std::make_unique<CudaBackend>();
    bool cuda_ready = cuda_backend->initialize() && cuda_backend->is_available();
    std::vector<CudaConfig> cuda_configs;
    if (cuda_ready) {
        const double cuda_relax = 8.0;
        auto available_forces = cuda_backend->get_force_methods();
        auto available_integrators = cuda_backend->get_integrators();
        for (const auto& force_name : available_forces) {
            for (const auto& integrator_name : available_integrators) {
                auto tol = get_integrator_tolerance(integrator_name);
                cuda_configs.push_back({
                    "CUDA " + force_name + " + " + integrator_name,
                    integrator_name,
                    force_name,
                    tol.first * cuda_relax,
                    tol.second * cuda_relax
                });
            }
        }
    } else {
        std::cout << "CUDA backend initialization failed, skipping CUDA tests\n";
        cuda_backend.reset();
    }
#endif

    bool passed = true;

    for (const auto& scenario : scenarios) {
        std::cout << "Scenario: " << scenario.name << "\n";

        BruteForce brute_fc;
        auto ref_acc = capture_accelerations(brute_fc, scenario.universe);

        RungeKuttaIntegrator rk_brute(std::make_shared<BruteForce>());
        auto rk_ref = advance_with(rk_brute, scenario.universe, scenario.dt);

        for (const auto& force_cfg : kCpuForceConfigs) {
            bool skip_for_size = (scenario.universe.size() > kAccurateForceBodyLimit) && force_cfg.name != "Brute Force";
            if (skip_for_size) {
                std::cout << "  Force " << force_cfg.name << " skipped for large scenario\n";
                continue;
            }

            auto fc_for_force = force_cfg.factory();
            auto acc = capture_accelerations(*fc_for_force, scenario.universe);
            double force_err = max_abs_diff(ref_acc, acc);
            std::cout << "  Force " << force_cfg.name
                      << " diff=" << force_err
                      << " (tol " << force_cfg.force_tolerance << ")\n";
            if (force_err > force_cfg.force_tolerance) {
                std::cout << "        ERROR: force error above tolerance\n";
                passed = false;
            }

            for (const auto& integrator_name : kIntegratorNames) {
                auto fc = force_cfg.factory();
                auto integrator = create_integrator(integrator_name, fc);
                if (!integrator) continue;
                auto result = advance_with(*integrator, scenario.universe, scenario.dt);
                auto base_tol = get_integrator_tolerance(integrator_name);
                double pos_tol = base_tol.first * force_cfg.pos_tolerance_scale;
                double vel_tol = base_tol.second * force_cfg.vel_tolerance_scale;
                double pos_err = max_abs_diff(rk_ref.positions, result.positions);
                double vel_err = max_abs_diff(rk_ref.velocities, result.velocities);
                std::cout << "    " << force_cfg.name << " + " << integrator_name
                          << " pos_err=" << pos_err
                          << " vel_err=" << vel_err
                          << " (tol " << pos_tol << ", " << vel_tol << ")\n";
                if (pos_err > pos_tol || vel_err > vel_tol) {
                    std::cout << "        ERROR: integrator drift above tolerance\n";
                    passed = false;
                }
            }
        }

#ifdef __APPLE__
        if (metal_ready && metal_backend) {
            for (const auto& cfg : metal_configs) {
                bool skip_for_size = scenario.universe.size() > kAccurateForceBodyLimit;
                if (skip_for_size) {
                    std::cout << "    " << cfg.label << " skipped for large scenario\n";
                    continue;
                }
                Universe metal_snapshot = scenario.universe;
                metal_backend->set_force_method(cfg.force_method);
                metal_backend->set_integrator(cfg.integrator);
                metal_backend->step(metal_snapshot, scenario.dt);
                auto metal_state = capture_state(metal_snapshot);
                double pos_err = max_abs_diff(rk_ref.positions, metal_state.positions);
                double vel_err = max_abs_diff(rk_ref.velocities, metal_state.velocities);
                std::cout << "    " << cfg.label
                          << " pos_err=" << pos_err
                          << " vel_err=" << vel_err
                          << " (tol " << cfg.pos_tolerance << ", " << cfg.vel_tolerance << ")\n";
                if (pos_err > cfg.pos_tolerance || vel_err > cfg.vel_tolerance) {
                    std::cout << "        ERROR: metal drift above tolerance\n";
                    passed = false;
                }
            }
        } else {
            std::cout << "    Metal backend unavailable, skipping Metal checks\n";
        }
#endif

#ifdef UNISIM_USE_CUDA
        if (cuda_ready && cuda_backend) {
            for (const auto& cfg : cuda_configs) {
                Universe cuda_snapshot = scenario.universe;
                cuda_backend->set_force_method(cfg.force_method);
                cuda_backend->set_integrator(cfg.integrator);
                cuda_backend->step(cuda_snapshot, scenario.dt);
                auto cuda_state = capture_state(cuda_snapshot);
                double pos_err = max_abs_diff(rk_ref.positions, cuda_state.positions);
                double vel_err = max_abs_diff(rk_ref.velocities, cuda_state.velocities);
                std::cout << "    " << cfg.label
                          << " pos_err=" << pos_err
                          << " vel_err=" << vel_err
                          << " (tol " << cfg.pos_tolerance << ", " << cfg.vel_tolerance << ")\n";
                if (pos_err > cfg.pos_tolerance || vel_err > cfg.vel_tolerance) {
                    std::cout << "        ERROR: cuda drift above tolerance\n";
                    passed = false;
                }
            }
        } else {
            std::cout << "    CUDA backend unavailable, skipping CUDA checks\n";
        }
#endif
    }

    if (!passed) {
        std::cerr << "Regression suite FAILED\n";
#ifdef __APPLE__
        if (metal_backend) {
            metal_backend->shutdown();
        }
#endif
#ifdef UNISIM_USE_CUDA
        if (cuda_backend) {
            cuda_backend->shutdown();
        }
#endif
        return 1;
    }

    std::cout << "Regression suite PASSED\n";
#ifdef __APPLE__
    if (metal_backend) {
        metal_backend->shutdown();
    }
#endif
#ifdef UNISIM_USE_CUDA
    if (cuda_backend) {
        cuda_backend->shutdown();
    }
#endif
    return 0;
}


