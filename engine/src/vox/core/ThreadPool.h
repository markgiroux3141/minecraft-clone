#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace vox {

// Fixed-size worker pool running submitted jobs FIFO. Completion signalling
// is the caller's concern (e.g. push results to a queue the caller drains).
// Destruction stops workers after their current job; queued jobs that never
// started are discarded, so jobs must not be required for correctness.
//
// Two lanes: High-priority jobs are pulled before any Normal job, so a
// latency-sensitive task (e.g. a player-edit remesh) jumps the streaming
// backlog instead of waiting behind it FIFO.
class ThreadPool {
public:
    enum class Priority { Normal, High };

    // threadCount == 0 picks hardware_concurrency - 1 (leaving the main
    // thread a core), minimum 1.
    explicit ThreadPool(unsigned threadCount = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void Submit(std::move_only_function<void()> job, Priority priority = Priority::Normal);

    unsigned ThreadCount() const { return static_cast<unsigned>(m_workers.size()); }

private:
    void WorkerMain(std::stop_token stopToken);

    std::mutex m_mutex;
    std::condition_variable_any m_signal;
    std::deque<std::move_only_function<void()>> m_jobs;         // Normal lane
    std::deque<std::move_only_function<void()>> m_priorityJobs; // High lane, drained first
    std::vector<std::jthread> m_workers;
};

} // namespace vox
