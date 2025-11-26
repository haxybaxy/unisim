#include "metal_compute.hpp"
#include <iostream>

namespace unisim {

MetalCompute::MetalCompute() {
    backend_ = std::make_shared<GpuBackend>();
    if (backend_->is_available()) {
        if (!backend_->initialize()) {
            std::cerr << "Failed to initialize Metal backend" << std::endl;
        }
    } else {
        std::cerr << "Metal backend not available on this platform" << std::endl;
    }
}

MetalCompute::~MetalCompute() {
    if (backend_) {
        backend_->shutdown();
    }
}

void MetalCompute::compute_forces(Universe& universe) {
    if (backend_ && backend_->is_available()) {
        backend_->compute_forces(universe);
    }
}

const char* MetalCompute::name() const {
    return "Metal GPU";
}

} // namespace unisim

