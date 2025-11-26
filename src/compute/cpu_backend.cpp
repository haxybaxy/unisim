#include "cpu_backend.hpp"
#include "../simulation/integrators/euler.hpp"
#include "../simulation/integrators/verlet.hpp"
#include "../simulation/integrators/leapfrog.hpp"
#include "../simulation/integrators/runge_kutta.hpp"
#include "../simulation/force_computers/brute_force.hpp"
#include "../simulation/force_computers/barnes_hut.hpp"
#include "../simulation/force_computers/fast_multipole.hpp"
#include <memory>
#include <iostream>

namespace unisim {

CpuBackend::CpuBackend() {
}

bool CpuBackend::initialize() {
    // Initialize force computers
    force_computers_["Brute Force"] = std::make_shared<BruteForce>();
    auto barnes_hut = std::make_shared<BarnesHut>();
    // Increase theta for better performance (slightly less accurate but much faster)
    barnes_hut->set_theta(0.7);
    force_computers_["Barnes-Hut"] = barnes_hut;
    auto fast_multipole = std::make_shared<FastMultipole>();
    fast_multipole->set_theta(0.45);
    force_computers_["Fast Multipole"] = fast_multipole;
    
    // Set default force computer
    current_force_computer_ = force_computers_["Brute Force"];
    
    // Initialize integrators
    recreate_integrators();
    
    // Set default integrator
    set_integrator("Runge-Kutta (RK4)");
    
    return true;
}

void CpuBackend::shutdown() {
    integrators_.clear();
    force_computers_.clear();
}

bool CpuBackend::is_available() const {
    return true;
}

const char* CpuBackend::name() const {
    return "CPU";
}

void CpuBackend::step(Universe& universe, double dt) {
    if (current_integrator_) {
        current_integrator_->step(universe, dt);
    }
}

std::vector<std::string> CpuBackend::get_integrators() const {
    std::vector<std::string> names;
    for (const auto& pair : integrators_) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> CpuBackend::get_force_methods() const {
    std::vector<std::string> names;
    for (const auto& pair : force_computers_) {
        names.push_back(pair.first);
    }
    return names;
}

void CpuBackend::set_integrator(const std::string& name) {
    auto it = integrators_.find(name);
    if (it != integrators_.end()) {
        current_integrator_ = it->second;
    } else {
        // Fallback or log error
        std::cerr << "Integrator not found: " << name << std::endl;
        if (!integrators_.empty()) {
            current_integrator_ = integrators_.begin()->second;
        }
    }
}

void CpuBackend::set_force_method(const std::string& name) {
    auto it = force_computers_.find(name);
    if (it != force_computers_.end()) {
        current_force_computer_ = it->second;
        // Recreate integrators because they hold a reference to force computer
        std::string current_integrator_name = current_integrator_ ? current_integrator_->name() : "";
        recreate_integrators();
        if (!current_integrator_name.empty()) {
            set_integrator(current_integrator_name);
        } else {
            // Reset integrator state even if name is empty (new integrator created)
            if (auto verlet = std::dynamic_pointer_cast<VerletIntegrator>(current_integrator_)) {
                verlet->reset();
            }
        }
    }
}

void CpuBackend::recreate_integrators() {
    integrators_.clear();
    if (!current_force_computer_) return;
    
    integrators_["Euler"] = std::make_shared<EulerIntegrator>(current_force_computer_);
    integrators_["Verlet"] = std::make_shared<VerletIntegrator>(current_force_computer_);
    integrators_["Leapfrog"] = std::make_shared<LeapfrogIntegrator>(current_force_computer_);
    integrators_["Runge-Kutta (RK4)"] = std::make_shared<RungeKuttaIntegrator>(current_force_computer_);
}

} // namespace unisim

