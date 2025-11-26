#pragma once

#include "../simulation/universe.hpp"
#include <string>
#include <vector>

namespace unisim {

/**
 * @brief Abstract base class for compute backends (CPU/GPU)
 */
class ComputeBackend {
public:
    virtual ~ComputeBackend() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool is_available() const = 0;
    virtual const char* name() const = 0;

    // Simulation Control
    virtual void step(Universe& universe, double dt) = 0;

    // Configuration
    virtual std::vector<std::string> get_integrators() const = 0;
    virtual std::vector<std::string> get_force_methods() const = 0;
    virtual void set_integrator(const std::string& name) = 0;
    virtual void set_force_method(const std::string& name) = 0;
};

} // namespace unisim

