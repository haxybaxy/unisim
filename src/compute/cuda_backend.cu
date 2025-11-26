#include "cuda_backend.hpp"

#ifdef UNISIM_USE_CUDA

#include "../simulation/body.hpp"
#include "../simulation/vector3d.hpp"
#include "../simulation/universe.hpp"

#include <cuda_runtime.h>
#include <cmath>
#include <iostream>
#include <vector>

namespace unisim {

namespace {

constexpr double kGravityConstant = 1.0;
constexpr double kSoftening = 0.1;
constexpr double kMaxForce = 1e9;
constexpr double kMaxAcceleration = 1e6;
constexpr double kMaxVelocity = 1e5;
constexpr double kMaxPosition = 1e9;

inline bool check_cuda(cudaError_t error, const char* context) {
    if (error != cudaSuccess) {
        std::cerr << "CUDA error (" << context << "): "
                  << cudaGetErrorString(error) << std::endl;
        return false;
    }
    return true;
}

__host__ __device__ inline double3 to_double3(const Vector3D& v) {
    return make_double3(v.x, v.y, v.z);
}

__host__ __device__ inline Vector3D to_vector3d(const double3& v) {
    return Vector3D(v.x, v.y, v.z);
}

__host__ __device__ inline double3 add3(const double3& a, const double3& b) {
    return make_double3(a.x + b.x, a.y + b.y, a.z + b.z);
}

__host__ __device__ inline double3 sub3(const double3& a, const double3& b) {
    return make_double3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__host__ __device__ inline double3 mul3(const double3& a, double scalar) {
    return make_double3(a.x * scalar, a.y * scalar, a.z * scalar);
}

__host__ __device__ inline double clampd(double value, double min_value, double max_value) {
    return value < min_value ? min_value : (value > max_value ? max_value : value);
}

__host__ __device__ inline double dot3(const double3& a, const double3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct CudaBody {
    double3 position;
    double3 velocity;
    double3 acceleration;
    double mass;
    double radius;
};

struct SimulationParams {
    double G;
    double softening;
    double dt;
    int num_bodies;
};

__device__ inline double3 compute_acceleration_at(
    const CudaBody* bodies,
    int self_idx,
    const double3& pos_i,
    double mass_i,
    const SimulationParams& params) {
    double3 acceleration = make_double3(0.0, 0.0, 0.0);

    if (!isfinite(mass_i) || mass_i <= 0.0) {
        return acceleration;
    }

    for (int j = 0; j < params.num_bodies; ++j) {
        if (j == self_idx) continue;

        double mass_j = bodies[j].mass;
        if (!isfinite(mass_j) || mass_j <= 0.0) continue;

        double3 pos_j = bodies[j].position;
        double3 r = sub3(pos_j, pos_i);
        double dist_sq = dot3(r, r) + params.softening * params.softening;
        if (dist_sq < 1e-12) continue;

        double dist = sqrt(dist_sq);
        if (!isfinite(dist) || dist <= 0.0) continue;

        double force_mag = params.G * mass_i * mass_j / dist_sq;
        force_mag = clampd(force_mag, -kMaxForce, kMaxForce);

        double inv_dist = 1.0 / dist;
        double3 direction = mul3(r, inv_dist);
        double3 force = mul3(direction, force_mag);
        acceleration = add3(acceleration, mul3(force, 1.0 / mass_i));
    }

    acceleration.x = clampd(acceleration.x, -kMaxAcceleration, kMaxAcceleration);
    acceleration.y = clampd(acceleration.y, -kMaxAcceleration, kMaxAcceleration);
    acceleration.z = clampd(acceleration.z, -kMaxAcceleration, kMaxAcceleration);
    return acceleration;
}

__global__ void compute_forces_bruteforce_kernel(CudaBody* bodies, SimulationParams params) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.num_bodies) return;

    double mass = bodies[idx].mass;
    if (!isfinite(mass) || mass <= 0.0) {
        bodies[idx].acceleration = make_double3(0.0, 0.0, 0.0);
        return;
    }

    double3 pos = bodies[idx].position;
    double3 accel = compute_acceleration_at(bodies, idx, pos, mass, params);
    bodies[idx].acceleration = accel;
}

__global__ void integrate_euler_kernel(CudaBody* bodies, SimulationParams params) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.num_bodies) return;

    double3 velocity = bodies[idx].velocity;
    double3 acceleration = bodies[idx].acceleration;
    velocity = add3(velocity, mul3(acceleration, params.dt));

    double3 position = bodies[idx].position;
    position = add3(position, mul3(velocity, params.dt));

    position.x = clampd(position.x, -kMaxPosition, kMaxPosition);
    position.y = clampd(position.y, -kMaxPosition, kMaxPosition);
    position.z = clampd(position.z, -kMaxPosition, kMaxPosition);

    velocity.x = clampd(velocity.x, -kMaxVelocity, kMaxVelocity);
    velocity.y = clampd(velocity.y, -kMaxVelocity, kMaxVelocity);
    velocity.z = clampd(velocity.z, -kMaxVelocity, kMaxVelocity);

    bodies[idx].velocity = velocity;
    bodies[idx].position = position;
}

__global__ void integrate_leapfrog_kernel(CudaBody* bodies, SimulationParams params) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.num_bodies) return;

    double3 velocity = bodies[idx].velocity;
    double3 acceleration = bodies[idx].acceleration;
    velocity = add3(velocity, mul3(acceleration, params.dt));

    double3 position = bodies[idx].position;
    position = add3(position, mul3(velocity, params.dt));

    position.x = clampd(position.x, -kMaxPosition, kMaxPosition);
    position.y = clampd(position.y, -kMaxPosition, kMaxPosition);
    position.z = clampd(position.z, -kMaxPosition, kMaxPosition);

    velocity.x = clampd(velocity.x, -kMaxVelocity, kMaxVelocity);
    velocity.y = clampd(velocity.y, -kMaxVelocity, kMaxVelocity);
    velocity.z = clampd(velocity.z, -kMaxVelocity, kMaxVelocity);

    bodies[idx].velocity = velocity;
    bodies[idx].position = position;
}

__global__ void integrate_verlet_kernel(
    CudaBody* bodies,
    double3* previous_positions,
    SimulationParams params) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.num_bodies) return;

    double3 current_position = bodies[idx].position;
    double3 old_position = previous_positions[idx];
    double3 acceleration = bodies[idx].acceleration;

    double dt_sq = params.dt * params.dt;
    double3 new_position = add3(
        sub3(mul3(current_position, 2.0), old_position),
        mul3(acceleration, dt_sq));

    double3 velocity = mul3(sub3(new_position, old_position), 1.0 / (2.0 * params.dt));

    new_position.x = clampd(new_position.x, -kMaxPosition, kMaxPosition);
    new_position.y = clampd(new_position.y, -kMaxPosition, kMaxPosition);
    new_position.z = clampd(new_position.z, -kMaxPosition, kMaxPosition);

    velocity.x = clampd(velocity.x, -kMaxVelocity, kMaxVelocity);
    velocity.y = clampd(velocity.y, -kMaxVelocity, kMaxVelocity);
    velocity.z = clampd(velocity.z, -kMaxVelocity, kMaxVelocity);

    previous_positions[idx] = current_position;
    bodies[idx].position = new_position;
    bodies[idx].velocity = velocity;
}

__global__ void integrate_runge_kutta_kernel(CudaBody* bodies, SimulationParams params) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.num_bodies) return;

    double3 position = bodies[idx].position;
    double3 velocity = bodies[idx].velocity;

    if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z)) {
        return;
    }

    if (!isfinite(velocity.x) || !isfinite(velocity.y) || !isfinite(velocity.z)) {
        velocity = make_double3(0.0, 0.0, 0.0);
    }

    double3 k1_vel = velocity;
    double3 k1_acc = bodies[idx].acceleration;
    if (!isfinite(k1_acc.x) || !isfinite(k1_acc.y) || !isfinite(k1_acc.z)) {
        k1_acc = make_double3(0.0, 0.0, 0.0);
    }

    double half_dt = params.dt * 0.5;

    double3 pos_k2 = add3(position, mul3(k1_vel, half_dt));
    double3 vel_k2 = add3(velocity, mul3(k1_acc, half_dt));
    double3 acc_k2 = compute_acceleration_at(bodies, idx, pos_k2, bodies[idx].mass, params);

    double3 pos_k3 = add3(position, mul3(vel_k2, half_dt));
    double3 vel_k3 = add3(velocity, mul3(acc_k2, half_dt));
    double3 acc_k3 = compute_acceleration_at(bodies, idx, pos_k3, bodies[idx].mass, params);

    double3 pos_k4 = add3(position, mul3(vel_k3, params.dt));
    double3 vel_k4 = add3(velocity, mul3(acc_k3, params.dt));
    double3 acc_k4 = compute_acceleration_at(bodies, idx, pos_k4, bodies[idx].mass, params);

    double inv_six = 1.0 / 6.0;
    double3 delta_pos = mul3(
        add3(
            add3(k1_vel, mul3(add3(vel_k2, vel_k3), 2.0)),
            vel_k4),
        params.dt * inv_six);

    double3 delta_vel = mul3(
        add3(
            add3(k1_acc, mul3(add3(acc_k2, acc_k3), 2.0)),
            acc_k4),
        params.dt * inv_six);

    position = add3(position, delta_pos);
    velocity = add3(velocity, delta_vel);

    position.x = clampd(position.x, -kMaxPosition, kMaxPosition);
    position.y = clampd(position.y, -kMaxPosition, kMaxPosition);
    position.z = clampd(position.z, -kMaxPosition, kMaxPosition);

    velocity.x = clampd(velocity.x, -kMaxVelocity, kMaxVelocity);
    velocity.y = clampd(velocity.y, -kMaxVelocity, kMaxVelocity);
    velocity.z = clampd(velocity.z, -kMaxVelocity, kMaxVelocity);

    bodies[idx].position = position;
    bodies[idx].velocity = velocity;
}

} // namespace

class CudaBackend::Impl {
public:
    Impl();
    ~Impl();

    bool initialize();
    void shutdown();

    bool is_available() const { return available_; }

    bool sync_to_device(const Universe& universe, double dt);
    bool compute_forces(double G, double softening);
    bool integrate(const std::string& integrator, double dt);
    bool sync_from_device(Universe& universe);

    void set_integrator(const std::string& name);

private:
    bool ensure_capacity(std::size_t required);
    void free_buffers();
    void reset_verlet_state();

    int device_id_;
    bool available_;
    std::size_t capacity_;
    std::size_t current_count_;
    std::size_t last_synced_count_;
    CudaBody* d_bodies_;
    double3* d_prev_positions_;
    cudaStream_t stream_;
    std::vector<CudaBody> host_bodies_;
    std::vector<double3> host_prev_positions_;
    bool integrator_requires_history_;
    bool verlet_initialized_;
    double last_verlet_dt_;
};

CudaBackend::Impl::Impl()
    : device_id_(0),
      available_(false),
      capacity_(0),
      current_count_(0),
      last_synced_count_(0),
      d_bodies_(nullptr),
      d_prev_positions_(nullptr),
      stream_(nullptr),
      integrator_requires_history_(false),
      verlet_initialized_(false),
      last_verlet_dt_(0.0) {}

CudaBackend::Impl::~Impl() {
    shutdown();
}

bool CudaBackend::Impl::initialize() {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess || device_count == 0) {
        std::cerr << "CUDA backend initialization failed: "
                  << cudaGetErrorString(err) << std::endl;
        return false;
    }

    device_id_ = 0;
    if (!check_cuda(cudaSetDevice(device_id_), "cudaSetDevice")) {
        return false;
    }

    if (!check_cuda(cudaStreamCreate(&stream_), "cudaStreamCreate")) {
        return false;
    }

    available_ = true;
    return true;
}

void CudaBackend::Impl::shutdown() {
    free_buffers();
    if (stream_) {
        cudaStreamDestroy(stream_);
        stream_ = nullptr;
    }
    available_ = false;
    current_count_ = 0;
    last_synced_count_ = 0;
    host_bodies_.clear();
    host_prev_positions_.clear();
}

bool CudaBackend::Impl::ensure_capacity(std::size_t required) {
    if (required == 0) {
        return true;
    }
    if (required <= capacity_) {
        return true;
    }

    free_buffers();

    capacity_ = required;
    if (!check_cuda(cudaMalloc(&d_bodies_, capacity_ * sizeof(CudaBody)), "cudaMalloc bodies")) {
        free_buffers();
        return false;
    }
    if (!check_cuda(cudaMalloc(&d_prev_positions_, capacity_ * sizeof(double3)), "cudaMalloc prev_positions")) {
        free_buffers();
        return false;
    }

    return true;
}

void CudaBackend::Impl::free_buffers() {
    if (d_bodies_) {
        cudaFree(d_bodies_);
        d_bodies_ = nullptr;
    }
    if (d_prev_positions_) {
        cudaFree(d_prev_positions_);
        d_prev_positions_ = nullptr;
    }
    capacity_ = 0;
}

void CudaBackend::Impl::reset_verlet_state() {
    verlet_initialized_ = false;
    last_verlet_dt_ = 0.0;
}

void CudaBackend::Impl::set_integrator(const std::string& name) {
    bool needs_history = (name == "Verlet");
    if (needs_history != integrator_requires_history_) {
        reset_verlet_state();
    }
    integrator_requires_history_ = needs_history;
}

bool CudaBackend::Impl::sync_to_device(const Universe& universe, double dt) {
    current_count_ = universe.size();
    if (current_count_ == 0) {
        return true;
    }

    if (!ensure_capacity(current_count_)) {
        return false;
    }

    host_bodies_.resize(current_count_);
    for (std::size_t i = 0; i < current_count_; ++i) {
        const Body& body = universe[i];
        CudaBody gpu_body;
        gpu_body.position = to_double3(body.position);
        gpu_body.velocity = to_double3(body.velocity);
        gpu_body.acceleration = to_double3(body.acceleration);
        gpu_body.mass = body.mass;
        gpu_body.radius = body.radius;
        host_bodies_[i] = gpu_body;
    }

    if (!check_cuda(
            cudaMemcpyAsync(d_bodies_, host_bodies_.data(),
                            current_count_ * sizeof(CudaBody),
                            cudaMemcpyHostToDevice, stream_),
            "cudaMemcpyAsync bodies upload")) {
        return false;
    }

    if (!check_cuda(cudaStreamSynchronize(stream_), "sync bodies upload")) {
        return false;
    }

    if (integrator_requires_history_) {
        if (current_count_ != last_synced_count_) {
            reset_verlet_state();
        }

        if (!verlet_initialized_ || std::abs(dt - last_verlet_dt_) > 1e-12) {
            host_prev_positions_.resize(current_count_);
            for (std::size_t i = 0; i < current_count_; ++i) {
                const auto& gpu_body = host_bodies_[i];
                host_prev_positions_[i] = make_double3(
                    gpu_body.position.x - gpu_body.velocity.x * dt,
                    gpu_body.position.y - gpu_body.velocity.y * dt,
                    gpu_body.position.z - gpu_body.velocity.z * dt);
            }

            if (!check_cuda(
                    cudaMemcpyAsync(d_prev_positions_, host_prev_positions_.data(),
                                    current_count_ * sizeof(double3),
                                    cudaMemcpyHostToDevice, stream_),
                    "cudaMemcpyAsync prev positions upload")) {
                return false;
            }

            if (!check_cuda(cudaStreamSynchronize(stream_), "sync prev positions upload")) {
                return false;
            }

            last_verlet_dt_ = dt;
            verlet_initialized_ = true;
        }
    } else {
        reset_verlet_state();
    }

    last_synced_count_ = current_count_;
    return true;
}

bool CudaBackend::Impl::compute_forces(double G, double softening) {
    if (current_count_ == 0) {
        return true;
    }

    SimulationParams params{G, softening, 0.0, static_cast<int>(current_count_)};
    int threads = 256;
    int blocks = static_cast<int>((current_count_ + threads - 1) / threads);

    compute_forces_bruteforce_kernel<<<blocks, threads, 0, stream_>>>(d_bodies_, params);
    if (!check_cuda(cudaGetLastError(), "compute_forces kernel launch")) {
        return false;
    }
    if (!check_cuda(cudaStreamSynchronize(stream_), "compute_forces sync")) {
        return false;
    }
    return true;
}

bool CudaBackend::Impl::integrate(const std::string& integrator, double dt) {
    if (current_count_ == 0) {
        return true;
    }

    SimulationParams params{kGravityConstant, kSoftening, dt, static_cast<int>(current_count_)};
    int threads = 256;
    int blocks = static_cast<int>((current_count_ + threads - 1) / threads);

    auto launch_and_sync = [&](auto kernel_call) -> bool {
        kernel_call();
        if (!check_cuda(cudaGetLastError(), "integration kernel launch")) {
            return false;
        }
        if (!check_cuda(cudaStreamSynchronize(stream_), "integration sync")) {
            return false;
        }
        return true;
    };

    if (integrator == "Verlet") {
        return launch_and_sync([&]() {
            integrate_verlet_kernel<<<blocks, threads, 0, stream_>>>(
                d_bodies_, d_prev_positions_, params);
        });
    } else if (integrator == "Leapfrog") {
        return launch_and_sync([&]() {
            integrate_leapfrog_kernel<<<blocks, threads, 0, stream_>>>(
                d_bodies_, params);
        });
    } else if (integrator == "Runge-Kutta" ||
               integrator == "Runge Kutta" ||
               integrator == "RK4") {
        return launch_and_sync([&]() {
            integrate_runge_kutta_kernel<<<blocks, threads, 0, stream_>>>(
                d_bodies_, params);
        });
    } else {
        return launch_and_sync([&]() {
            integrate_euler_kernel<<<blocks, threads, 0, stream_>>>(
                d_bodies_, params);
        });
    }
}

bool CudaBackend::Impl::sync_from_device(Universe& universe) {
    if (current_count_ == 0) {
        return true;
    }

    if (!check_cuda(
            cudaMemcpyAsync(host_bodies_.data(), d_bodies_,
                            current_count_ * sizeof(CudaBody),
                            cudaMemcpyDeviceToHost, stream_),
            "cudaMemcpyAsync bodies download")) {
        return false;
    }

    if (!check_cuda(cudaStreamSynchronize(stream_), "sync bodies download")) {
        return false;
    }

    for (std::size_t i = 0; i < current_count_; ++i) {
        Body& body = universe[i];
        body.position = to_vector3d(host_bodies_[i].position);
        body.velocity = to_vector3d(host_bodies_[i].velocity);
        body.acceleration = to_vector3d(host_bodies_[i].acceleration);
    }

    return true;
}

CudaBackend::CudaBackend()
    : impl_(std::make_unique<Impl>()),
      current_integrator_("Euler"),
      current_force_method_("Brute Force") {}

CudaBackend::~CudaBackend() {
    shutdown();
}

bool CudaBackend::initialize() {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    bool ok = impl_->initialize();
    if (ok) {
        impl_->set_integrator(current_integrator_);
    }
    return ok;
}

void CudaBackend::shutdown() {
    if (impl_) {
        impl_->shutdown();
    }
}

bool CudaBackend::is_available() const {
    return impl_ && impl_->is_available();
}

const char* CudaBackend::name() const {
    return "CUDA";
}

void CudaBackend::step(Universe& universe, double dt) {
    if (!impl_ || !impl_->is_available()) {
        std::cerr << "CUDA backend is not available on this system." << std::endl;
        return;
    }

    if (universe.size() == 0 || dt <= 0.0) {
        return;
    }

    if (!impl_->sync_to_device(universe, dt)) {
        std::cerr << "Failed to transfer simulation data to CUDA device." << std::endl;
        return;
    }

    if (current_force_method_ != "Brute Force") {
        std::cerr << "CUDA backend currently supports only the \"Brute Force\" force method." << std::endl;
    }

    if (!impl_->compute_forces(kGravityConstant, kSoftening)) {
        std::cerr << "CUDA force computation failed." << std::endl;
        return;
    }

    if (!impl_->integrate(current_integrator_, dt)) {
        std::cerr << "CUDA integration failed for integrator " << current_integrator_ << "." << std::endl;
        return;
    }

    if (!impl_->sync_from_device(universe)) {
        std::cerr << "Failed to transfer results back from CUDA device." << std::endl;
    }
}

std::vector<std::string> CudaBackend::get_integrators() const {
    return {"Euler", "Verlet", "Leapfrog", "Runge-Kutta"};
}

std::vector<std::string> CudaBackend::get_force_methods() const {
    return {"Brute Force"};
}

void CudaBackend::set_integrator(const std::string& name) {
    current_integrator_ = name;
    if (impl_) {
        impl_->set_integrator(name);
    }
}

void CudaBackend::set_force_method(const std::string& name) {
    if (name != "Brute Force") {
        std::cerr << "CUDA backend only supports the \"Brute Force\" force method. "
                     "Requested method \"" << name << "\" will be ignored."
                  << std::endl;
        current_force_method_ = "Brute Force";
        return;
    }
    current_force_method_ = name;
}

} // namespace unisim

#endif // UNISIM_USE_CUDA


