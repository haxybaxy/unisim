#pragma once

#include "../simulation/universe.hpp"
#include <cairo.h>

namespace unisim {

/**
 * @brief Abstract base class for renderers
 */
class Renderer {
public:
    virtual ~Renderer() = default;

    /**
     * @brief Render the universe using a Cairo context
     * @param cr Cairo context to render to
     * @param universe The universe to render
     * @param width Viewport width
     * @param height Viewport height
     * @param trajectories Optional trajectory history for each body (indexed by body index)
     */
    virtual void render(cairo_t* cr, const Universe& universe, int width, int height,
                       const std::vector<std::vector<Vector3D>>* trajectories = nullptr) = 0;

    /**
     * @brief Get the name of the renderer
     */
    virtual const char* name() const = 0;

    /**
     * @brief Set camera/view parameters
     */
    virtual void set_camera_position(const Vector3D& pos) {}
    virtual void set_camera_target(const Vector3D& target) {}
};

} // namespace unisim

