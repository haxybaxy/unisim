#pragma once

#include "body.hpp"
#include <vector>
#include <memory>
#include <cstddef>

namespace unisim {

/**
 * @brief Container for all bodies in the simulation
 */
class Universe {
public:
    Universe() = default;
    ~Universe() = default;

    // Copyable and movable
    Universe(const Universe&) = default;
    Universe& operator=(const Universe&) = default;
    Universe(Universe&&) = default;
    Universe& operator=(Universe&&) = default;

    void add_body(const Body& body) {
        bodies_.push_back(body);
    }

    void add_body(Body&& body) {
        bodies_.push_back(std::move(body));
    }

    void clear() {
        bodies_.clear();
    }

    std::size_t size() const {
        return bodies_.size();
    }

    bool empty() const {
        return bodies_.empty();
    }

    Body& operator[](std::size_t index) {
        return bodies_[index];
    }

    const Body& operator[](std::size_t index) const {
        return bodies_[index];
    }

    Body* data() {
        return bodies_.data();
    }

    const Body* data() const {
        return bodies_.data();
    }

    std::vector<Body>::iterator begin() {
        return bodies_.begin();
    }

    std::vector<Body>::iterator end() {
        return bodies_.end();
    }

    std::vector<Body>::const_iterator begin() const {
        return bodies_.begin();
    }

    std::vector<Body>::const_iterator end() const {
        return bodies_.end();
    }

    void reserve(std::size_t capacity) {
        bodies_.reserve(capacity);
    }

private:
    std::vector<Body> bodies_;
};

} // namespace unisim

