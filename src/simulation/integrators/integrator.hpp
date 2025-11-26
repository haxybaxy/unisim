#pragma once

#include "../universe.hpp"

namespace unisim {

/**
 * @brief Abstract base class for numerical integrators
 */
class Integrator {
public:
    virtual ~Integrator() = default;

    /**
     * @brief Advance the simulation by one time step
     * @param universe The universe containing all bodies
     * @param dt Time step size
     */
    virtual void step(Universe& universe, double dt) = 0;

    /**
     * @brief Get the name of the integrator
     */
    virtual const char* name() const = 0;
};

} // namespace unisim

