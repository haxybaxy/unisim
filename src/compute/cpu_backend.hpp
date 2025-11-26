#pragma once

#include "compute_backend.hpp"
#include "../simulation/integrators/integrator.hpp"
#include "../simulation/force_computers/force_computer.hpp"
#include <map>
#include <string>
#include <memory>

namespace unisim {

/**
 * @brief CPU compute backend
 */
class CpuBackend : public ComputeBackend {
public:
    CpuBackend();
    
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
    std::map<std::string, std::shared_ptr<Integrator>> integrators_;
    std::map<std::string, std::shared_ptr<ForceComputer>> force_computers_;
    
    std::shared_ptr<Integrator> current_integrator_;
    std::shared_ptr<ForceComputer> current_force_computer_;
    
    void recreate_integrators();
};

} // namespace unisim
