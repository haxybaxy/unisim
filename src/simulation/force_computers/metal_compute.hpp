#pragma once

#include "force_computer.hpp"
#include "../../compute/gpu_backend.hpp"
#include <memory>

namespace unisim {

class MetalCompute : public ForceComputer {
public:
    MetalCompute();
    ~MetalCompute() override;
    
    void compute_forces(Universe& universe) override;
    const char* name() const override;

private:
    std::shared_ptr<GpuBackend> backend_;
};

} // namespace unisim

