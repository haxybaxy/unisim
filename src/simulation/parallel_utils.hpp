#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

namespace unisim {

struct ParallelRuntimeStats {
    std::atomic<std::uint64_t> total_jobs{0};
    std::atomic<std::uint64_t> total_threads_used{0};
    std::atomic<std::uint32_t> last_thread_count{1};
    std::atomic<std::uint32_t> peak_thread_count{1};
    std::atomic<std::uint32_t> active_jobs{0};
};

inline ParallelRuntimeStats& parallel_runtime_stats() {
    static ParallelRuntimeStats stats;
    return stats;
}

struct ParallelMetricsSnapshot {
    std::uint32_t last_threads{1};
    std::uint32_t peak_threads{1};
    double average_threads{1.0};
    std::uint32_t active_jobs{0};
};

inline ParallelMetricsSnapshot get_parallel_metrics_snapshot() {
    auto& stats = parallel_runtime_stats();
    const auto jobs = stats.total_jobs.load(std::memory_order_relaxed);
    const auto threads = stats.total_threads_used.load(std::memory_order_relaxed);
    ParallelMetricsSnapshot snapshot;
    snapshot.last_threads = stats.last_thread_count.load(std::memory_order_relaxed);
    snapshot.peak_threads = stats.peak_thread_count.load(std::memory_order_relaxed);
    snapshot.active_jobs = stats.active_jobs.load(std::memory_order_relaxed);
    snapshot.average_threads = (jobs > 0)
        ? static_cast<double>(threads) / static_cast<double>(jobs)
        : 0.0;
    return snapshot;
}

/**
 * @brief Determine how many worker threads to use for a given amount of work.
 *
 * @param total_work Number of iterations/items to process.
 * @param min_chunk Minimum iterations per worker before spawning more threads.
 */
inline std::size_t determine_thread_count(std::size_t total_work, std::size_t min_chunk = 1024) {
    if (total_work == 0) {
        return 1;
    }

    const std::size_t hw = std::max<unsigned>(1u, std::thread::hardware_concurrency());
    if (total_work <= min_chunk) {
        return 1;
    }

    if (min_chunk == 0) {
        return hw;
    }

    const std::size_t required = (total_work + min_chunk - 1) / min_chunk;
    return std::max<std::size_t>(1, std::min(hw, required));
}

/**
 * @brief Run a function over [begin, end) using a fixed number of threads.
 *
 * The provided functor must have the signature
 * `void func(std::size_t chunk_begin, std::size_t chunk_end, std::size_t worker_index,
 *            std::size_t worker_count)`.
 */
template <typename Func>
void parallel_for_range(std::size_t begin,
                        std::size_t end,
                        std::size_t num_threads,
                        Func&& func) {
    if (end <= begin) {
        return;
    }

    num_threads = std::max<std::size_t>(1, num_threads);
    const std::size_t total = end - begin;

    using Functor = std::decay_t<Func>;
    Functor task(std::forward<Func>(func));

    auto& stats = parallel_runtime_stats();
    struct StatsJobGuard {
        ParallelRuntimeStats& stats_ref;
        explicit StatsJobGuard(ParallelRuntimeStats& s) : stats_ref(s) {
            stats_ref.active_jobs.fetch_add(1, std::memory_order_relaxed);
        }
        ~StatsJobGuard() {
            stats_ref.active_jobs.fetch_sub(1, std::memory_order_relaxed);
        }
    } guard(stats);

    const auto threads_used = static_cast<std::uint32_t>(num_threads);
    stats.last_thread_count.store(threads_used, std::memory_order_relaxed);
    stats.total_jobs.fetch_add(1, std::memory_order_relaxed);
    stats.total_threads_used.fetch_add(threads_used, std::memory_order_relaxed);
    auto peak = stats.peak_thread_count.load(std::memory_order_relaxed);
    while (threads_used > peak &&
           !stats.peak_thread_count.compare_exchange_weak(
               peak, threads_used,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
        // retry until updated
    }

    if (num_threads == 1) {
        task(begin, end, 0, 1);
        return;
    }

    const std::size_t base = total / num_threads;
    const std::size_t remainder = total % num_threads;

    std::vector<std::thread> workers(num_threads - 1);
    std::size_t chunk_begin = begin;

    for (std::size_t i = 0; i < num_threads - 1; ++i) {
        const std::size_t extra = (i < remainder) ? 1 : 0;
        const std::size_t chunk_end = chunk_begin + base + extra;
        workers[i] = std::thread([chunk_begin, chunk_end, i, num_threads, &task]() {
            task(chunk_begin, chunk_end, i, num_threads);
        });
        chunk_begin = chunk_end;
    }

    task(chunk_begin, end, num_threads - 1, num_threads);

    for (auto& worker : workers) {
        worker.join();
    }
}

/**
 * @brief Run a function over [begin, end) with an automatically determined number of threads.
 */
template <typename Func>
void parallel_for_range(std::size_t begin,
                        std::size_t end,
                        Func&& func,
                        std::size_t min_chunk = 1024) {
    const std::size_t total = (end > begin) ? (end - begin) : 0;
    const std::size_t threads = determine_thread_count(total, min_chunk);
    parallel_for_range(begin, end, threads, std::forward<Func>(func));
}

} // namespace unisim


