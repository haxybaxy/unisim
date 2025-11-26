#ifndef UNISIM_USE_CUDA

#include "cuda_backend.hpp"
#include <iostream>

namespace unisim {

class CudaBackend::Impl {};

CudaBackend::CudaBackend()
    : impl_(std::make_unique<Impl>()),
      current_integrator_("Euler"),
      current_force_method_("Brute Force") {}

CudaBackend::~CudaBackend() = default;

bool CudaBackend::initialize() {
    std::cerr << "CUDA backend was requested but the build was compiled without CUDA support." << std::endl;
    return false;
}

void CudaBackend::shutdown() {
    // Nothing to do in the stub implementation
}

bool CudaBackend::is_available() const {
    return false;
}

const char* CudaBackend::name() const {
    return "CUDA";
}

void CudaBackend::step(Universe&, double) {
    // Intentionally left blank, since no CUDA support is available
}

std::vector<std::string> CudaBackend::get_integrators() const {
    return {"Euler", "Verlet", "Leapfrog", "Runge-Kutta"};
}

std::vector<std::string> CudaBackend::get_force_methods() const {
    return {"Brute Force"};
}

void CudaBackend::set_integrator(const std::string& name) {
    current_integrator_ = name;
}

void CudaBackend::set_force_method(const std::string& name) {
    current_force_method_ = name;
}

} // namespace unisim

#endif // UNISIM_USE_CUDA


