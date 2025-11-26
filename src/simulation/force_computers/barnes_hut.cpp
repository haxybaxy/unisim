#include "barnes_hut.hpp"
#include "../universe.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace unisim {

void BarnesHut::compute_forces(Universe& universe) {
    const std::size_t n = universe.size();
    if (n == 0) return;
    
    // Reset accelerations
    for (std::size_t i = 0; i < n; ++i) {
        universe[i].acceleration = Vector3D(0.0, 0.0, 0.0);
    }
    
    if (n == 1) return; // No forces for single body
    
    // Find bounding box
    Vector3D min_pos = universe[0].position;
    Vector3D max_pos = universe[0].position;
    
    for (std::size_t i = 1; i < n; ++i) {
        min_pos.x = std::min(min_pos.x, universe[i].position.x);
        min_pos.y = std::min(min_pos.y, universe[i].position.y);
        min_pos.z = std::min(min_pos.z, universe[i].position.z);
        
        max_pos.x = std::max(max_pos.x, universe[i].position.x);
        max_pos.y = std::max(max_pos.y, universe[i].position.y);
        max_pos.z = std::max(max_pos.z, universe[i].position.z);
    }
    
    // Add padding to bounding box
    Vector3D padding = (max_pos - min_pos) * 0.1;
    min_pos = min_pos - padding;
    max_pos = max_pos + padding;
    
    // Calculate center and size
    Vector3D center = (min_pos + max_pos) * 0.5;
    double size = std::max({max_pos.x - min_pos.x, 
                           max_pos.y - min_pos.y, 
                           max_pos.z - min_pos.z}) * 0.5;
    
    // Ensure minimum size to avoid degeneracy when bodies are clustered tightly
    if (size < 1e-6) {
        size = 1.0;
    }
    
    // Build octree
    root_ = std::make_unique<OctreeNode>(center, size);
    std::vector<std::size_t> all_indices(n);
    for (std::size_t i = 0; i < n; ++i) {
        all_indices[i] = i;
    }
    build_tree(root_.get(), universe, all_indices);
    
    // Compute forces for each body using tree traversal
    for (std::size_t i = 0; i < n; ++i) {
        Vector3D acceleration(0.0, 0.0, 0.0);
        compute_force_from_node(root_.get(), universe, i, universe[i].position, acceleration);
        universe[i].acceleration = acceleration;
    }
}

void BarnesHut::build_tree(OctreeNode* node, const Universe& universe, 
                           const std::vector<std::size_t>& body_indices) {
    build_tree_recursive(node, universe, body_indices, 0);
}

void BarnesHut::build_tree_recursive(OctreeNode* node, const Universe& universe, 
                                     const std::vector<std::size_t>& body_indices, int depth) {
    if (body_indices.empty()) {
        return;
    }
    
    // Maximum recursion depth to prevent stack overflow and performance issues
    const int MAX_DEPTH = 30;
    if (depth > MAX_DEPTH) {
        // Force leaf node at max depth
        node->is_leaf = true;
        node->body_count = body_indices.size();
        node->body_index = body_indices[0];
        node->leaf_body_indices = body_indices;
        double total_mass = 0.0;
        Vector3D weighted_pos(0.0, 0.0, 0.0);
        for (std::size_t idx : body_indices) {
            total_mass += universe[idx].mass;
            weighted_pos = weighted_pos + universe[idx].position * universe[idx].mass;
        }
        node->total_mass = total_mass;
        if (total_mass > 0.0) {
            node->com = weighted_pos / total_mass;
        } else {
            node->com = universe[body_indices[0]].position;
        }
        return;
    }
    
    // Stop recursion if cell is too small (prevent infinite subdivision)
    // Use a more reasonable threshold relative to typical simulation scale
    const double MIN_CELL_SIZE = 1e-6;
    if (node->size < MIN_CELL_SIZE) {
        // Force leaf node for very small cells
        if (body_indices.size() == 1) {
            node->is_leaf = true;
            node->body_count = 1;
            node->body_index = body_indices[0];
            node->total_mass = universe[body_indices[0]].mass;
            node->com = universe[body_indices[0]].position;
        } else {
            // Multiple bodies in tiny cell - use first one as representative
            node->is_leaf = true;
            node->body_count = body_indices.size();
            node->body_index = body_indices[0];
            node->leaf_body_indices = body_indices;
            double total_mass = 0.0;
            Vector3D weighted_pos(0.0, 0.0, 0.0);
            for (std::size_t idx : body_indices) {
                total_mass += universe[idx].mass;
                weighted_pos = weighted_pos + universe[idx].position * universe[idx].mass;
            }
            node->total_mass = total_mass;
            if (total_mass > 0.0) {
                node->com = weighted_pos / total_mass;
            } else {
                node->com = universe[body_indices[0]].position;
            }
        }
        return;
    }
    
    if (body_indices.size() == 1) {
        // Leaf node: store the body
        node->is_leaf = true;
        node->body_count = 1;
        node->body_index = body_indices[0];
        node->leaf_body_indices = body_indices;
        node->total_mass = universe[body_indices[0]].mass;
        node->com = universe[body_indices[0]].position;
        return;
    }
    
    // Internal node: subdivide
    node->is_leaf = false;
    node->body_count = 0;
    node->leaf_body_indices.clear();
    
    // Distribute bodies into octants
    std::vector<std::vector<std::size_t>> octant_bodies(8);
    
    for (std::size_t idx : body_indices) {
        int octant = get_octant(universe[idx].position, node->center);
        octant_bodies[octant].push_back(idx);
    }
    
    // Check if all bodies ended up in the same octant (can cause infinite recursion)
    int non_empty_octants = 0;
    for (int i = 0; i < 8; ++i) {
        if (!octant_bodies[i].empty()) {
            non_empty_octants++;
        }
    }
    
    // If all bodies in same octant, force leaf node to prevent infinite recursion
    // This happens when bodies are very close together
    if (non_empty_octants == 1) {
        node->is_leaf = true;
        node->body_count = body_indices.size();
        node->body_index = body_indices[0]; // Index of representative, but body_count>1 ensures no self-skip
        node->leaf_body_indices = body_indices;
        double total_mass = 0.0;
        Vector3D weighted_pos(0.0, 0.0, 0.0);
        for (std::size_t idx : body_indices) {
            total_mass += universe[idx].mass;
            weighted_pos = weighted_pos + universe[idx].position * universe[idx].mass;
        }
        node->total_mass = total_mass;
        if (total_mass > 0.0) {
            node->com = weighted_pos / total_mass;
        } else {
            node->com = universe[body_indices[0]].position;
        }
        return;
    }
    
    // Compute center of mass for this node
    double total_mass = 0.0;
    Vector3D weighted_pos(0.0, 0.0, 0.0);
    
    for (std::size_t idx : body_indices) {
        double mass = universe[idx].mass;
        total_mass += mass;
        weighted_pos = weighted_pos + universe[idx].position * mass;
    }
    
    node->total_mass = total_mass;
    if (total_mass > 0.0) {
        node->com = weighted_pos / total_mass;
    } else {
        node->com = node->center;
    }
    
    // Recursively build children
    double child_size = node->size * 0.5;
    
    for (int octant = 0; octant < 8; ++octant) {
        if (!octant_bodies[octant].empty()) {
            Vector3D child_center = get_octant_center(node->center, node->size, octant);
            node->children[octant] = std::make_unique<OctreeNode>(child_center, child_size);
            build_tree_recursive(node->children[octant].get(), universe, octant_bodies[octant], depth + 1);
        }
    }
}

void BarnesHut::compute_force_from_node(const OctreeNode* node, const Universe& universe,
                                        std::size_t body_index, const Vector3D& position,
                                        Vector3D& acceleration) const {
    if (!node || node->total_mass == 0.0) {
        return;
    }
    
    if (node->is_leaf) {
        const auto& leaf_bodies = node->leaf_body_indices;
        bool contains_target = false;
        if (leaf_bodies.size() > 1) {
            contains_target = std::find(leaf_bodies.begin(), leaf_bodies.end(), body_index) != leaf_bodies.end();
        }
        
        if (contains_target && leaf_bodies.size() > 1) {
            // Compute direct interactions within the small leaf
            const Vector3D& target_pos = position;
            for (std::size_t idx : leaf_bodies) {
                if (idx == body_index) continue;
                const Body& other = universe[idx];
                Vector3D r = other.position - target_pos;
                double dist_sq = r.magnitude_squared() + softening_ * softening_;
                double dist = std::sqrt(dist_sq);
                double accel_mag = G_ * other.mass / dist_sq;
                Vector3D accel_dir = r / dist;
                acceleration += accel_dir * accel_mag;
            }
            return;
        }
        
        // Leaf doesn't contain the target (or only one body) – treat as aggregate
        if (node->body_count == 1 && node->body_index == body_index) {
            return;
        }
        
        Vector3D r = node->com - position;
        double dist_sq = r.magnitude_squared();
        
        // Apply softening to avoid singularities
        dist_sq += softening_ * softening_;
        double dist = std::sqrt(dist_sq);
        double accel_mag = G_ * node->total_mass / dist_sq;
        Vector3D accel_dir = r / dist;
        
        acceleration += accel_dir * accel_mag;
    } else {
        // Internal node: check if we can use approximation
        Vector3D r = node->com - position;
        double dist_sq = r.magnitude_squared();
        double dist = std::sqrt(dist_sq);
        
        // Opening angle criterion: s/d < theta (node sufficiently far)
        double s_over_d = node->size / dist;
        
        if (s_over_d < theta_) {
            // Use center of mass approximation (node is far enough)
            // Apply softening
            dist_sq += softening_ * softening_;
            // Acceleration: a = F / m_body = G * m_node / r²
            double accel_mag = G_ * node->total_mass / dist_sq;
            Vector3D accel_dir = r / dist;
            
            acceleration += accel_dir * accel_mag;
        } else {
            // Traverse children (node too close/large for approximation)
            for (int i = 0; i < 8; ++i) {
                if (node->children[i]) {
                    compute_force_from_node(node->children[i].get(), universe, body_index, position, acceleration);
                }
            }
        }
    }
}

int BarnesHut::get_octant(const Vector3D& point, const Vector3D& center) const {
    int octant = 0;
    if (point.x >= center.x) octant |= 1; // Right
    if (point.y >= center.y) octant |= 2; // Top
    if (point.z >= center.z) octant |= 4; // Front
    return octant;
}

Vector3D BarnesHut::get_octant_center(const Vector3D& parent_center, double parent_size, int octant) const {
    double offset = parent_size * 0.5;
    Vector3D center = parent_center;
    
    if (octant & 1) center.x += offset; // Right
    else center.x -= offset;            // Left
    
    if (octant & 2) center.y += offset; // Top
    else center.y -= offset;            // Bottom
    
    if (octant & 4) center.z += offset; // Front
    else center.z -= offset;            // Back
    
    return center;
}

} // namespace unisim

