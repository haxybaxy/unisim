#pragma once

#include "force_computer.hpp"
#include "../vector3d.hpp"
#include <memory>
#include <vector>

namespace unisim {

/**
 * @brief Barnes-Hut O(N log N) force computation using octree
 * 
 * Uses a hierarchical octree to approximate forces from distant groups of bodies,
 * significantly faster than brute-force for large N.
 */
class BarnesHut : public ForceComputer {
public:
    BarnesHut(double gravitational_constant = 1.0, double softening = 0.01, double theta = 0.5)
        : G_(gravitational_constant), softening_(softening), theta_(theta) {}

    void compute_forces(Universe& universe) override;

    const char* name() const override {
        return "Barnes-Hut";
    }

    void set_gravitational_constant(double G) {
        G_ = G;
    }

    void set_softening(double softening) {
        softening_ = softening;
    }

    void set_theta(double theta) {
        theta_ = theta; // Opening angle threshold (typically 0.5)
    }

private:
    // Octree node for spatial subdivision
    struct OctreeNode {
        Vector3D center;        // Center of this cell
        Vector3D com;           // Center of mass
        double size;            // Size of this cell (half-width)
        double total_mass;      // Total mass in this node
        bool is_leaf;
        std::size_t body_count; // Number of bodies represented by this node (1 for true leaf)
        std::vector<std::size_t> leaf_body_indices; // Actual body indices when leaf aggregates >1 body
        
        // For leaf nodes
        std::size_t body_index; // Index into universe array (valid when body_count == 1)
        
        // For internal nodes: 8 children (octree)
        std::unique_ptr<OctreeNode> children[8];
        
        OctreeNode(const Vector3D& center, double size)
            : center(center), size(size), total_mass(0.0), is_leaf(true), body_count(0), body_index(0) {
            com = Vector3D(0.0, 0.0, 0.0);
            for (int i = 0; i < 8; ++i) {
                children[i] = nullptr;
            }
        }
    };

    void build_tree(OctreeNode* node, const Universe& universe, 
                    const std::vector<std::size_t>& body_indices);
    void build_tree_recursive(OctreeNode* node, const Universe& universe, 
                              const std::vector<std::size_t>& body_indices, int depth);
    void compute_force_from_node(const OctreeNode* node, const Universe& universe,
                                 std::size_t body_index, const Vector3D& position,
                                 Vector3D& acceleration) const;
    int get_octant(const Vector3D& point, const Vector3D& center) const;
    Vector3D get_octant_center(const Vector3D& parent_center, double parent_size, int octant) const;
    
    double G_;          // Gravitational constant
    double softening_;  // Softening parameter
    double theta_;      // Opening angle threshold (s/d < theta to use approximation)
    
    std::unique_ptr<OctreeNode> root_;
};

} // namespace unisim

