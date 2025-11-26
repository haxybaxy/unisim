#include "metrics_monitor.hpp"
#include <thread>
#include <cmath>
#include <cstring>
#include <sstream>
#include <cstdio>

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

namespace unisim {

MetricsMonitor::MetricsMonitor()
    : cpu_usage_(0.0)
    , gpu_usage_(0.0)
    , memory_usage_mb_(0.0)
    , fps_(0.0)
    , sim_time_(0.0)
    , step_count_(0)
    , last_frame_time_(std::chrono::steady_clock::now())
    , fps_start_time_(std::chrono::steady_clock::now())
    , frame_count_(0)
    , system_info_detected_(false)
#ifdef __APPLE__
    , host_port_(mach_host_self())
    , cpu_initialized_(false)
#endif
{
#ifdef __APPLE__
    // Initialize CPU load tracking
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    host_statistics(host_port_, HOST_CPU_LOAD_INFO, 
                    (host_info_t)&prev_cpu_load_, &count);
    cpu_initialized_ = true;
#endif
    
    // Detect system information once
    detect_system_info();
}

MetricsMonitor::~MetricsMonitor() {
#ifdef __APPLE__
    if (host_port_ != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), host_port_);
    }
#endif
}

void MetricsMonitor::update() {
#ifdef __APPLE__
    update_cpu_usage_macos();
    update_memory_usage_macos();
    update_gpu_usage_macos();
#else
    // Fallback for non-macOS systems
    cpu_usage_ = 0.0;
    gpu_usage_ = 0.0;
    memory_usage_mb_ = 0.0;
#endif
}

#ifdef __APPLE__
void MetricsMonitor::update_cpu_usage_macos() {
    if (!cpu_initialized_) return;
    
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    host_cpu_load_info_data_t cpu_load;
    
    kern_return_t result = host_statistics(host_port_, HOST_CPU_LOAD_INFO,
                                          (host_info_t)&cpu_load, &count);
    if (result != KERN_SUCCESS) {
        cpu_usage_ = 0.0;
        return;
    }
    
    // Calculate CPU usage percentage
    uint64_t total_prev = prev_cpu_load_.cpu_ticks[CPU_STATE_USER] +
                          prev_cpu_load_.cpu_ticks[CPU_STATE_SYSTEM] +
                          prev_cpu_load_.cpu_ticks[CPU_STATE_NICE] +
                          prev_cpu_load_.cpu_ticks[CPU_STATE_IDLE];
    
    uint64_t total_curr = cpu_load.cpu_ticks[CPU_STATE_USER] +
                          cpu_load.cpu_ticks[CPU_STATE_SYSTEM] +
                          cpu_load.cpu_ticks[CPU_STATE_NICE] +
                          cpu_load.cpu_ticks[CPU_STATE_IDLE];
    
    uint64_t used_prev = prev_cpu_load_.cpu_ticks[CPU_STATE_USER] +
                         prev_cpu_load_.cpu_ticks[CPU_STATE_SYSTEM] +
                         prev_cpu_load_.cpu_ticks[CPU_STATE_NICE];
    
    uint64_t used_curr = cpu_load.cpu_ticks[CPU_STATE_USER] +
                         cpu_load.cpu_ticks[CPU_STATE_SYSTEM] +
                         cpu_load.cpu_ticks[CPU_STATE_NICE];
    
    uint64_t total_diff = total_curr - total_prev;
    uint64_t used_diff = used_curr - used_prev;
    
    if (total_diff > 0) {
        cpu_usage_ = (100.0 * used_diff) / total_diff;
    } else {
        cpu_usage_ = 0.0;
    }
    
    // Clamp to 0-100
    if (cpu_usage_ > 100.0) cpu_usage_ = 100.0;
    if (cpu_usage_ < 0.0) cpu_usage_ = 0.0;
    
    prev_cpu_load_ = cpu_load;
}

void MetricsMonitor::update_memory_usage_macos() {
    struct task_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    kern_return_t result = task_info(mach_task_self(), TASK_BASIC_INFO,
                                     (task_info_t)&info, &size);
    
    if (result == KERN_SUCCESS) {
        memory_usage_mb_ = info.resident_size / (1024.0 * 1024.0);
    } else {
        memory_usage_mb_ = 0.0;
    }
}

void MetricsMonitor::update_gpu_usage_macos() {
    // macOS doesn't provide easy GPU usage monitoring without IOKit
    // For now, we'll use a heuristic based on whether Metal compute is active
    // This is a placeholder - in a real implementation, you'd use IOKit
    // or Metal performance counters
    
    // Simple heuristic: if we're using GPU compute, estimate based on CPU load
    // This is not accurate but provides some indication
    gpu_usage_ = cpu_usage_ * 0.7; // Rough estimate
    
    // Clamp to 0-100
    if (gpu_usage_ > 100.0) gpu_usage_ = 100.0;
    if (gpu_usage_ < 0.0) gpu_usage_ = 0.0;
}
#endif

void MetricsMonitor::record_frame() {
    auto now = std::chrono::steady_clock::now();
    frame_count_++;
    
    auto elapsed = std::chrono::duration<double>(now - fps_start_time_).count();
    
    if (elapsed >= FPS_UPDATE_INTERVAL) {
        fps_ = frame_count_ / elapsed;
        frame_count_ = 0;
        fps_start_time_ = now;
    }
    
    last_frame_time_ = now;
}

void MetricsMonitor::detect_system_info() {
    if (system_info_detected_) return;
    
#ifdef __APPLE__
    detect_system_info_macos();
#else
    // Fallback for non-macOS
    cpu_name_ = "Unknown CPU";
    metal_version_ = "N/A";
    cuda_version_ = "N/A";
    gpu_name_ = "Unknown GPU";
#endif
    
    system_info_detected_ = true;
}

#ifdef __APPLE__
void MetricsMonitor::detect_system_info_macos() {
    // Detect CPU name
    size_t size = 0;
    sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0);
    if (size > 0) {
        char* cpu_brand = new char[size];
        if (sysctlbyname("machdep.cpu.brand_string", cpu_brand, &size, nullptr, 0) == 0) {
            cpu_name_ = std::string(cpu_brand);
        } else {
            cpu_name_ = "Unknown CPU";
        }
        delete[] cpu_brand;
    } else {
        cpu_name_ = "Unknown CPU";
    }
    
    // Detect Metal version and GPU using system_profiler
    // This avoids needing Objective-C++ in this file
    FILE* gpu_check = popen("system_profiler SPDisplaysDataType 2>/dev/null | grep -A 5 'Chipset Model' | head -1 | sed 's/.*Chipset Model: //'", "r");
    if (gpu_check) {
        char gpu_buffer[256];
        if (fgets(gpu_buffer, sizeof(gpu_buffer), gpu_check) != nullptr) {
            // Remove trailing newline
            size_t len = strlen(gpu_buffer);
            if (len > 0 && gpu_buffer[len-1] == '\n') {
                gpu_buffer[len-1] = '\0';
            }
            if (strlen(gpu_buffer) > 0) {
                gpu_name_ = std::string(gpu_buffer);
            } else {
                gpu_name_ = "Apple GPU";
            }
        } else {
            gpu_name_ = "Apple GPU";
        }
        pclose(gpu_check);
    } else {
        gpu_name_ = "Apple GPU";
    }
    
    // Detect macOS version to infer Metal version
    FILE* os_check = popen("sw_vers -productVersion 2>/dev/null", "r");
    if (os_check) {
        char os_buffer[64];
        if (fgets(os_buffer, sizeof(os_buffer), os_check) != nullptr) {
            float os_version = 0.0f;
            if (sscanf(os_buffer, "%f", &os_version) == 1) {
                if (os_version >= 13.0f) {
                    metal_version_ = "Metal 3";
                } else if (os_version >= 10.15f) {
                    metal_version_ = "Metal 2.2+";
                } else if (os_version >= 10.13f) {
                    metal_version_ = "Metal 2";
                } else {
                    metal_version_ = "Metal 1.x";
                }
            } else {
                metal_version_ = "Metal (Available)";
            }
        } else {
            metal_version_ = "Metal (Available)";
        }
        pclose(os_check);
    } else {
        metal_version_ = "Metal (Available)";
    }
    
    // Detect CUDA availability
    // On macOS, CUDA is typically not available (NVIDIA dropped macOS support)
    // But we can check for CUDA libraries
    cuda_version_ = "Not Available";
    
    // Try to detect CUDA by checking for common CUDA paths
    // This is a simple check - in practice you'd use dlopen or similar
    FILE* cuda_check = popen("which nvcc 2>/dev/null", "r");
    if (cuda_check) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), cuda_check) != nullptr) {
            // CUDA compiler found, try to get version
            FILE* version_check = popen("nvcc --version 2>/dev/null | grep release", "r");
            if (version_check) {
                char version_buffer[256];
                if (fgets(version_buffer, sizeof(version_buffer), version_check) != nullptr) {
                    std::string version_str(version_buffer);
                    // Extract version number
                    size_t pos = version_str.find("release");
                    if (pos != std::string::npos) {
                        size_t start = version_str.find_first_of("0123456789", pos);
                        size_t end = version_str.find_first_not_of("0123456789.", start);
                        if (start != std::string::npos) {
                            cuda_version_ = "CUDA " + version_str.substr(start, end - start);
                        }
                    }
                }
                pclose(version_check);
            }
        }
        pclose(cuda_check);
    }
}
#endif

} // namespace unisim

