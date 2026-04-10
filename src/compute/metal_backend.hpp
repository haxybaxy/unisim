#pragma once

#include "compute_backend.hpp"
#include "../simulation/universe.hpp"
#include <string>
#include <vector>
#include <memory>

namespace unisim {

/**
 * @brief Metal compute backend implementation
 */
class MetalBackend : public ComputeBackend {
public:
    MetalBackend();
    ~MetalBackend();
    
    bool initialize() override;
    void shutdown() override;
    bool is_available() const override;
    const char* name() const override;

    void step(Universe& universe, double dt) override;

    std::vector<std::string> get_integrators() const override;
    std::vector<std::string> get_force_methods() const override;
    void set_integrator(const std::string& name) override;
    void set_force_method(const std::string& name) override;

    void set_softening(double s) { softening_ = s; }
    void set_theta(double t) { theta_ = t; }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    std::string current_integrator_;
    std::string current_force_method_;
    double softening_ = 0.1;
    double theta_ = 0.5;

    void setup_integrators();
};

} // namespace unisim
