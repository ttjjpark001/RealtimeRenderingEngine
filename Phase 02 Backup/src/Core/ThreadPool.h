#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>
#include <type_traits>

namespace RRE
{

// Fixed-size worker thread pool.
// Submit tasks from any thread; results are returned via std::future<T>.
//
// Usage:
//   ThreadPool pool;                          // hardware_concurrency() workers
//   auto fut = pool.Submit([]{ return 42; }); // returns std::future<int>
//   int val = fut.get();
class ThreadPool
{
public:
    // threadCount == 0 → use std::thread::hardware_concurrency()
    explicit ThreadPool(size_t threadCount = 0);
    ~ThreadPool();

    // Non-copyable, non-moveable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit a callable and return a future for its result.
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using RetT = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<RetT()>>(
            [func = std::forward<F>(f),
             tup  = std::make_tuple(std::forward<Args>(args)...)]() mutable -> RetT
            {
                return std::apply(std::move(func), std::move(tup));
            });

        std::future<RetT> fut = task->get_future();

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_stop)
                throw std::runtime_error("ThreadPool is stopped");
            m_tasks.push([task]() { (*task)(); });
        }
        m_cv.notify_one();
        return fut;
    }

    size_t GetThreadCount() const { return m_workers.size(); }

private:
    void WorkerLoop();

    std::vector<std::thread>        m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex                      m_mutex;
    std::condition_variable         m_cv;
    bool                            m_stop = false;
};

} // namespace RRE
