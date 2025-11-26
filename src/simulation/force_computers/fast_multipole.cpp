#include "fast_multipole.hpp"
#include "../body.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace unisim {

namespace {
constexpr double kEpsilon = 1e-12;
}

FastMultipole::FastMultipole(double gravitational_constant,
                             double softening,
                             double theta,
                             std::size_t max_bodies_per_leaf,
                             int max_depth)
    : G_(gravitational_constant),
      softening_(softening),
      theta_(theta),
      max_bodies_per_leaf_(max_bodies_per_leaf),
      max_depth_(max_depth),
      min_cell_size_(1e-6) {
}

void FastMultipole::reset_universe(Universe& universe) const {
    for (std::size_t i = 0; i < universe.size(); ++i) {
        universe[i].acceleration = Vector3D(0.0, 0.0, 0.0);
    }
}

void FastMultipole::build_tree(const Universe& universe) {
    leaves_.clear();
    root_.reset();

    if (universe.empty()) {
        return;
    }

    Vector3D min_pos = universe[0].position;
    Vector3D max_pos = universe[0].position;

    for (std::size_t i = 1; i < universe.size(); ++i) {
        const Vector3D& p = universe[i].position;
        min_pos.x = std::min(min_pos.x, p.x);
        min_pos.y = std::min(min_pos.y, p.y);
        min_pos.z = std::min(min_pos.z, p.z);

        max_pos.x = std::max(max_pos.x, p.x);
        max_pos.y = std::max(max_pos.y, p.y);
        max_pos.z = std::max(max_pos.z, p.z);
    }

    Vector3D span = max_pos - min_pos;
    double max_span = std::max({span.x, span.y, span.z});

    if (max_span < 1e-6) {
        max_span = 1.0;
        min_pos = min_pos - Vector3D(0.5, 0.5, 0.5);
        max_pos = min_pos + Vector3D(1.0, 1.0, 1.0);
    } else {
        Vector3D padding = span * 0.1;
        min_pos = min_pos - padding;
        max_pos = max_pos + padding;
    }

    Vector3D center = (min_pos + max_pos) * 0.5;
    double half_size = std::max({max_pos.x - min_pos.x,
                                 max_pos.y - min_pos.y,
                                 max_pos.z - min_pos.z}) * 0.5;

    if (half_size < 1e-4) {
        half_size = 1.0;
    }

    root_ = std::make_unique<Node>(center, half_size, nullptr);
    std::vector<std::size_t> indices(universe.size());
    for (std::size_t i = 0; i < universe.size(); ++i) {
        indices[i] = i;
    }

    build_tree_recursive(root_.get(), universe, indices, 0);
}

void FastMultipole::build_tree_recursive(Node* node,
                                         const Universe& universe,
                                         const std::vector<std::size_t>& body_indices,
                                         int depth) {
    if (!node || body_indices.empty()) {
        return;
    }

    node->bodies = body_indices;
    node->is_leaf = true;

    bool stop = (body_indices.size() <= max_bodies_per_leaf_) ||
                (depth >= max_depth_) ||
                (node->half_size < min_cell_size_);

    if (stop) {
        leaves_.push_back(node);
        return;
    }

    std::vector<std::size_t> octant_bodies[8];
    for (std::size_t idx : body_indices) {
        int octant = get_octant(universe[idx].position, node->center);
        octant_bodies[octant].push_back(idx);
    }

    int active_octants = 0;
    for (int i = 0; i < 8; ++i) {
        if (!octant_bodies[i].empty()) {
            ++active_octants;
        }
    }

    if (active_octants <= 1) {
        leaves_.push_back(node);
        return;
    }

    node->is_leaf = false;
    node->bodies.clear();

    double child_half_size = node->half_size * 0.5;
    for (int octant = 0; octant < 8; ++octant) {
        if (octant_bodies[octant].empty()) {
            node->children[octant].reset();
            continue;
        }

        Vector3D child_center = get_octant_center(node->center, node->half_size, octant);
        node->children[octant] = std::make_unique<Node>(child_center, child_half_size, node);
        build_tree_recursive(node->children[octant].get(), universe, octant_bodies[octant], depth + 1);
    }
}

int FastMultipole::get_octant(const Vector3D& point, const Vector3D& center) const {
    int octant = 0;
    if (point.x >= center.x) octant |= 1;
    if (point.y >= center.y) octant |= 2;
    if (point.z >= center.z) octant |= 4;
    return octant;
}

Vector3D FastMultipole::get_octant_center(const Vector3D& parent_center, double parent_half_size, int octant) const {
    double offset = parent_half_size * 0.5;
    Vector3D child_center = parent_center;

    child_center.x += (octant & 1) ? offset : -offset;
    child_center.y += (octant & 2) ? offset : -offset;
    child_center.z += (octant & 4) ? offset : -offset;

    return child_center;
}

void FastMultipole::compute_multipole(Node* node, const Universe& universe) {
    if (!node) return;

    node->mass = 0.0;
    node->dipole = Vector3D(0.0, 0.0, 0.0);

    if (node->is_leaf) {
        for (std::size_t idx : node->bodies) {
            const Body& body = universe[idx];
            node->mass += body.mass;
            node->dipole += (body.position - node->center) * body.mass;
        }
        return;
    }

    for (const auto& child : node->children) {
        if (!child) continue;
        compute_multipole(child.get(), universe);
        if (child->mass <= 0.0) continue;
        node->mass += child->mass;
        Vector3D shift = child->center - node->center;
        node->dipole += child->dipole + shift * child->mass;
    }
}

bool FastMultipole::well_separated(const Node* a, const Node* b) const {
    if (!a || !b || a == b) return false;
    double dist = (a->center - b->center).magnitude();
    double size = a->half_size + b->half_size;
    if (dist <= kEpsilon) {
        return false;
    }
    return (size / dist) < theta_;
}

double FastMultipole::acceptance_ratio(const Node* a, const Node* b) const {
    if (!a || !b) return std::numeric_limits<double>::infinity();
    double dist = (a->center - b->center).magnitude();
    if (dist <= kEpsilon) {
        return std::numeric_limits<double>::infinity();
    }
    double size = a->half_size + b->half_size;
    return size / dist;
}

void FastMultipole::apply_multipole(const Node* source, Node* target) {
    if (!source || !target) return;
    if (source->mass <= 0.0) return;

    Vector3D delta = target->center - source->center;
    double r2 = delta.magnitude_squared() + softening_ * softening_;
    double r = std::sqrt(r2);
    if (r <= kEpsilon) {
        return;
    }

    double inv_r = 1.0 / r;
    double inv_r2 = 1.0 / r2;
    double inv_r3 = inv_r * inv_r2;
    double inv_r5 = inv_r3 * inv_r2;

    double potential = G_ * source->mass * inv_r;
    Vector3D gradient = delta * (-G_ * source->mass * inv_r3);

    if (source->dipole.magnitude_squared() > 0.0) {
        double dot = source->dipole.dot(delta);
        potential += G_ * dot * inv_r3;
        Vector3D dipole_grad = source->dipole * (G_ * inv_r3) -
                               delta * (3.0 * G_ * dot * inv_r5);
        gradient += dipole_grad;
    }

    target->local_potential += potential;
    target->local_gradient += gradient;
}

void FastMultipole::compute_direct_within(Node* node, Universe& universe) {
    if (!node || node->bodies.size() < 2) return;

    const auto& bodies = node->bodies;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        std::size_t idx_i = bodies[i];
        Body& body_i = universe[idx_i];
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            std::size_t idx_j = bodies[j];
            Body& body_j = universe[idx_j];

            Vector3D r = body_j.position - body_i.position;
            double dist_sq = r.magnitude_squared() + softening_ * softening_;
            double dist = std::sqrt(dist_sq);
            if (dist <= kEpsilon) continue;

            double inv_dist3 = 1.0 / (dist_sq * dist);
            Vector3D scaled = r * (G_ * inv_dist3);

            body_i.acceleration += scaled * body_j.mass;
            body_j.acceleration -= scaled * body_i.mass;
        }
    }
}

void FastMultipole::compute_direct(Node* a, Node* b, Universe& universe) {
    if (!a || !b) return;
    if (a->bodies.empty() || b->bodies.empty()) return;

    for (std::size_t idx_i : a->bodies) {
        Body& body_i = universe[idx_i];
        for (std::size_t idx_j : b->bodies) {
            if (idx_i == idx_j) continue;
            Body& body_j = universe[idx_j];

            Vector3D r = body_j.position - body_i.position;
            double dist_sq = r.magnitude_squared() + softening_ * softening_;
            double dist = std::sqrt(dist_sq);
            if (dist <= kEpsilon) continue;

            double inv_dist3 = 1.0 / (dist_sq * dist);
            Vector3D scaled = r * (G_ * inv_dist3);

            body_i.acceleration += scaled * body_j.mass;
            body_j.acceleration -= scaled * body_i.mass;
        }
    }
}

void FastMultipole::process_self(Node* node, Universe& universe) {
    if (!node) return;

    if (node->is_leaf) {
        compute_direct_within(node, universe);
        return;
    }

    for (int i = 0; i < 8; ++i) {
        if (node->children[i]) {
            process_self(node->children[i].get(), universe);
        }
    }

    for (int i = 0; i < 8; ++i) {
        if (!node->children[i]) continue;
        for (int j = i + 1; j < 8; ++j) {
            if (!node->children[j]) continue;
            process_pair(node->children[i].get(), node->children[j].get(), universe);
        }
    }
}

void FastMultipole::process_pair(Node* a, Node* b, Universe& universe) {
    if (!a || !b) return;
    if (a == b) {
        process_self(a, universe);
        return;
    }

    if (well_separated(a, b)) {
        apply_multipole(a, b);
        apply_multipole(b, a);
        return;
    }

    if (a->is_leaf && b->is_leaf) {
        compute_direct(a, b, universe);
        return;
    }

    if ((a->half_size >= b->half_size && !a->is_leaf) || b->is_leaf) {
        for (auto& child : a->children) {
            if (child) {
                process_pair(child.get(), b, universe);
            }
        }
    } else {
        for (auto& child : b->children) {
            if (child) {
                process_pair(a, child.get(), universe);
            }
        }
    }
}

void FastMultipole::propagate_local(Node* node) {
    if (!node) return;

    for (auto& child : node->children) {
        if (!child) continue;
        Vector3D offset = child->center - node->center;
        child->local_potential += node->local_potential + node->local_gradient.dot(offset);
        child->local_gradient += node->local_gradient;
        propagate_local(child.get());
    }
}

void FastMultipole::apply_far_field(Universe& universe) const {
    for (Node* leaf : leaves_) {
        if (!leaf) continue;
        Vector3D far_accel = leaf->local_gradient * (-1.0);
        for (std::size_t idx : leaf->bodies) {
            universe[idx].acceleration += far_accel;
        }
    }
}

void FastMultipole::compute_forces(Universe& universe) {
    const std::size_t count = universe.size();
    if (count == 0) return;

    reset_universe(universe);
    if (count == 1) return;

    build_tree(universe);
    if (!root_) return;

    compute_multipole(root_.get(), universe);
    root_->local_potential = 0.0;
    root_->local_gradient = Vector3D(0.0, 0.0, 0.0);

    process_pair(root_.get(), root_.get(), universe);
    propagate_local(root_.get());
    apply_far_field(universe);
}

} // namespace unisim


