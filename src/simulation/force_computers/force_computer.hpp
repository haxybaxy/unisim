#pragma once

#include "../universe.hpp"

namespace unisim {

/**
 * @brief Abstract base class for force computation methods
 */
class ForceComputer {
public:
    virtual ~ForceComputer() = default;

    /**
     * @brief Compute forces for all bodies in the universe
     * @param universe The universe containing all bodies
     * 
     * This method should update the acceleration field of each body
     * based on gravitational interactions.
     */
    virtual void compute_forces(Universe& universe) = 0;

    /**
     * @brief Get the name of the force computation method
     */
    virtual const char* name() const = 0;
};

} // namespace unisim

