#pragma once

#include "integrator.hpp"
#include "../../compute/gpu_backend.hpp"
#include <memory>

namespace unisim {

class MetalIntegrator : public Integrator {
public:
    MetalIntegrator() {
        backend_ = std::make_shared<GpuBackend>();
        if (backend_->is_available()) {
            backend_->initialize();
        }
    }

    ~MetalIntegrator() override {
        if (backend_) {
            backend_->shutdown();
        }
    }

    void step(Universe& universe, double dt) override {
        if (!backend_ || !backend_->is_available()) return;

        // Check if we need to re-upload data (e.g., first run or size changed)
        // We use universe size as a proxy for "changed"
        if (universe.size() != current_count_) {
            backend_->set_data(universe.data(), universe.size());
            current_count_ = universe.size();
        }

        // Run simulation on GPU
        backend_->step(static_cast<float>(dt));

        // Sync back to CPU for rendering
        backend_->get_data(universe.data(), universe.size());
    }

    const char* name() const override {
        return "Metal Full (GPU)";
    }

private:
    std::shared_ptr<GpuBackend> backend_;
    size_t current_count_{0};
};

} // namespace unisim

