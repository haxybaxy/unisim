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

// Force computation kernel
kernel void compute_forces(
    device Body* bodies [[buffer(0)]],
    constant SimulationParams& params [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    if (id >= params.num_bodies) return;
    
    // Skip if body has invalid mass
    if (bodies[id].mass <= 0.0 || !isfinite(bodies[id].mass)) {
        bodies[id].acceleration = float3(0.0, 0.0, 0.0);
        return;
    }
    
    float3 acceleration = float3(0.0, 0.0, 0.0);
    
    for (uint j = 0; j < params.num_bodies; ++j) {
        if (j == id) continue;
        
        // Skip if other body has invalid mass or position
        if (bodies[j].mass <= 0.0 || !isfinite(bodies[j].mass)) continue;
        if (!isfinite(bodies[j].position.x) || !isfinite(bodies[j].position.y) || !isfinite(bodies[j].position.z)) continue;
        
        float3 r = bodies[j].position - bodies[id].position;
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

