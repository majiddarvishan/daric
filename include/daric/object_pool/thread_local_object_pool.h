#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "object_pool.h"

// A convenience wrapper that gives each thread its own shared ObjectPool
// instance Note: the global/shared file-level object pool is intended to be
// created using std::make_shared<ObjectPool>() so deleters can use
// weak_from_this safely.

namespace daric::object_pool
{
class ThreadLocalObjectPool
{
  public:
    // Return a shared_ptr-managed ObjectPool that is thread-local.
    static std::shared_ptr<ObjectPool> instance()
    {
        thread_local std::shared_ptr<ObjectPool> inst =
            std::make_shared<ObjectPool>();
        return inst;
    }

    // Alternatively, provide a process-global pool (if you want single pool
    // across threads)
    static std::shared_ptr<ObjectPool> global_instance()
    {
        static std::shared_ptr<ObjectPool> g = std::make_shared<ObjectPool>();
        return g;
    }

    // Prevent construction / copying
    ThreadLocalObjectPool() = delete;
    ThreadLocalObjectPool(const ThreadLocalObjectPool&) = delete;
    ThreadLocalObjectPool& operator=(const ThreadLocalObjectPool&) = delete;

    // Create API (matches ObjectPool)
    template<typename T, typename... Args>
    static std::shared_ptr<T> create(Args&&... args)
    {
        return instance()->create<T>(std::forward<Args>(args)...);
    }

    static void set_registry(std::shared_ptr<prometheus::Registry> registry)
    {
        internal::PrometheusMetrics::instance().set_registry(registry);
    }
};
}  // namespace daric::object_pool