#pragma once

#include "force_computer.hpp"
#include "../vector3d.hpp"
#include <memory>
#include <vector>

namespace unisim {

/**
 * @brief Fast multipole method (FMM) force computer (monopole/dipole, p=1)
 */
class FastMultipole : public ForceComputer {
public:
    FastMultipole(double gravitational_constant = 1.0,
                  double softening = 0.01,
                  double theta = 0.45,
                  std::size_t max_bodies_per_leaf = 32,
                  int max_depth = 14);

    void compute_forces(Universe& universe) override;

    const char* name() const override {
        return "Fast Multipole";
    }

    void set_gravitational_constant(double G) { G_ = G; }
    void set_softening(double softening) { softening_ = softening; }
    void set_theta(double theta) { theta_ = theta; }
    void set_max_leaf_size(std::size_t count) { max_bodies_per_leaf_ = count; }

private:
    struct Node {
        Vector3D center;
        double half_size;
        bool is_leaf;
        Node* parent;
        std::vector<std::size_t> bodies;
        std::unique_ptr<Node> children[8];

        double mass{0.0};
        Vector3D dipole{0.0, 0.0, 0.0};

        double local_potential{0.0};
        Vector3D local_gradient{0.0, 0.0, 0.0};

        explicit Node(const Vector3D& c, double size, Node* p)
            : center(c), half_size(size), is_leaf(true), parent(p) {}
    };

    double G_;
    double softening_;
    double theta_;
    std::size_t max_bodies_per_leaf_;
    int max_depth_;
    double min_cell_size_;

    std::unique_ptr<Node> root_;
    std::vector<Node*> leaves_;

    void reset_universe(Universe& universe) const;

    void build_tree(const Universe& universe);
    void build_tree_recursive(Node* node,
                              const Universe& universe,
                              const std::vector<std::size_t>& body_indices,
                              int depth);

    int get_octant(const Vector3D& point, const Vector3D& center) const;
    Vector3D get_octant_center(const Vector3D& parent_center, double parent_half_size, int octant) const;

    void compute_multipole(Node* node, const Universe& universe);
    void process_pair(Node* a, Node* b, Universe& universe);
    void process_self(Node* node, Universe& universe);
    void compute_direct(Node* a, Node* b, Universe& universe);
    void compute_direct_within(Node* node, Universe& universe);
    void apply_multipole(const Node* source, Node* target);
    bool well_separated(const Node* a, const Node* b) const;

    void propagate_local(Node* node);
    void apply_far_field(Universe& universe) const;

    double acceptance_ratio(const Node* a, const Node* b) const;
};

} // namespace unisim


