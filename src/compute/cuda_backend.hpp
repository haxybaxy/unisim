#pragma once

#include "compute_backend.hpp"
#include "../simulation/universe.hpp"
#include <memory>
#include <string>
#include <vector>

namespace unisim {

/**
 * @brief CUDA compute backend implementation (runtime availability checked)
 */
class CudaBackend : public ComputeBackend {
public:
    CudaBackend();
    ~CudaBackend();

    bool initialize() override;
    void shutdown() override;
    bool is_available() const override;
    const char* name() const override;

    void step(Universe& universe, double dt) override;

    std::vector<std::string> get_integrators() const override;
    std::vector<std::string> get_force_methods() const override;
    void set_integrator(const std::string& name) override;
    void set_force_method(const std::string& name) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::string current_integrator_;
    std::string current_force_method_;
};

} // namespace unisim


