#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "metal_backend.hpp"
#include "../simulation/universe.hpp"
#include "../simulation/body.hpp"
#include "../simulation/force_computers/barnes_hut.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <queue>

namespace unisim {

// Metal-compatible body structure (must match shader)
// Using packed struct to ensure exact byte alignment
struct __attribute__((packed)) MetalBody {
    float position[3];
    float velocity[3];
    float acceleration[3];
    float mass;
    float radius;
};

class MetalBackend::Impl {
public:
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLLibrary> library;
    id<MTLComputePipelineState> forcePipeline;
    id<MTLComputePipelineState> eulerPipeline;
    id<MTLComputePipelineState> verletPipeline;
    id<MTLComputePipelineState> leapfrogPipeline;
    id<MTLComputePipelineState> rungeKuttaPipeline;
    
    id<MTLBuffer> bodyBuffer;
    id<MTLBuffer> prevPositionBuffer; // For Verlet
    id<MTLBuffer> paramsBuffer;
    id<MTLBuffer> treeBuffer; // For hierarchical tree nodes (Barnes-Hut / FMM)
    id<MTLBuffer> leafIndexBuffer; // Flattened leaf body indices
    id<MTLComputePipelineState> barnesHutForcePipeline;
    id<MTLComputePipelineState> fmmForcePipeline;
    
    std::vector<MetalBody> metalBodies;
    std::vector<float> prevPositions; // For Verlet
    
    // Barnes-Hut tree data (flattened for GPU)
    struct FlattenedTreeNode {
        float center[3];
        float com[3];
        float size;
        float total_mass;
        int32_t is_leaf;
        int32_t body_index; // For leaf nodes
        int32_t body_count;
        int32_t body_offset;
        int32_t child_indices[8]; // For internal nodes (-1 if no child)
        float dipole[3];
        float pad;
    };
    std::vector<FlattenedTreeNode> tree_nodes_;
    std::vector<int32_t> leaf_body_indices_;
    
    bool initialized;
    
    Impl() : device(nil), commandQueue(nil), library(nil),
             forcePipeline(nil), eulerPipeline(nil), verletPipeline(nil), leapfrogPipeline(nil), rungeKuttaPipeline(nil),
             bodyBuffer(nil), prevPositionBuffer(nil), paramsBuffer(nil),
             treeBuffer(nil), leafIndexBuffer(nil),
             barnesHutForcePipeline(nil), fmmForcePipeline(nil),
             initialized(false) {}
    
    bool init() {
        @autoreleasepool {
            // Get default Metal device
            device = MTLCreateSystemDefaultDevice();
            if (!device) {
                std::cerr << "Metal is not supported on this device" << std::endl;
                return false;
            }
            
            // Create command queue
            commandQueue = [device newCommandQueue];
            if (!commandQueue) {
                std::cerr << "Failed to create Metal command queue" << std::endl;
                return false;
            }
            
            // Load shader library from embedded source
            NSError* error = nil;
            NSString* shaderSource = @R"(
#include <metal_stdlib>
using namespace metal;

struct Body {
    packed_float3 position;
    packed_float3 velocity;
    packed_float3 acceleration;
    float mass;
    float radius;
};

struct SimulationParams {
    float G;
    float softening;
    float dt;
    uint32_t num_bodies;
};

kernel void compute_forces(
    device Body* bodies [[buffer(0)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    // Skip if body has invalid mass
    if (bodies[id].mass <= 0.0 || !isfinite(bodies[id].mass)) {
        bodies[id].acceleration = packed_float3(0.0, 0.0, 0.0);
        return;
    }
    
    float3 acceleration = float3(0.0, 0.0, 0.0);
    float3 pos_i = float3(bodies[id].position);
    
    for (uint j = 0; j < params.num_bodies; ++j) {
        if (j == id) continue;
        
        // Skip if other body has invalid mass or position
        if (bodies[j].mass <= 0.0 || !isfinite(bodies[j].mass)) continue;
        
        float3 pos_j = float3(bodies[j].position);
        if (!isfinite(pos_j.x) || !isfinite(pos_j.y) || !isfinite(pos_j.z)) continue;
        
        float3 r = pos_j - pos_i;
        float dist_sq = dot(r, r) + params.softening * params.softening;
        
        // Prevent division by zero
        if (dist_sq < 1e-10) continue;
        
        float dist = sqrt(dist_sq);
        
        // Force magnitude: F = G * m1 * m2 / r²
        float force_mag = params.G * bodies[id].mass * bodies[j].mass / dist_sq;
        
        // Clamp force to prevent extreme values
        float max_force = 1e6;
        force_mag = clamp(force_mag, -max_force, max_force);
        
        // Force direction
        float3 force_dir = r / dist;
        float3 force = force_dir * force_mag;
        
        // Acceleration = Force / mass
        acceleration += force / bodies[id].mass;
    }
    
    // Clamp acceleration to prevent extreme values
    float max_accel = 1e4;
    acceleration.x = clamp(acceleration.x, -max_accel, max_accel);
    acceleration.y = clamp(acceleration.y, -max_accel, max_accel);
    acceleration.z = clamp(acceleration.z, -max_accel, max_accel);
    
    bodies[id].acceleration = packed_float3(acceleration);
}

kernel void integrate_euler(
    device Body* bodies [[buffer(0)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    float3 vel = float3(bodies[id].velocity);
    float3 accel = float3(bodies[id].acceleration);
    vel += accel * params.dt;
    
    float3 pos = float3(bodies[id].position);
    pos += vel * params.dt;
    
    bodies[id].velocity = vel;
    bodies[id].position = pos;
}

kernel void integrate_verlet(
    device Body* bodies [[buffer(0)]],
    device packed_float3* prev_positions [[buffer(2)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    float3 pos = float3(bodies[id].position);
    float3 prev_pos = prev_positions[id];
    float3 accel = float3(bodies[id].acceleration);
    
    float3 temp = pos;
    pos = 2.0 * pos - prev_pos + accel * params.dt * params.dt;
    float3 vel = (pos - prev_pos) / (2.0 * params.dt);
    
    bodies[id].position = pos;
    bodies[id].velocity = vel;
    prev_positions[id] = temp;
}

kernel void integrate_leapfrog(
    device Body* bodies [[buffer(0)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    float3 vel = float3(bodies[id].velocity);
    float3 accel = float3(bodies[id].acceleration);
    vel += accel * params.dt;
    
    float3 pos = float3(bodies[id].position);
    pos += vel * params.dt;
    
    bodies[id].velocity = vel;
    bodies[id].position = pos;
}

// Runge-Kutta 4th order integrator
// This kernel performs RK4 integration in a single pass
// It requires forces to be pre-computed (k1 stage)
// Then computes k2, k3, k4 and final update
kernel void integrate_runge_kutta(
    device Body* bodies [[buffer(0)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    // Validate inputs
    float3 pos = float3(bodies[id].position);
    float3 vel = float3(bodies[id].velocity);
    
    if (!isfinite(pos.x) || !isfinite(pos.y) || !isfinite(pos.z)) {
        return;
    }
    
    if (!isfinite(vel.x) || !isfinite(vel.y) || !isfinite(vel.z)) {
        vel = float3(0.0, 0.0, 0.0);
    }
    
    // k1: derivatives at current state (already computed in acceleration)
    float3 k1_vel = vel;  // dx/dt = velocity
    float3 k1_acc = float3(bodies[id].acceleration);  // dv/dt = acceleration
    
    if (!isfinite(k1_acc.x) || !isfinite(k1_acc.y) || !isfinite(k1_acc.z)) {
        k1_acc = float3(0.0, 0.0, 0.0);
    }
    
    // k2: derivatives at state advanced by dt/2 using k1
    float3 pos_k2 = pos + k1_vel * (params.dt * 0.5);
    float3 vel_k2 = vel + k1_acc * (params.dt * 0.5);
    
    // Compute acceleration at k2 state (force computation)
    float3 acc_k2 = float3(0.0, 0.0, 0.0);
    for (uint j = 0; j < params.num_bodies; ++j) {
        if (j == id) continue;
        
        if (bodies[j].mass <= 0.0 || !isfinite(bodies[j].mass)) continue;
        
        float3 pos_j = float3(bodies[j].position);
        if (!isfinite(pos_j.x) || !isfinite(pos_j.y) || !isfinite(pos_j.z)) continue;
        
        float3 r = pos_j - pos_k2;
        float dist_sq = dot(r, r) + params.softening * params.softening;
        if (dist_sq < 1e-10) continue;
        
        float dist = sqrt(dist_sq);
        float force_mag = params.G * bodies[id].mass * bodies[j].mass / dist_sq;
        float max_force = 1e6;
        force_mag = clamp(force_mag, -max_force, max_force);
        
        float3 force_dir = r / dist;
        float3 force = force_dir * force_mag;
        acc_k2 += force / bodies[id].mass;
    }
    
    float max_accel = 1e4;
    acc_k2.x = clamp(acc_k2.x, -max_accel, max_accel);
    acc_k2.y = clamp(acc_k2.y, -max_accel, max_accel);
    acc_k2.z = clamp(acc_k2.z, -max_accel, max_accel);
    
    // k3: derivatives at state advanced by dt/2 using k2
    float3 pos_k3 = pos + vel_k2 * (params.dt * 0.5);
    float3 vel_k3 = vel + acc_k2 * (params.dt * 0.5);
    
    // Compute acceleration at k3 state
    float3 acc_k3 = float3(0.0, 0.0, 0.0);
    for (uint j = 0; j < params.num_bodies; ++j) {
        if (j == id) continue;
        
        if (bodies[j].mass <= 0.0 || !isfinite(bodies[j].mass)) continue;
        
        float3 pos_j = float3(bodies[j].position);
        if (!isfinite(pos_j.x) || !isfinite(pos_j.y) || !isfinite(pos_j.z)) continue;
        
        float3 r = pos_j - pos_k3;
        float dist_sq = dot(r, r) + params.softening * params.softening;
        if (dist_sq < 1e-10) continue;
        
        float dist = sqrt(dist_sq);
        float force_mag = params.G * bodies[id].mass * bodies[j].mass / dist_sq;
        float max_force = 1e6;
        force_mag = clamp(force_mag, -max_force, max_force);
        
        float3 force_dir = r / dist;
        float3 force = force_dir * force_mag;
        acc_k3 += force / bodies[id].mass;
    }
    
    acc_k3.x = clamp(acc_k3.x, -max_accel, max_accel);
    acc_k3.y = clamp(acc_k3.y, -max_accel, max_accel);
    acc_k3.z = clamp(acc_k3.z, -max_accel, max_accel);
    
    // k4: derivatives at state advanced by dt using k3
    float3 pos_k4 = pos + vel_k3 * params.dt;
    float3 vel_k4 = vel + acc_k3 * params.dt;
    
    // Compute acceleration at k4 state
    float3 acc_k4 = float3(0.0, 0.0, 0.0);
    for (uint j = 0; j < params.num_bodies; ++j) {
        if (j == id) continue;
        
        if (bodies[j].mass <= 0.0 || !isfinite(bodies[j].mass)) continue;
        
        float3 pos_j = float3(bodies[j].position);
        if (!isfinite(pos_j.x) || !isfinite(pos_j.y) || !isfinite(pos_j.z)) continue;
        
        float3 r = pos_j - pos_k4;
        float dist_sq = dot(r, r) + params.softening * params.softening;
        if (dist_sq < 1e-10) continue;
        
        float dist = sqrt(dist_sq);
        float force_mag = params.G * bodies[id].mass * bodies[j].mass / dist_sq;
        float max_force = 1e6;
        force_mag = clamp(force_mag, -max_force, max_force);
        
        float3 force_dir = r / dist;
        float3 force = force_dir * force_mag;
        acc_k4 += force / bodies[id].mass;
    }
    
    acc_k4.x = clamp(acc_k4.x, -max_accel, max_accel);
    acc_k4.y = clamp(acc_k4.y, -max_accel, max_accel);
    acc_k4.z = clamp(acc_k4.z, -max_accel, max_accel);
    
    // Final RK4 update: weighted average
    float3 d_pos = (k1_vel + vel_k2 * 2.0 + vel_k3 * 2.0 + vel_k4) * (params.dt / 6.0);
    float3 d_vel = (k1_acc + acc_k2 * 2.0 + acc_k3 * 2.0 + acc_k4) * (params.dt / 6.0);
    
    pos += d_pos;
    vel += d_vel;
    
    // Clamp values
    float max_pos = 1e6;
    pos.x = clamp(pos.x, -max_pos, max_pos);
    pos.y = clamp(pos.y, -max_pos, max_pos);
    pos.z = clamp(pos.z, -max_pos, max_pos);
    
    float max_vel = 1e4;
    vel.x = clamp(vel.x, -max_vel, max_vel);
    vel.y = clamp(vel.y, -max_vel, max_vel);
    vel.z = clamp(vel.z, -max_vel, max_vel);
    
    bodies[id].position = pos;
    bodies[id].velocity = vel;
}

// Barnes-Hut tree node structure (flattened for GPU)
struct TreeNode {
    packed_float3 center;
    packed_float3 com;
    float size;
    float total_mass;
    int32_t is_leaf;
    int32_t body_index;
    int32_t body_count;
    int32_t body_offset;
    int32_t child_indices[8];
    packed_float3 dipole;
    float pad;
};

// Barnes-Hut force computation kernel
kernel void compute_forces_barnes_hut(
    device Body* bodies [[buffer(0)]],
    device TreeNode* tree_nodes [[buffer(2)]],
    constant SimulationParams& params [[buffer(1)]],
    device int32_t* leaf_indices [[buffer(3)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    // Skip if body has invalid mass
    if (bodies[id].mass <= 0.0 || !isfinite(bodies[id].mass)) {
        bodies[id].acceleration = packed_float3(0.0, 0.0, 0.0);
        return;
    }
    
    float3 pos_i = float3(bodies[id].position);
    float3 acceleration = float3(0.0, 0.0, 0.0);
    
    // Stack for tree traversal (avoid recursion)
    int32_t stack[64];
    int stack_ptr = 0;
    stack[stack_ptr++] = 0; // Start at root
    
    const float theta = 0.2; // Opening angle threshold
    
    while (stack_ptr > 0) {
        int32_t node_idx = stack[--stack_ptr];
        TreeNode node = tree_nodes[node_idx];
        
        if (node.total_mass <= 0.0) continue;
        
        if (node.is_leaf != 0) {
            bool leaf_contains_target = false;
            if (node.body_count > 1 && node.body_offset >= 0 && leaf_indices != nullptr) {
                for (int i = 0; i < node.body_count; ++i) {
                    int32_t other_idx = leaf_indices[node.body_offset + i];
                    if (other_idx == (int32_t)id) {
                        leaf_contains_target = true;
                        break;
                    }
                }
            }
            
            if (leaf_contains_target) {
                for (int i = 0; i < node.body_count; ++i) {
                    int32_t other_idx = leaf_indices[node.body_offset + i];
                    if (other_idx == (int32_t)id) continue;
                    Body other = bodies[other_idx];
                    if (other.mass <= 0.0 || !isfinite(other.mass)) continue;
                    
                    float3 other_pos = float3(other.position);
                    if (!isfinite(other_pos.x) || !isfinite(other_pos.y) || !isfinite(other_pos.z)) continue;
                    
                    float3 r = other_pos - pos_i;
                    float dist_sq = dot(r, r) + params.softening * params.softening;
                    float dist = sqrt(dist_sq);
                    float accel_mag = params.G * other.mass / dist_sq;
                    
                    float max_accel = 1e4;
                    accel_mag = clamp(accel_mag, -max_accel, max_accel);
                    
                    acceleration += (r / dist) * accel_mag;
                }
                continue;
            }
            
            if (node.body_count == 1 && node.body_index == (int32_t)id) continue; // Skip self only for true leaf
            
            float3 r = float3(node.com) - pos_i;
            float dist_sq = dot(r, r) + params.softening * params.softening;
            float dist = sqrt(dist_sq);
            float accel_mag = params.G * node.total_mass / dist_sq;
            
            float max_accel = 1e4;
            accel_mag = clamp(accel_mag, -max_accel, max_accel);
            
            float3 accel_dir = r / dist;
            acceleration += accel_dir * accel_mag;
        } else {
            // Internal node: check opening angle criterion
            float3 r = float3(node.com) - pos_i;
            float dist_sq = dot(r, r);
            float dist = sqrt(dist_sq);
            
            // Opening angle criterion: s/d < theta (node sufficiently far)
            float s_over_d = node.size / dist;
            
            if (s_over_d < theta) {
                // Use center of mass approximation (node is far enough)
                // Apply softening
                float dist_sq_soft = dist_sq + params.softening * params.softening;
                // Acceleration: a = F / m_body = G * m_node / r²
                float accel_mag = params.G * node.total_mass / dist_sq_soft;
                
                float max_accel = 1e4;
                accel_mag = clamp(accel_mag, -max_accel, max_accel);
                
                float3 accel_dir = r / dist;
                acceleration += accel_dir * accel_mag;
            } else {
                // Traverse children (node too close/large for approximation)
                for (int i = 7; i >= 0; --i) {
                    if (node.child_indices[i] >= 0 && stack_ptr < 63) { // Leave room for safety
                        stack[stack_ptr++] = node.child_indices[i];
                    }
                }
            }
        }
    }
    
    // Clamp acceleration
    float max_accel = 1e4;
    acceleration.x = clamp(acceleration.x, -max_accel, max_accel);
    acceleration.y = clamp(acceleration.y, -max_accel, max_accel);
    acceleration.z = clamp(acceleration.z, -max_accel, max_accel);
    
    bodies[id].acceleration = packed_float3(acceleration);
}

kernel void compute_forces_fmm(
    device Body* bodies [[buffer(0)]],
    device TreeNode* tree_nodes [[buffer(2)]],
    constant SimulationParams& params [[buffer(1)]],
    device int32_t* leaf_indices [[buffer(3)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    if (!tree_nodes) return;

    if (bodies[id].mass <= 0.0 || !isfinite(bodies[id].mass)) {
        bodies[id].acceleration = packed_float3(0.0, 0.0, 0.0);
        return;
    }

    float3 pos_i = float3(bodies[id].position);
    float3 acceleration = float3(0.0, 0.0, 0.0);
    const float theta = 0.25;

    int32_t stack[64];
    int stack_ptr = 0;
    stack[stack_ptr++] = 0;

    while (stack_ptr > 0) {
        int32_t node_idx = stack[--stack_ptr];
        TreeNode node = tree_nodes[node_idx];
        if (node.total_mass <= 0.0) continue;

        bool is_leaf = node.is_leaf != 0;
        bool contains_target = false;

        if (is_leaf) {
            if (node.body_count == 1 && node.body_index == (int32_t)id) {
                contains_target = true;
            } else if (node.body_count > 1 && node.body_offset >= 0 && leaf_indices != nullptr) {
                for (int i = 0; i < node.body_count; ++i) {
                    int32_t other_idx = leaf_indices[node.body_offset + i];
                    if (other_idx == (int32_t)id) {
                        contains_target = true;
                        break;
                    }
                }
            }
        }

        if (is_leaf && contains_target && node.body_count > 1 && leaf_indices != nullptr) {
            for (int i = 0; i < node.body_count; ++i) {
                int32_t other_idx = leaf_indices[node.body_offset + i];
                if (other_idx == (int32_t)id) continue;
                Body other = bodies[other_idx];
                if (other.mass <= 0.0 || !isfinite(other.mass)) continue;
                float3 r = float3(other.position) - pos_i;
                float dist_sq = dot(r, r) + params.softening * params.softening;
                if (dist_sq < 1e-10) continue;
                float inv_dist = rsqrt(dist_sq);
                float inv_dist3 = inv_dist / dist_sq;
                acceleration += r * (params.G * other.mass * inv_dist3);
            }
            continue;
        } else if (is_leaf && contains_target) {
            continue;
        }

        float3 r = float3(node.com) - pos_i;
        float dist_sq = dot(r, r) + params.softening * params.softening;
        float dist = sqrt(dist_sq);
        if (dist_sq < 1e-10) continue;

        if (is_leaf) {
            float size_ratio = node.size / dist;
            if (size_ratio >= theta && leaf_indices != nullptr) {
                if (node.body_count == 1) {
                    int32_t other_idx = node.body_index;
                    if (other_idx != (int32_t)id) {
                        Body other = bodies[other_idx];
                        if (other.mass > 0.0 && isfinite(other.mass)) {
                            float3 r_body = float3(other.position) - pos_i;
                            float dist_sq_body = dot(r_body, r_body) + params.softening * params.softening;
                            if (dist_sq_body >= 1e-10) {
                                float inv_dist = rsqrt(dist_sq_body);
                                float inv_dist3 = inv_dist / dist_sq_body;
                                acceleration += r_body * (params.G * other.mass * inv_dist3);
                            }
                        }
                    }
                } else if (node.body_offset >= 0) {
                    for (int i = 0; i < node.body_count; ++i) {
                        int32_t other_idx = leaf_indices[node.body_offset + i];
                        if (other_idx == (int32_t)id) continue;
                        Body other = bodies[other_idx];
                        if (other.mass <= 0.0 || !isfinite(other.mass)) continue;
                        float3 r_body = float3(other.position) - pos_i;
                        float dist_sq_body = dot(r_body, r_body) + params.softening * params.softening;
                        if (dist_sq_body < 1e-10) continue;
                        float inv_dist = rsqrt(dist_sq_body);
                        float inv_dist3 = inv_dist / dist_sq_body;
                        acceleration += r_body * (params.G * other.mass * inv_dist3);
                    }
                }
                continue;
            }
        } else {
            float size_ratio = node.size / dist;
            if (size_ratio >= theta) {
                for (int i = 7; i >= 0; --i) {
                    if (node.child_indices[i] >= 0 && stack_ptr < 63) {
                        stack[stack_ptr++] = node.child_indices[i];
                    }
                }
                continue;
            }
        }

        float inv_dist = 1.0f / dist;
        float inv_dist2 = 1.0f / dist_sq;
        float inv_dist3 = inv_dist * inv_dist2;
        float inv_dist5 = inv_dist3 * inv_dist2;

        float3 accel = r * (params.G * node.total_mass * inv_dist3);

        float3 dipole = float3(node.dipole);
        float dotDR = dot(dipole, r);
        accel += (-params.G) * dipole * inv_dist3;
        accel += (3.0f * params.G * dotDR) * r * inv_dist5;

        acceleration += accel;
    }

    float max_accel = 1e4;
    acceleration.x = clamp(acceleration.x, -max_accel, max_accel);
    acceleration.y = clamp(acceleration.y, -max_accel, max_accel);
    acceleration.z = clamp(acceleration.z, -max_accel, max_accel);

    bodies[id].acceleration = packed_float3(acceleration);
}
)";
            
            library = [device newLibraryWithSource:shaderSource options:nil error:&error];
            if (!library) {
                std::cerr << "Failed to compile Metal shaders: " << 
                    (error ? [error.localizedDescription UTF8String] : "Unknown error") << std::endl;
                return false;
            }
            
            // Create compute pipeline states
            id<MTLFunction> forceFunction = [library newFunctionWithName:@"compute_forces"];
            if (!forceFunction) {
                std::cerr << "Failed to find compute_forces function" << std::endl;
                return false;
            }
            
            forcePipeline = [device newComputePipelineStateWithFunction:forceFunction error:&error];
            if (!forcePipeline) {
                std::cerr << "Failed to create force pipeline: " << 
                    (error ? [error.localizedDescription UTF8String] : "Unknown error") << std::endl;
                return false;
            }
            
            id<MTLFunction> eulerFunction = [library newFunctionWithName:@"integrate_euler"];
            if (eulerFunction) {
                eulerPipeline = [device newComputePipelineStateWithFunction:eulerFunction error:&error];
            }
            
            id<MTLFunction> verletFunction = [library newFunctionWithName:@"integrate_verlet"];
            if (verletFunction) {
                verletPipeline = [device newComputePipelineStateWithFunction:verletFunction error:&error];
            }
            
            id<MTLFunction> leapfrogFunction = [library newFunctionWithName:@"integrate_leapfrog"];
            if (leapfrogFunction) {
                leapfrogPipeline = [device newComputePipelineStateWithFunction:leapfrogFunction error:&error];
            }
            
            id<MTLFunction> rungeKuttaFunction = [library newFunctionWithName:@"integrate_runge_kutta"];
            if (rungeKuttaFunction) {
                rungeKuttaPipeline = [device newComputePipelineStateWithFunction:rungeKuttaFunction error:&error];
                if (!rungeKuttaPipeline && error) {
                    std::cerr << "Warning: Failed to create RK4 pipeline: " << 
                        [error.localizedDescription UTF8String] << std::endl;
                }
            }
            
            id<MTLFunction> barnesHutForceFunction = [library newFunctionWithName:@"compute_forces_barnes_hut"];
            if (barnesHutForceFunction) {
                barnesHutForcePipeline = [device newComputePipelineStateWithFunction:barnesHutForceFunction error:&error];
                if (!barnesHutForcePipeline && error) {
                    std::cerr << "Warning: Failed to create Barnes-Hut force pipeline: " << 
                        [error.localizedDescription UTF8String] << std::endl;
                } else {
                    std::cerr << "Metal: Barnes-Hut force pipeline created successfully" << std::endl;
                }
            } else {
                std::cerr << "Warning: Barnes-Hut force function not found in Metal library" << std::endl;
            }
            
            id<MTLFunction> fmmForceFunction = [library newFunctionWithName:@"compute_forces_fmm"];
            if (fmmForceFunction) {
                fmmForcePipeline = [device newComputePipelineStateWithFunction:fmmForceFunction error:&error];
                if (!fmmForcePipeline && error) {
                    std::cerr << "Warning: Failed to create Fast Multipole pipeline: "
                              << [error.localizedDescription UTF8String] << std::endl;
                } else {
                    std::cerr << "Metal: Fast Multipole force pipeline created successfully" << std::endl;
                }
            } else {
                std::cerr << "Warning: Fast Multipole force function not found in Metal library" << std::endl;
            }
            
            initialized = true;
            return true;
        }
    }
    
    void cleanup() {
        @autoreleasepool {
            bodyBuffer = nil;
            prevPositionBuffer = nil;
            paramsBuffer = nil;
            forcePipeline = nil;
            eulerPipeline = nil;
            verletPipeline = nil;
            leapfrogPipeline = nil;
            rungeKuttaPipeline = nil;
            barnesHutForcePipeline = nil;
            treeBuffer = nil;
            leafIndexBuffer = nil;
            fmmForcePipeline = nil;
            library = nil;
            commandQueue = nil;
            device = nil;
            initialized = false;
        }
    }
    
    void syncToMetal(const Universe& universe) {
        if (!initialized || !device) return;
        
        size_t n = universe.size();
        if (n == 0) return;
        
        metalBodies.resize(n);
        prevPositions.resize(n * 3);
        
        // Copy data from universe to Metal structures
        for (size_t i = 0; i < n; ++i) {
            const Body& body = universe[i];
            MetalBody& mb = metalBodies[i];
            
            // Validate and sanitize position
            if (std::isfinite(body.position.x) && std::isfinite(body.position.y) && std::isfinite(body.position.z)) {
                mb.position[0] = static_cast<float>(body.position.x);
                mb.position[1] = static_cast<float>(body.position.y);
                mb.position[2] = static_cast<float>(body.position.z);
            } else {
                // Reset to origin if invalid
                mb.position[0] = 0.0f;
                mb.position[1] = 0.0f;
                mb.position[2] = 0.0f;
            }
            
            // Validate and sanitize velocity
            if (std::isfinite(body.velocity.x) && std::isfinite(body.velocity.y) && std::isfinite(body.velocity.z)) {
                mb.velocity[0] = static_cast<float>(body.velocity.x);
                mb.velocity[1] = static_cast<float>(body.velocity.y);
                mb.velocity[2] = static_cast<float>(body.velocity.z);
            } else {
                mb.velocity[0] = 0.0f;
                mb.velocity[1] = 0.0f;
                mb.velocity[2] = 0.0f;
            }
            
            // Validate and sanitize acceleration
            if (std::isfinite(body.acceleration.x) && std::isfinite(body.acceleration.y) && std::isfinite(body.acceleration.z)) {
                mb.acceleration[0] = static_cast<float>(body.acceleration.x);
                mb.acceleration[1] = static_cast<float>(body.acceleration.y);
                mb.acceleration[2] = static_cast<float>(body.acceleration.z);
            } else {
                mb.acceleration[0] = 0.0f;
                mb.acceleration[1] = 0.0f;
                mb.acceleration[2] = 0.0f;
            }
            
            // Validate mass
            if (std::isfinite(body.mass) && body.mass > 0.0) {
                mb.mass = static_cast<float>(body.mass);
            } else {
                mb.mass = 1.0f; // Default mass
            }
            
            mb.radius = static_cast<float>(body.radius);
            
            // Initialize previous positions for Verlet
            prevPositions[i * 3 + 0] = mb.position[0];
            prevPositions[i * 3 + 1] = mb.position[1];
            prevPositions[i * 3 + 2] = mb.position[2];
        }
        
        // Update buffers if size changed or buffers don't exist
        NSUInteger requiredBodySize = n * sizeof(MetalBody);
        NSUInteger requiredPosSize = n * 3 * sizeof(float);
        
        if (!bodyBuffer || [bodyBuffer length] < requiredBodySize) {
            bodyBuffer = [device newBufferWithBytes:metalBodies.data()
                                              length:requiredBodySize
                                             options:MTLResourceStorageModeShared];
            if (!bodyBuffer) {
                std::cerr << "Failed to create Metal body buffer" << std::endl;
                return;
            }
        } else {
            // Copy to existing buffer
            void* bufferPtr = [bodyBuffer contents];
            if (bufferPtr) {
                memcpy(bufferPtr, metalBodies.data(), requiredBodySize);
            }
        }
        
        if (!prevPositionBuffer || [prevPositionBuffer length] < requiredPosSize) {
            prevPositionBuffer = [device newBufferWithBytes:prevPositions.data()
                                                     length:requiredPosSize
                                                    options:MTLResourceStorageModeShared];
            if (!prevPositionBuffer) {
                std::cerr << "Failed to create Metal previous position buffer" << std::endl;
                return;
            }
        } else {
            // Copy to existing buffer
            void* bufferPtr = [prevPositionBuffer contents];
            if (bufferPtr) {
                memcpy(bufferPtr, prevPositions.data(), requiredPosSize);
            }
        }
    }
    
    void syncFromMetal(Universe& universe) {
        if (!initialized || !bodyBuffer) {
            std::cerr << "Cannot sync from Metal: not initialized or buffer is nil" << std::endl;
            return;
        }
        
        size_t n = universe.size();
        if (n == 0) {
            std::cerr << "Warning: syncing from Metal with empty universe" << std::endl;
            return;
        }
        
        // Ensure metalBodies is the right size
        if (metalBodies.size() < n) {
            std::cerr << "Warning: metalBodies size mismatch, resizing from " 
                      << metalBodies.size() << " to " << n << std::endl;
            metalBodies.resize(n);
        }
        
        void* bufferPtr = [bodyBuffer contents];
        if (!bufferPtr) {
            std::cerr << "Failed to get Metal buffer contents" << std::endl;
            return;
        }
        
        NSUInteger bufferSize = [bodyBuffer length];
        NSUInteger requiredSize = n * sizeof(MetalBody);
        
        if (bufferSize < requiredSize) {
            std::cerr << "Metal buffer too small: " << bufferSize << " < " << requiredSize << std::endl;
            return;
        }
        
        // Copy from Metal buffer to our intermediate storage
        memcpy(metalBodies.data(), bufferPtr, requiredSize);
        
        // Copy from intermediate storage to universe, with bounds checking
        size_t copyCount = std::min(n, metalBodies.size());
        for (size_t i = 0; i < copyCount; ++i) {
            Body& body = universe[i];
            const MetalBody& mb = metalBodies[i];
            
            // Validate and fix invalid positions
            bool hasInvalidPos = !std::isfinite(mb.position[0]) || !std::isfinite(mb.position[1]) || !std::isfinite(mb.position[2]);
            bool hasInvalidVel = !std::isfinite(mb.velocity[0]) || !std::isfinite(mb.velocity[1]) || !std::isfinite(mb.velocity[2]);
            bool hasInvalidAccel = !std::isfinite(mb.acceleration[0]) || !std::isfinite(mb.acceleration[1]) || !std::isfinite(mb.acceleration[2]);
            
            if (hasInvalidPos || hasInvalidVel || hasInvalidAccel) {
                // Only log first few warnings to avoid spam
                static int warning_count = 0;
                if (warning_count++ < 5) {
                    std::cerr << "Warning: Invalid data for body " << i 
                              << " (pos:" << hasInvalidPos << " vel:" << hasInvalidVel << " accel:" << hasInvalidAccel << ")" << std::endl;
                }
                
                // Reset to last known good values (don't update if invalid)
                // Keep current universe values for invalid fields
                if (!hasInvalidPos) {
                    body.position.x = mb.position[0];
                    body.position.y = mb.position[1];
                    body.position.z = mb.position[2];
                }
                
                if (!hasInvalidVel) {
                    body.velocity.x = mb.velocity[0];
                    body.velocity.y = mb.velocity[1];
                    body.velocity.z = mb.velocity[2];
                } else {
                    // Reset velocity to zero if invalid
                    body.velocity.x = 0.0;
                    body.velocity.y = 0.0;
                    body.velocity.z = 0.0;
                }
                
                if (!hasInvalidAccel) {
                    body.acceleration.x = mb.acceleration[0];
                    body.acceleration.y = mb.acceleration[1];
                    body.acceleration.z = mb.acceleration[2];
                } else {
                    // Reset acceleration to zero if invalid
                    body.acceleration.x = 0.0;
                    body.acceleration.y = 0.0;
                    body.acceleration.z = 0.0;
                }
                
                continue;
            }
            
            // All data is valid, copy everything
            body.position.x = mb.position[0];
            body.position.y = mb.position[1];
            body.position.z = mb.position[2];
            
            body.velocity.x = mb.velocity[0];
            body.velocity.y = mb.velocity[1];
            body.velocity.z = mb.velocity[2];
            
            body.acceleration.x = mb.acceleration[0];
            body.acceleration.y = mb.acceleration[1];
            body.acceleration.z = mb.acceleration[2];
        }
    }

    void initializeVerletHistory(const Universe& universe, double dt) {
        if (!initialized || dt <= 0.0) {
            return;
        }

        size_t n = universe.size();
        if (n == 0) return;

        if (prevPositions.size() < n * 3) {
            prevPositions.resize(n * 3);
        }

        for (size_t i = 0; i < n; ++i) {
            const Body& body = universe[i];
            prevPositions[i * 3 + 0] = static_cast<float>(body.position.x - body.velocity.x * dt);
            prevPositions[i * 3 + 1] = static_cast<float>(body.position.y - body.velocity.y * dt);
            prevPositions[i * 3 + 2] = static_cast<float>(body.position.z - body.velocity.z * dt);
        }

        if (prevPositionBuffer) {
            void* bufferPtr = [prevPositionBuffer contents];
            if (bufferPtr) {
                memcpy(bufferPtr, prevPositions.data(), n * 3 * sizeof(float));
            }
        }
    }
    
    void buildBarnesHutTree(const Universe& universe) {
        // Build tree directly (don't call compute_forces, that's wasteful)
        tree_nodes_.clear();
        leaf_body_indices_.clear();
        
        if (universe.size() == 0) return;
        
        // Find bounding box
        Vector3D min_pos = universe[0].position;
        Vector3D max_pos = universe[0].position;
        
        for (size_t i = 1; i < universe.size(); ++i) {
            min_pos.x = std::min(min_pos.x, universe[i].position.x);
            min_pos.y = std::min(min_pos.y, universe[i].position.y);
            min_pos.z = std::min(min_pos.z, universe[i].position.z);
            
            max_pos.x = std::max(max_pos.x, universe[i].position.x);
            max_pos.y = std::max(max_pos.y, universe[i].position.y);
            max_pos.z = std::max(max_pos.z, universe[i].position.z);
        }
        
        Vector3D span = max_pos - min_pos;
        double max_span = std::max({span.x, span.y, span.z});
        
        // If all bodies are at same position, create a small box around them
        if (max_span < 1e-10) {
            min_pos = min_pos - Vector3D(1.0, 1.0, 1.0);
            max_pos = max_pos + Vector3D(1.0, 1.0, 1.0);
            span = max_pos - min_pos;
            max_span = std::max({span.x, span.y, span.z});
        }
        
        Vector3D padding = span * 0.1;
        min_pos = min_pos - padding;
        max_pos = max_pos + padding;
        
        Vector3D center = (min_pos + max_pos) * 0.5;
        double size = std::max({max_pos.x - min_pos.x, 
                                max_pos.y - min_pos.y, 
                                max_pos.z - min_pos.z}) * 0.5;
        
        // Ensure minimum size to prevent numerical issues
        if (size < 1e-6) {
            size = 1.0;
        }
        
        // Build tree recursively and flatten it
        std::vector<std::size_t> all_indices(universe.size());
        for (size_t i = 0; i < universe.size(); ++i) {
            all_indices[i] = i;
        }
        
        flattenTreeRecursive(universe, center, size, all_indices, 0, 0);
    }
    
    int flattenTreeRecursive(const Universe& universe, const Vector3D& center, double size,
                            const std::vector<std::size_t>& body_indices, int parent_idx, int depth = 0) {
        if (body_indices.empty()) {
            return -1; // No node created
        }

        auto compute_leaf_dipole = [&](FlattenedTreeNode& node_ref) {
            Vector3D node_center(static_cast<double>(node_ref.center[0]),
                                 static_cast<double>(node_ref.center[1]),
                                 static_cast<double>(node_ref.center[2]));
            Vector3D dipole(0.0, 0.0, 0.0);
            for (std::size_t idx : body_indices) {
                const Body& body = universe[idx];
                dipole += (body.position - node_center) * body.mass;
            }
            node_ref.dipole[0] = static_cast<float>(dipole.x);
            node_ref.dipole[1] = static_cast<float>(dipole.y);
            node_ref.dipole[2] = static_cast<float>(dipole.z);
            node_ref.pad = 0.0f;
        };
        
        auto compute_internal_dipole = [&](FlattenedTreeNode& node_ref) {
            Vector3D node_center(static_cast<double>(node_ref.center[0]),
                                 static_cast<double>(node_ref.center[1]),
                                 static_cast<double>(node_ref.center[2]));
            Vector3D dipole(0.0, 0.0, 0.0);
            for (int child = 0; child < 8; ++child) {
                int child_idx = node_ref.child_indices[child];
                if (child_idx < 0) continue;
                const auto& child_node = tree_nodes_[child_idx];
                Vector3D child_center(child_node.center[0], child_node.center[1], child_node.center[2]);
                Vector3D child_dipole(child_node.dipole[0], child_node.dipole[1], child_node.dipole[2]);
                dipole += child_dipole + (child_center - node_center) * child_node.total_mass;
            }
            node_ref.dipole[0] = static_cast<float>(dipole.x);
            node_ref.dipole[1] = static_cast<float>(dipole.y);
            node_ref.dipole[2] = static_cast<float>(dipole.z);
            node_ref.pad = 0.0f;
        };
        
        // Maximum recursion depth to prevent stack overflow and performance issues
        const int MAX_DEPTH = 30;
        if (depth > MAX_DEPTH) {
            // Force leaf node at max depth
            int node_idx = static_cast<int>(tree_nodes_.size());
        FlattenedTreeNode node;
            node.center[0] = static_cast<float>(center.x);
            node.center[1] = static_cast<float>(center.y);
            node.center[2] = static_cast<float>(center.z);
            node.size = static_cast<float>(size);
            node.is_leaf = 1;
            node.body_index = static_cast<int32_t>(body_indices[0]);
        node.body_count = static_cast<int32_t>(body_indices.size());
        if (node.body_count > 1) {
            node.body_offset = static_cast<int32_t>(leaf_body_indices_.size());
            for (std::size_t idx : body_indices) {
                leaf_body_indices_.push_back(static_cast<int32_t>(idx));
            }
        } else {
            node.body_offset = -1;
        }
            
            double total_mass = 0.0;
            Vector3D weighted_pos(0.0, 0.0, 0.0);
            for (std::size_t idx : body_indices) {
                total_mass += universe[idx].mass;
                weighted_pos = weighted_pos + universe[idx].position * universe[idx].mass;
            }
            node.total_mass = static_cast<float>(total_mass);
            if (total_mass > 0.0) {
                Vector3D com = weighted_pos / total_mass;
                node.com[0] = static_cast<float>(com.x);
                node.com[1] = static_cast<float>(com.y);
                node.com[2] = static_cast<float>(com.z);
            } else {
                node.com[0] = static_cast<float>(universe[body_indices[0]].position.x);
                node.com[1] = static_cast<float>(universe[body_indices[0]].position.y);
                node.com[2] = static_cast<float>(universe[body_indices[0]].position.z);
            }
            
            for (int i = 0; i < 8; ++i) {
                node.child_indices[i] = -1;
            }
            
            compute_leaf_dipole(node);
            tree_nodes_.push_back(node);
            return node_idx;
        }
        
        // Stop recursion if cell is too small (prevent infinite subdivision)
        // Use a more reasonable threshold relative to typical simulation scale
        const double MIN_CELL_SIZE = 1e-6;
        if (size < MIN_CELL_SIZE) {
            // Force leaf node for very small cells
            int node_idx = static_cast<int>(tree_nodes_.size());
        FlattenedTreeNode node;
            node.center[0] = static_cast<float>(center.x);
            node.center[1] = static_cast<float>(center.y);
            node.center[2] = static_cast<float>(center.z);
            node.size = static_cast<float>(size);
            node.is_leaf = 1;
            node.body_index = static_cast<int32_t>(body_indices[0]);
        node.body_count = static_cast<int32_t>(body_indices.size());
        if (node.body_count > 1) {
            node.body_offset = static_cast<int32_t>(leaf_body_indices_.size());
            for (std::size_t idx : body_indices) {
                leaf_body_indices_.push_back(static_cast<int32_t>(idx));
            }
        } else {
            node.body_offset = -1;
        }
            
            double total_mass = 0.0;
            Vector3D weighted_pos(0.0, 0.0, 0.0);
            for (std::size_t idx : body_indices) {
                total_mass += universe[idx].mass;
                weighted_pos = weighted_pos + universe[idx].position * universe[idx].mass;
            }
            node.total_mass = static_cast<float>(total_mass);
            if (total_mass > 0.0) {
                Vector3D com = weighted_pos / total_mass;
                node.com[0] = static_cast<float>(com.x);
                node.com[1] = static_cast<float>(com.y);
                node.com[2] = static_cast<float>(com.z);
            } else {
                node.com[0] = static_cast<float>(universe[body_indices[0]].position.x);
                node.com[1] = static_cast<float>(universe[body_indices[0]].position.y);
                node.com[2] = static_cast<float>(universe[body_indices[0]].position.z);
            }
            
            for (int i = 0; i < 8; ++i) {
                node.child_indices[i] = -1;
            }
            
            compute_leaf_dipole(node);
            tree_nodes_.push_back(node);
            return node_idx;
        }
        
        int node_idx = static_cast<int>(tree_nodes_.size());
        FlattenedTreeNode node;
        node.center[0] = static_cast<float>(center.x);
        node.center[1] = static_cast<float>(center.y);
        node.center[2] = static_cast<float>(center.z);
        node.size = static_cast<float>(size);
        node.body_count = 0;
        node.body_offset = -1;
        
        // Initialize child indices
        for (int i = 0; i < 8; ++i) {
            node.child_indices[i] = -1;
        }
        
        if (body_indices.size() == 1) {
            // Leaf node
            node.is_leaf = 1;
            node.body_index = static_cast<int32_t>(body_indices[0]);
            node.body_count = static_cast<int32_t>(body_indices.size());
            node.body_offset = -1;
            node.total_mass = static_cast<float>(universe[body_indices[0]].mass);
            node.com[0] = static_cast<float>(universe[body_indices[0]].position.x);
            node.com[1] = static_cast<float>(universe[body_indices[0]].position.y);
            node.com[2] = static_cast<float>(universe[body_indices[0]].position.z);
        } else {
            // Internal node: compute COM and subdivide
            node.is_leaf = 0;
            node.body_index = -1;
            
            double total_mass = 0.0;
            Vector3D weighted_pos(0.0, 0.0, 0.0);
            
            for (std::size_t idx : body_indices) {
                double mass = universe[idx].mass;
                total_mass += mass;
                weighted_pos = weighted_pos + universe[idx].position * mass;
            }
            
            node.total_mass = static_cast<float>(total_mass);
            if (total_mass > 0.0) {
                Vector3D com = weighted_pos / total_mass;
                node.com[0] = static_cast<float>(com.x);
                node.com[1] = static_cast<float>(com.y);
                node.com[2] = static_cast<float>(com.z);
            } else {
                node.com[0] = node.center[0];
                node.com[1] = node.center[1];
                node.com[2] = node.center[2];
            }
            
            // Distribute bodies into octants
            std::vector<std::vector<std::size_t>> octant_bodies(8);
            
            for (std::size_t idx : body_indices) {
                int octant = getOctant(universe[idx].position, center);
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
                node.is_leaf = 1;
                node.body_index = static_cast<int32_t>(body_indices[0]);
                node.body_count = static_cast<int32_t>(body_indices.size());
                if (node.body_count > 1) {
                    node.body_offset = static_cast<int32_t>(leaf_body_indices_.size());
                    for (std::size_t idx : body_indices) {
                        leaf_body_indices_.push_back(static_cast<int32_t>(idx));
                    }
                }
                // COM already computed above
            } else {
                // Recursively build children
                double child_size = size * 0.5;
                for (int octant = 0; octant < 8; ++octant) {
                    if (!octant_bodies[octant].empty()) {
                        Vector3D child_center = getOctantCenter(center, size, octant);
                        int child_idx = flattenTreeRecursive(universe, child_center, child_size, 
                                                            octant_bodies[octant], node_idx, depth + 1);
                        node.child_indices[octant] = child_idx;
                    }
                }
            }
        }
        
        if (node.is_leaf != 0) {
            compute_leaf_dipole(node);
        } else {
            compute_internal_dipole(node);
        }
        tree_nodes_.push_back(node);
        return node_idx;
    }
    
    int getOctant(const Vector3D& point, const Vector3D& center) const {
        int octant = 0;
        if (point.x >= center.x) octant |= 1;
        if (point.y >= center.y) octant |= 2;
        if (point.z >= center.z) octant |= 4;
        return octant;
    }
    
    Vector3D getOctantCenter(const Vector3D& parent_center, double parent_size, int octant) const {
        double offset = parent_size * 0.5;
        Vector3D center = parent_center;
        
        if (octant & 1) center.x += offset;
        else center.x -= offset;
        
        if (octant & 2) center.y += offset;
        else center.y -= offset;
        
        if (octant & 4) center.z += offset;
        else center.z -= offset;
        
        return center;
    }
    
    void computeForces(Universe& universe, double G, double softening, const std::string& force_method) {
        if (!initialized || !device || !commandQueue) {
            std::cerr << "Metal backend not properly initialized" << std::endl;
            return;
        }
        
        @autoreleasepool {
            // Sync universe to Metal buffers
            syncToMetal(universe);
            
            if (!bodyBuffer) {
                std::cerr << "Body buffer is nil" << std::endl;
                return;
            }
            
            // Debug: Check first body before compute (only first time)
            static bool first_compute = true;
            if (first_compute && universe.size() > 0) {
                void* debugPtr = [bodyBuffer contents];
                if (debugPtr) {
                    MetalBody* debugBody = static_cast<MetalBody*>(debugPtr);
                    std::cerr << "Metal: Before compute - Body 0: pos=(" << debugBody[0].position[0] << "," 
                              << debugBody[0].position[1] << "," << debugBody[0].position[2] 
                              << ") mass=" << debugBody[0].mass << std::endl;
                    first_compute = false;
                }
            }
            
            struct SimulationParams {
                float G;
                float softening;
                float dt;
                uint32_t num_bodies;
            } params;
            
            params.G = static_cast<float>(G);
            params.softening = static_cast<float>(softening);
            params.dt = 0.0f; // Not used in force computation
            params.num_bodies = static_cast<uint32_t>(universe.size());
            
            if (!paramsBuffer || [paramsBuffer length] < sizeof(params)) {
                paramsBuffer = [device newBufferWithBytes:&params
                                                    length:sizeof(params)
                                                   options:MTLResourceStorageModeShared];
                if (!paramsBuffer) {
                    std::cerr << "Failed to create params buffer" << std::endl;
                    return;
                }
            } else {
                void* bufferPtr = [paramsBuffer contents];
                if (bufferPtr) {
                    memcpy(bufferPtr, &params, sizeof(params));
                }
            }
            
            // Check which force method to use
            bool use_barnes_hut = (force_method == "Barnes-Hut" && barnesHutForcePipeline != nil);
            bool use_fmm = (force_method == "Fast Multipole" && fmmForcePipeline != nil);
            bool use_tree = use_barnes_hut || use_fmm;
            
            if (use_tree) {
                buildBarnesHutTree(universe);
                if (!tree_nodes_.empty()) {
                    const auto& root = tree_nodes_[0];
                    std::cerr << "Metal FMM root mass=" << root.total_mass
                              << " com=(" << root.com[0] << "," << root.com[1] << "," << root.com[2] << ")"
                              << " size=" << root.size << std::endl;
                }
                
                if (tree_nodes_.empty()) {
                    std::cerr << "Warning: Hierarchical tree is empty, falling back to brute force" << std::endl;
                    use_tree = false;
                    use_barnes_hut = false;
                    use_fmm = false;
                } else {
                    NSUInteger treeSize = tree_nodes_.size() * sizeof(FlattenedTreeNode);
                    if (!treeBuffer || [treeBuffer length] < treeSize) {
                        treeBuffer = [device newBufferWithBytes:tree_nodes_.data()
                                                         length:treeSize
                                                        options:MTLResourceStorageModeShared];
                        if (!treeBuffer) {
                            std::cerr << "Failed to create tree buffer" << std::endl;
                            return;
                        }
                    } else {
                        void* bufferPtr = [treeBuffer contents];
                        if (bufferPtr) {
                            memcpy(bufferPtr, tree_nodes_.data(), treeSize);
                        }
                    }
                    
                    NSUInteger leafIndexSize = leaf_body_indices_.size() * sizeof(int32_t);
                    if (leafIndexSize > 0) {
                        if (!leafIndexBuffer || [leafIndexBuffer length] < leafIndexSize) {
                            leafIndexBuffer = [device newBufferWithBytes:leaf_body_indices_.data()
                                                                 length:leafIndexSize
                                                                options:MTLResourceStorageModeShared];
                            if (!leafIndexBuffer) {
                                std::cerr << "Failed to create leaf index buffer" << std::endl;
                                return;
                            }
                        } else {
                            void* bufferPtr = [leafIndexBuffer contents];
                            if (bufferPtr) {
                                memcpy(bufferPtr, leaf_body_indices_.data(), leafIndexSize);
                            }
                        }
                    } else {
                        leafIndexBuffer = nil;
                    }
                }
            }
            
            id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
            if (!commandBuffer) {
                std::cerr << "Failed to create command buffer" << std::endl;
                return;
            }
            
            id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
            if (!encoder) {
                std::cerr << "Failed to create compute encoder" << std::endl;
                return;
            }
            
            if (use_fmm && fmmForcePipeline && use_tree) {
                [encoder setComputePipelineState:fmmForcePipeline];
                [encoder setBuffer:bodyBuffer offset:0 atIndex:0];
                [encoder setBuffer:paramsBuffer offset:0 atIndex:1];
                [encoder setBuffer:treeBuffer offset:0 atIndex:2];
                if (leafIndexBuffer) {
                    [encoder setBuffer:leafIndexBuffer offset:0 atIndex:3];
                } else {
                    [encoder setBuffer:nil offset:0 atIndex:3];
                }
                
                MTLSize gridSize = MTLSizeMake(params.num_bodies, 1, 1);
                NSUInteger threadGroupSize = fmmForcePipeline.threadExecutionWidth;
                if (threadGroupSize == 0) threadGroupSize = 256;
                MTLSize threadgroupSize = MTLSizeMake(threadGroupSize, 1, 1);
                
                [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
            } else if (use_barnes_hut && barnesHutForcePipeline && use_tree) {
                // Use Barnes-Hut force computation
                [encoder setComputePipelineState:barnesHutForcePipeline];
                [encoder setBuffer:bodyBuffer offset:0 atIndex:0];
                [encoder setBuffer:paramsBuffer offset:0 atIndex:1];
                [encoder setBuffer:treeBuffer offset:0 atIndex:2];
                if (leafIndexBuffer) {
                    [encoder setBuffer:leafIndexBuffer offset:0 atIndex:3];
                } else {
                    [encoder setBuffer:nil offset:0 atIndex:3];
                }
                
                MTLSize gridSize = MTLSizeMake(params.num_bodies, 1, 1);
                NSUInteger threadGroupSize = barnesHutForcePipeline.threadExecutionWidth;
                if (threadGroupSize == 0) threadGroupSize = 256;
                MTLSize threadgroupSize = MTLSizeMake(threadGroupSize, 1, 1);
                
                [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
            } else {
                // Use brute-force
                [encoder setComputePipelineState:forcePipeline];
                [encoder setBuffer:bodyBuffer offset:0 atIndex:0];
                [encoder setBuffer:paramsBuffer offset:0 atIndex:1];
                
                MTLSize gridSize = MTLSizeMake(params.num_bodies, 1, 1);
                NSUInteger threadGroupSize = forcePipeline.threadExecutionWidth;
                if (threadGroupSize == 0) threadGroupSize = 256;
                MTLSize threadgroupSize = MTLSizeMake(threadGroupSize, 1, 1);
                
                [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
            }
            
            [encoder endEncoding];
            
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];
            
            // Check for errors
            if (commandBuffer.error) {
                std::cerr << "Metal command buffer error: " << 
                    [[commandBuffer.error localizedDescription] UTF8String] << std::endl;
                return;
            }
            
            // Debug: Check first body after compute (only first time)
            static bool first_after_compute = true;
            if (first_after_compute && universe.size() > 0) {
                void* debugPtr = [bodyBuffer contents];
                if (debugPtr) {
                    MetalBody* debugBody = static_cast<MetalBody*>(debugPtr);
                    std::cerr << "Metal: After compute - Body 0: accel=(" << debugBody[0].acceleration[0] << "," 
                              << debugBody[0].acceleration[1] << "," << debugBody[0].acceleration[2] << ")" << std::endl;
                    first_after_compute = false;
                }
            }
            
            // Don't sync from Metal here - wait until after integration
            // The force computation only updates accelerations in the Metal buffer
        }
    }
    
    void integrate(Universe& universe, double dt, const std::string& method) {
        if (!initialized || !device || !commandQueue) return;
        
        @autoreleasepool {
            // Ensure buffers are synced (they should be from computeForces, but double-check)
            if (!bodyBuffer || !paramsBuffer) {
                std::cerr << "Required buffers are nil - syncing to Metal" << std::endl;
                syncToMetal(universe);
                if (!bodyBuffer || !paramsBuffer) {
                    std::cerr << "Failed to sync buffers" << std::endl;
                    return;
                }
            }
            
            id<MTLComputePipelineState> pipeline = nil;
            
            if (method == "Euler" && eulerPipeline) {
                pipeline = eulerPipeline;
            } else if (method == "Verlet" && verletPipeline) {
                pipeline = verletPipeline;
            } else if (method == "Leapfrog" && leapfrogPipeline) {
                pipeline = leapfrogPipeline;
            } else if ((method == "Runge-Kutta" || method == "RK4" || method == "Runge Kutta") && rungeKuttaPipeline) {
                pipeline = rungeKuttaPipeline;
            } else {
                // Fallback to Euler
                pipeline = eulerPipeline;
            }
            
            if (!pipeline) {
                std::cerr << "No valid integration pipeline for method: " << method << std::endl;
                return;
            }
            
            struct SimulationParams {
                float G;
                float softening;
                float dt;
                uint32_t num_bodies;
            } params;
            
            // RK4 needs G and softening for internal force computation
            // Other integrators don't use these
            if (method == "Runge-Kutta" || method == "RK4" || method == "Runge Kutta") {
                params.G = 1.0f; // Default G value
                params.softening = 0.1f; // Default softening
            } else {
                params.G = 0.0f; // Not used in other integrators
                params.softening = 0.0f; // Not used in other integrators
            }
            params.dt = static_cast<float>(dt);
            params.num_bodies = static_cast<uint32_t>(universe.size());
            
            void* bufferPtr = [paramsBuffer contents];
            if (bufferPtr) {
                memcpy(bufferPtr, &params, sizeof(params));
            }
            
            id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
            if (!commandBuffer) {
                std::cerr << "Failed to create command buffer for integration" << std::endl;
                return;
            }
            
            id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
            if (!encoder) {
                std::cerr << "Failed to create compute encoder for integration" << std::endl;
                return;
            }
            
            [encoder setComputePipelineState:pipeline];
            [encoder setBuffer:bodyBuffer offset:0 atIndex:0];
            [encoder setBuffer:paramsBuffer offset:0 atIndex:1];
            
            if (method == "Verlet" && prevPositionBuffer) {
                [encoder setBuffer:prevPositionBuffer offset:0 atIndex:2];
            }
            
            MTLSize gridSize = MTLSizeMake(params.num_bodies, 1, 1);
            NSUInteger threadGroupSize = pipeline.threadExecutionWidth;
            if (threadGroupSize == 0) threadGroupSize = 256; // Fallback
            MTLSize threadgroupSize = MTLSizeMake(threadGroupSize, 1, 1);
            
            [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
            [encoder endEncoding];
            
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];
            
            // Check for errors
            if (commandBuffer.error) {
                std::cerr << "Metal integration error: " << 
                    [[commandBuffer.error localizedDescription] UTF8String] << std::endl;
                return;
            }
            
            // Debug: Check first body after integration (only first time)
            static bool first_after_integrate = true;
            if (first_after_integrate && universe.size() > 0) {
                void* debugPtr = [bodyBuffer contents];
                if (debugPtr) {
                    MetalBody* debugBody = static_cast<MetalBody*>(debugPtr);
                    std::cerr << "Metal: After integrate - Body 0: pos=(" << debugBody[0].position[0] << "," 
                              << debugBody[0].position[1] << "," << debugBody[0].position[2] 
                              << ") vel=(" << debugBody[0].velocity[0] << "," 
                              << debugBody[0].velocity[1] << "," << debugBody[0].velocity[2] << ")" << std::endl;
                    first_after_integrate = false;
                }
            }
            
            // Now sync the integrated results back to the universe
            syncFromMetal(universe);
        }
    }
};

MetalBackend::MetalBackend() : impl_(std::make_unique<Impl>()), 
                               current_integrator_("Euler"),
                               current_force_method_("Brute Force") {
}

MetalBackend::~MetalBackend() {
    shutdown();
}

bool MetalBackend::initialize() {
    if (!impl_->init()) {
        return false;
    }
    setup_integrators();
    return true;
}

void MetalBackend::shutdown() {
    if (impl_) {
        impl_->cleanup();
    }
}

bool MetalBackend::is_available() const {
    return impl_ && impl_->initialized;
}

const char* MetalBackend::name() const {
    return "Metal";
}

void MetalBackend::step(Universe& universe, double dt) {
    if (!is_available()) {
        std::cerr << "Metal backend not available" << std::endl;
        return;
    }
    
    if (universe.size() == 0) {
        return; // Nothing to compute
    }
    
    if (dt <= 0.0) {
        return; // Invalid time step
    }
    
    // For RK4, forces are computed internally in the kernel
    // For other integrators, compute forces first
    if (current_integrator_ != "Runge-Kutta" && current_integrator_ != "RK4" && current_integrator_ != "Runge Kutta") {
        // Compute forces
        impl_->computeForces(universe, 1.0, 0.1, current_force_method_); // G=1.0, softening=0.1
    } else {
        // For RK4, we still need to compute initial forces (k1 stage)
        impl_->computeForces(universe, 1.0, 0.1, current_force_method_);
    }

    if (current_integrator_ == "Verlet") {
        impl_->initializeVerletHistory(universe, dt);
    }
    
    // Integrate
    impl_->integrate(universe, dt, current_integrator_);
}

std::vector<std::string> MetalBackend::get_integrators() const {
    return {"Euler", "Verlet", "Leapfrog", "Runge-Kutta"};
}

std::vector<std::string> MetalBackend::get_force_methods() const {
    std::vector<std::string> methods = {"Brute Force"};
    
    // Add Barnes-Hut if the pipeline was successfully created
    if (impl_ && impl_->barnesHutForcePipeline) {
        methods.push_back("Barnes-Hut");
    }
    
    if (impl_ && impl_->fmmForcePipeline) {
        methods.push_back("Fast Multipole");
    }
    
    return methods;
}

void MetalBackend::set_integrator(const std::string& name) {
    current_integrator_ = name;
}

void MetalBackend::set_force_method(const std::string& name) {
    current_force_method_ = name;
}

void MetalBackend::setup_integrators() {
    // Already set up in initialize()
}

} // namespace unisim

