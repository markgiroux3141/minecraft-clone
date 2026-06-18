#include "vox/core/ThreadPool.h"

#include "vox/core/Log.h"

namespace vox {

ThreadPool::ThreadPool(unsigned threadCount) {
    if (threadCount == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        threadCount = hw > 1 ? hw - 1 : 1;
    }
    m_workers.reserve(threadCount);
    for (unsigned i = 0; i < threadCount; ++i) {
        m_workers.emplace_back([this](std::stop_token stopToken) { WorkerMain(stopToken); });
    }
    VOX_INFO("ThreadPool started with {} workers", threadCount);
}

ThreadPool::~ThreadPool() {
    // Request all stops before any join so workers wind down in parallel
    // (jthread's destructor would otherwise stop+join them one at a time).
    for (auto& worker : m_workers) {
        worker.request_stop();
    }
    m_signal.notify_all();
}

void ThreadPool::Submit(std::move_only_function<void()> job, Priority priority) {
    {
        std::lock_guard lock(m_mutex);
        (priority == Priority::High ? m_priorityJobs : m_jobs).push_back(std::move(job));
    }
    m_signal.notify_one();
}

void ThreadPool::WorkerMain(std::stop_token stopToken) {
    while (true) {
        std::move_only_function<void()> job;
        {
            std::unique_lock lock(m_mutex);
            m_signal.wait(lock, stopToken,
                          [this] { return !m_priorityJobs.empty() || !m_jobs.empty(); });
            if (stopToken.stop_requested()) {
                return; // discard any still-queued jobs
            }
            // High lane first; a Normal job only runs when no High job waits.
            auto& lane = m_priorityJobs.empty() ? m_jobs : m_priorityJobs;
            job = std::move(lane.front());
            lane.pop_front();
        }
        job();
    }
}

} // namespace vox
