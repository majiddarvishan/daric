#pragma once

#include "object_pool.h"

#include <memory>
#include <mutex>

/**
 * Thread-local global message pool manager.
 *
 * Each thread transparently gets its own ObjectPool instance.
 * Access via ThreadLocalObjectPool::instance() which returns a reference
 * to the thread-local pool.
 *
 * Example:
 *   auto& pool = ThreadLocalObjectPool::instance();
 *   auto msg = pool.create<SubmitSM>("src","dst","hello");
 */
namespace daric::object_pool
{
class ThreadLocalObjectPool {
public:
    // Get the pool instance for the current thread
    static ObjectPool& instance() {
        thread_local ObjectPool pool;
        return pool;
    }

    // Prevent construction / copying
    ThreadLocalObjectPool() = delete;
    ThreadLocalObjectPool(const ThreadLocalObjectPool&) = delete;
    ThreadLocalObjectPool& operator=(const ThreadLocalObjectPool&) = delete;

    // Create API (matches ObjectPool)
    template<typename T, typename... Args>
    static std::shared_ptr<T> create(Args&&... args)
    {
        return instance().create<T>(std::forward<Args>(args)...);
        // auto sp = create<T>();
        // std::invoke(std::forward<InitFunc>(init), *sp);
        // return sp;
    }

    static void set_registry(std::shared_ptr<prometheus::Registry> registry)
    {
        internal::PrometheusMetrics::instance().set_registry(registry);
    }
};
}