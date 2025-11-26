#ifndef __APPLE__

#include "metal_backend.hpp"
#include <iostream>

namespace unisim {

class MetalBackend::Impl {};

MetalBackend::MetalBackend()
    : impl_(std::make_unique<Impl>()),
      current_integrator_("Euler"),
      current_force_method_("Brute Force") {}

MetalBackend::~MetalBackend() = default;

bool MetalBackend::initialize() {
    std::cerr << "Metal backend is not supported on this platform." << std::endl;
    return false;
}

void MetalBackend::shutdown() {
    // Nothing to clean up in the stub implementation
}

bool MetalBackend::is_available() const {
    return false;
}

const char* MetalBackend::name() const {
    return "Metal";
}

void MetalBackend::step(Universe&, double) {
    // Stub implementation does nothing
}

std::vector<std::string> MetalBackend::get_integrators() const {
    return {"Euler", "Verlet", "Leapfrog", "Runge-Kutta"};
}

std::vector<std::string> MetalBackend::get_force_methods() const {
    return {"Brute Force"};
}

void MetalBackend::set_integrator(const std::string& name) {
    current_integrator_ = name;
}

void MetalBackend::set_force_method(const std::string& name) {
    current_force_method_ = name;
}

void MetalBackend::setup_integrators() {
    // No-op
}

} // namespace unisim

#endif // __APPLE__


