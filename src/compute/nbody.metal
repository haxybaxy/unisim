#include <metal_stdlib>
using namespace metal;

struct Body {
    float3 position;
    float3 velocity;
    float3 acceleration;
    float mass;
    float radius;
};

struct SimulationParams {
    float G;           // Gravitational constant
    float softening;   // Softening parameter
    float dt;          // Time step
    uint32_t num_bodies;
};

constexpr ushort kMaxTileSize = 256;

// Force computation kernel
kernel void compute_forces(
    device Body* bodies [[buffer(0)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]],
    uint local_id [[thread_index_in_threadgroup]],
    uint tg_size [[threads_per_threadgroup]]
) {
    if (id >= params.num_bodies) return;
    
    tg_size = min((uint)kMaxTileSize, max<uint>(1, tg_size));
    
    // Skip if body has invalid mass
    Body self = bodies[id];
    if (self.mass <= 0.0 || !isfinite(self.mass)) {
        bodies[id].acceleration = float3(0.0, 0.0, 0.0);
        return;
    }
    
    threadgroup Body sharedBodies[kMaxTileSize];
    
    float3 acceleration = float3(0.0, 0.0, 0.0);
    const float3 pos_i = self.position;
    const float mass_i = self.mass;
    
    for (uint tile_begin = 0; tile_begin < params.num_bodies; tile_begin += tg_size) {
        uint global_index = tile_begin + local_id;
        if (global_index < params.num_bodies) {
            sharedBodies[local_id] = bodies[global_index];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        
        uint tile_count = min(tg_size, params.num_bodies - tile_begin);
        for (uint j = 0; j < tile_count; ++j) {
            uint other_idx = tile_begin + j;
            if (other_idx == id) {
                continue;
            }
            
            const Body other = sharedBodies[j];
            
            if (other.mass <= 0.0 || !isfinite(other.mass)) {
                continue;
            }
            
            if (!isfinite(other.position.x) || !isfinite(other.position.y) || !isfinite(other.position.z)) {
                continue;
            }
            
            float3 r = other.position - pos_i;
            float dist_sq = dot(r, r) + params.softening * params.softening;
            
            if (dist_sq < 1e-10) {
                continue;
            }
            
            float dist = sqrt(dist_sq);
            
            float force_mag = params.G * mass_i * other.mass / dist_sq;
            
            const float max_force = 1e6;
            force_mag = clamp(force_mag, -max_force, max_force);
            
            float3 force_dir = r / dist;
            float3 force = force_dir * force_mag;
            
            acceleration += force / mass_i;
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    // Clamp acceleration to prevent extreme values
    float max_accel = 1e4;
    acceleration.x = clamp(acceleration.x, -max_accel, max_accel);
    acceleration.y = clamp(acceleration.y, -max_accel, max_accel);
    acceleration.z = clamp(acceleration.z, -max_accel, max_accel);
    
    bodies[id].acceleration = acceleration;
}

// Euler integrator kernel
kernel void integrate_euler(
    device Body* bodies [[buffer(0)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    // Validate inputs
    if (!isfinite(bodies[id].position.x) || !isfinite(bodies[id].position.y) || !isfinite(bodies[id].position.z)) {
        return; // Skip invalid bodies
    }
    
    if (!isfinite(bodies[id].velocity.x) || !isfinite(bodies[id].velocity.y) || !isfinite(bodies[id].velocity.z)) {
        bodies[id].velocity = float3(0.0, 0.0, 0.0);
    }
    
    if (!isfinite(bodies[id].acceleration.x) || !isfinite(bodies[id].acceleration.y) || !isfinite(bodies[id].acceleration.z)) {
        bodies[id].acceleration = float3(0.0, 0.0, 0.0);
    }
    
    bodies[id].velocity += bodies[id].acceleration * params.dt;
    bodies[id].position += bodies[id].velocity * params.dt;
    
    // Clamp positions to prevent extreme values
    float max_pos = 1e6;
    bodies[id].position.x = clamp(bodies[id].position.x, -max_pos, max_pos);
    bodies[id].position.y = clamp(bodies[id].position.y, -max_pos, max_pos);
    bodies[id].position.z = clamp(bodies[id].position.z, -max_pos, max_pos);
    
    // Clamp velocities
    float max_vel = 1e4;
    bodies[id].velocity.x = clamp(bodies[id].velocity.x, -max_vel, max_vel);
    bodies[id].velocity.y = clamp(bodies[id].velocity.y, -max_vel, max_vel);
    bodies[id].velocity.z = clamp(bodies[id].velocity.z, -max_vel, max_vel);
}

// Verlet integrator kernel (needs previous positions)
kernel void integrate_verlet(
    device Body* bodies [[buffer(0)]],
    device packed_float3* prev_positions [[buffer(2)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    // Validate inputs
    if (!isfinite(bodies[id].position.x) || !isfinite(bodies[id].position.y) || !isfinite(bodies[id].position.z)) {
        return;
    }
    
    if (!isfinite(prev_positions[id].x) || !isfinite(prev_positions[id].y) || !isfinite(prev_positions[id].z)) {
        prev_positions[id] = bodies[id].position;
    }
    
    if (!isfinite(bodies[id].acceleration.x) || !isfinite(bodies[id].acceleration.y) || !isfinite(bodies[id].acceleration.z)) {
        bodies[id].acceleration = float3(0.0, 0.0, 0.0);
    }
    
    float3 temp = bodies[id].position;
    bodies[id].position = 2.0 * bodies[id].position - prev_positions[id] + 
                         bodies[id].acceleration * params.dt * params.dt;
    
    // Prevent division by zero
    if (abs(params.dt) > 1e-10) {
        bodies[id].velocity = (bodies[id].position - prev_positions[id]) / (2.0 * params.dt);
    } else {
        bodies[id].velocity = float3(0.0, 0.0, 0.0);
    }
    
    prev_positions[id] = temp;
    
    // Clamp values
    float max_pos = 1e6;
    bodies[id].position.x = clamp(bodies[id].position.x, -max_pos, max_pos);
    bodies[id].position.y = clamp(bodies[id].position.y, -max_pos, max_pos);
    bodies[id].position.z = clamp(bodies[id].position.z, -max_pos, max_pos);
    
    float max_vel = 1e4;
    bodies[id].velocity.x = clamp(bodies[id].velocity.x, -max_vel, max_vel);
    bodies[id].velocity.y = clamp(bodies[id].velocity.y, -max_vel, max_vel);
    bodies[id].velocity.z = clamp(bodies[id].velocity.z, -max_vel, max_vel);
}

// Leapfrog integrator kernel
kernel void integrate_leapfrog(
    device Body* bodies [[buffer(0)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    // Validate inputs
    if (!isfinite(bodies[id].position.x) || !isfinite(bodies[id].position.y) || !isfinite(bodies[id].position.z)) {
        return;
    }
    
    if (!isfinite(bodies[id].velocity.x) || !isfinite(bodies[id].velocity.y) || !isfinite(bodies[id].velocity.z)) {
        bodies[id].velocity = float3(0.0, 0.0, 0.0);
    }
    
    if (!isfinite(bodies[id].acceleration.x) || !isfinite(bodies[id].acceleration.y) || !isfinite(bodies[id].acceleration.z)) {
        bodies[id].acceleration = float3(0.0, 0.0, 0.0);
    }
    
    bodies[id].velocity += bodies[id].acceleration * params.dt;
    bodies[id].position += bodies[id].velocity * params.dt;
    
    // Clamp values
    float max_pos = 1e6;
    bodies[id].position.x = clamp(bodies[id].position.x, -max_pos, max_pos);
    bodies[id].position.y = clamp(bodies[id].position.y, -max_pos, max_pos);
    bodies[id].position.z = clamp(bodies[id].position.z, -max_pos, max_pos);
    
    float max_vel = 1e4;
    bodies[id].velocity.x = clamp(bodies[id].velocity.x, -max_vel, max_vel);
    bodies[id].velocity.y = clamp(bodies[id].velocity.y, -max_vel, max_vel);
    bodies[id].velocity.z = clamp(bodies[id].velocity.z, -max_vel, max_vel);
}

