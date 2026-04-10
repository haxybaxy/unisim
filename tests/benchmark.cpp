#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <cmath>
#include <memory>
#include <string>
#include <iomanip>

#include "../src/simulation/universe.hpp"
#include "../src/simulation/body.hpp"
#include "../src/simulation/vector3d.hpp"

#ifdef __APPLE__
#include "../src/compute/metal_backend.hpp"
#endif


using namespace unisim;

Universe make_plummer_universe(std::size_t n, double a = 1.0, double total_mass = 1.0, unsigned seed = 42) {
    Universe universe;
    universe.reserve(n);

    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    double body_mass = total_mass / n;

    for (std::size_t i = 0; i < n; ++i) {
        // Radius via inverse CDF: r = a / sqrt(X^(-2/3) - 1)
        double X = unit(gen);
        while (X < 1e-10) X = unit(gen);
        double r = a / std::sqrt(std::pow(X, -2.0 / 3.0) - 1.0);

        // Uniform direction on sphere
        double costheta = 2.0 * unit(gen) - 1.0;
        double sintheta = std::sqrt(1.0 - costheta * costheta);
        double phi = 2.0 * M_PI * unit(gen);

        // Velocity via rejection sampling (Aarseth, Henon & Wielen 1974)
        double v_esc = std::sqrt(2.0) * std::pow(1.0 + (r * r) / (a * a), -0.25);
        double q, g;
        do {
            q = unit(gen);
            g = unit(gen);
        } while (g > q * q * std::pow(1.0 - q * q, 3.5));
        double v = v_esc * q;

        double v_costheta = 2.0 * unit(gen) - 1.0;
        double v_sintheta = std::sqrt(1.0 - v_costheta * v_costheta);
        double v_phi = 2.0 * M_PI * unit(gen);

        Body b;
        b.position = {r * sintheta * std::cos(phi),
                      r * sintheta * std::sin(phi),
                      r * costheta};
        b.velocity = {v * v_sintheta * std::cos(v_phi),
                      v * v_sintheta * std::sin(v_phi),
                      v * v_costheta};
        b.mass = body_mass;
        b.radius = 0.05;
        universe.add_body(std::move(b));
    }
    return universe;
}

struct BenchResult {
    std::size_t n;
    int steps;
    double total_ms;
    double avg_step_ms;
    double std_step_ms;
    double ci95_ms;
    double cv;
    double bodies_per_sec;
};

BenchResult run_benchmark(ComputeBackend& backend, std::size_t n, int warmup_steps, int bench_steps, double dt) {
    Universe universe = make_plummer_universe(n);

    // Warmup
    for (int i = 0; i < warmup_steps; ++i) {
        backend.step(universe, dt);
    }

    // Benchmark — time each step individually
    std::vector<double> step_times(bench_steps);
    for (int i = 0; i < bench_steps; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        backend.step(universe, dt);
        auto end = std::chrono::high_resolution_clock::now();
        step_times[i] = std::chrono::duration<double, std::milli>(end - start).count();
    }

    // Mean
    double sum = 0.0;
    for (double t : step_times) sum += t;
    double mean = sum / bench_steps;

    // Std dev
    double sq_sum = 0.0;
    for (double t : step_times) sq_sum += (t - mean) * (t - mean);
    double std_dev = std::sqrt(sq_sum / bench_steps);

    // CI95 = 1.96 * std / sqrt(n_samples)
    double ci95 = 1.96 * std_dev / std::sqrt(static_cast<double>(bench_steps));

    // CV = std / mean
    double cv = (mean > 0.0) ? std_dev / mean : 0.0;

    double total_ms = sum;
    double steps_per_sec = 1000.0 / mean;
    double bodies_per_sec = steps_per_sec * n;

    return {n, bench_steps, total_ms, mean, std_dev, ci95, cv, bodies_per_sec};
}

int main(int argc, char** argv) {
    std::vector<std::size_t> body_counts = {1000, 5000, 10000, 50000, 100000};
    int warmup_steps = 50;
    int bench_steps = 100;
    double dt = 0.001;

    // Parse optional args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--steps" && i + 1 < argc) {
            bench_steps = std::stoi(argv[++i]);
        } else if (arg == "--warmup" && i + 1 < argc) {
            warmup_steps = std::stoi(argv[++i]);
        } else if (arg == "--dt" && i + 1 < argc) {
            dt = std::stod(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: benchmark [--steps N] [--warmup N] [--dt F]\n";
            return 0;
        }
    }

#ifdef __APPLE__
    auto metal = std::make_unique<MetalBackend>();
    if (!metal->initialize() || !metal->is_available()) {
        std::cerr << "Metal backend not available\n";
        return 1;
    }

    metal->set_force_method("Barnes-Hut");
    metal->set_integrator("Leapfrog");
    metal->set_theta(0.75);
    metal->set_softening(0.5);

    std::cout << "backend,force_method,integrator,theta,softening,n,steps,total_ms,avg_step_ms,std_step_ms,ci95_ms,cv,bodies_per_sec\n";

    for (std::size_t n : body_counts) {
        auto result = run_benchmark(*metal, n, warmup_steps, bench_steps, dt);
        std::cout << std::fixed << std::setprecision(4)
                  << "Metal,Barnes-Hut,Leapfrog,0.75,0.5,"
                  << result.n << ","
                  << result.steps << ","
                  << std::setprecision(3) << result.total_ms << ","
                  << result.avg_step_ms << ","
                  << result.std_step_ms << ","
                  << result.ci95_ms << ","
                  << std::setprecision(4) << result.cv << ","
                  << std::setprecision(0) << result.bodies_per_sec << "\n";
    }

    metal->shutdown();
#else
    std::cerr << "Metal backend only available on macOS\n";
    return 1;
#endif

    return 0;
}
