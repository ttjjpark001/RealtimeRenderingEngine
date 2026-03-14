#include "Core/ThreadPool.h"
#include <stdexcept>

namespace RRE
{

ThreadPool::ThreadPool(size_t threadCount)
{
    if (threadCount == 0)
        threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0)
        threadCount = 2; // fallback if hardware_concurrency returns 0

    m_workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i)
        m_workers.emplace_back([this] { WorkerLoop(); });
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    for (auto& t : m_workers)
        if (t.joinable()) t.join();
}

void ThreadPool::WorkerLoop()
{
    for (;;)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
            if (m_stop && m_tasks.empty())
                return;
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        task();
    }
}

} // namespace RRE
