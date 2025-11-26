#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>

#include "../simulation/parallel_utils.hpp"

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

namespace unisim {

/**
 * @brief System metrics monitor for CPU, GPU, memory, and performance tracking
 */
class MetricsMonitor {
public:
    MetricsMonitor();
    ~MetricsMonitor();

    // Update and get metrics
    void update();
    
    double get_cpu_usage() const { return cpu_usage_; }
    double get_gpu_usage() const { return gpu_usage_; }
    double get_memory_usage_mb() const { return memory_usage_mb_; }
    double get_fps() const { return fps_; }
    uint32_t get_process_thread_count() const { return process_thread_count_; }
    uint32_t get_logical_cores() const { return logical_cores_; }
    uint32_t get_physical_cores() const { return physical_cores_; }
    uint32_t get_parallel_threads_last() const { return parallel_last_threads_; }
    uint32_t get_parallel_threads_peak() const { return parallel_peak_threads_; }
    double get_parallel_threads_avg() const { return parallel_avg_threads_; }
    uint32_t get_parallel_active_jobs() const { return parallel_active_jobs_; }
    
    // FPS tracking
    void record_frame();
    
    // Simulation metrics
    void set_simulation_time(double time) { sim_time_ = time; }
    void set_step_count(uint64_t steps) { step_count_ = steps; }
    double get_simulation_time() const { return sim_time_; }
    uint64_t get_step_count() const { return step_count_; }
    
    // Performance metrics
    void record_step_time(double time_ms) {
        step_times_.push_back(time_ms);
        // Keep only last 100 measurements for rolling average
        if (step_times_.size() > 100) {
            step_times_.pop_front();
        }
    }
    
    double get_avg_step_time_ms() const {
        if (step_times_.empty()) return 0.0;
        double sum = 0.0;
        for (double t : step_times_) {
            sum += t;
        }
        return sum / step_times_.size();
    }
    
    double get_steps_per_second() const {
        double avg_ms = get_avg_step_time_ms();
        if (avg_ms <= 0.0) return 0.0;
        return 1000.0 / avg_ms;
    }
    
    double get_bodies_per_second(size_t num_bodies) const {
        return get_steps_per_second() * num_bodies;
    }
    
    // System information
    const std::string& get_cpu_name() const { return cpu_name_; }
    const std::string& get_metal_version() const { return metal_version_; }
    const std::string& get_cuda_version() const { return cuda_version_; }
    const std::string& get_gpu_name() const { return gpu_name_; }

private:
#ifdef __APPLE__
    void update_cpu_usage_macos();
    void update_memory_usage_macos();
    void update_gpu_usage_macos();
    void update_thread_usage_macos();
    void detect_system_info_macos();
    
    // macOS-specific state
    host_cpu_load_info_data_t prev_cpu_load_;
    mach_port_t host_port_;
    bool cpu_initialized_;
#endif
    
    void detect_system_info();
    void update_parallel_metrics();

    double cpu_usage_;
    double gpu_usage_;
    double memory_usage_mb_;
    double fps_;
    double sim_time_;
    uint64_t step_count_;
    uint32_t process_thread_count_;
    uint32_t logical_cores_;
    uint32_t physical_cores_;
    uint32_t parallel_last_threads_;
    uint32_t parallel_peak_threads_;
    double parallel_avg_threads_;
    uint32_t parallel_active_jobs_;

    // FPS calculation
    std::chrono::steady_clock::time_point last_frame_time_;
    std::chrono::steady_clock::time_point fps_start_time_;
    uint64_t frame_count_;
    static constexpr double FPS_UPDATE_INTERVAL = 0.5; // Update FPS every 0.5 seconds
    
    // Step timing for performance metrics
    std::deque<double> step_times_; // Rolling window of step times in milliseconds
    
    // System information (detected once)
    std::string cpu_name_;
    std::string metal_version_;
    std::string cuda_version_;
    std::string gpu_name_;
    bool system_info_detected_{false};
};

} // namespace unisim

