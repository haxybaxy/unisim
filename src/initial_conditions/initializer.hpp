#pragma once

#include "../simulation/universe.hpp"

namespace unisim {

/**
 * @brief Abstract base class for initial condition generators
 */
class Initializer {
public:
    virtual ~Initializer() = default;

    /**
     * @brief Generate initial conditions and populate the universe
     * @param universe The universe to populate
     * @param num_bodies Number of bodies to generate
     */
    virtual void initialize(Universe& universe, std::size_t num_bodies) = 0;

    /**
     * @brief Get the name of the initializer
     */
    virtual const char* name() const = 0;
};

} // namespace unisim

