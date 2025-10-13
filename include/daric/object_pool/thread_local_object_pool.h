#pragma once

#include "object_pool.h"
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <vector>
#include <thread>
#include <functional>

namespace daric::object_pool
{
/*
 ThreadLocalObjectPool with automatic reclaimer thread.

 Design:
  - Per-thread data (PerThreadData) holds:
      - a heap-allocated ObjectPool instance (std::unique_ptr<ObjectPool>)
      - a thread-safe pending returns queue (vector + mutex)
      - thread id
  - A registry (map) stores shared_ptr<PerThreadData> for all threads.
  - When a thread first uses the wrapper, it registers its PerThreadData.
  - create<T> uses the local PerThreadData->pool.create_raw and then returns
    a std::shared_ptr<T> whose deleter captures a weak_ptr<PerThreadData>.
  - When the deleter runs:
      - if capturing weak_ptr can be locked, it pushes the raw pointer into that PerThreadData->pending queue (with mutex).
      - otherwise (per-thread data gone), it calls ownerPool.release() directly.
  - A background reclaimer thread periodically scans all registered PerThreadData and drains their pending queues by calling pool->release() for each object (this is safe because pool is heap-allocated and accessible from background thread).
  - The reclaimer runs until stop_reclaimer() is called or program exit.

 Usage:
   ThreadLocalObjectPool::start_reclaimer(std::chrono::milliseconds(200));
   auto p = ThreadLocalObjectPool::create<Buffer>(...);
   // p may be destroyed on any thread; the object will be returned to origin pool automatically.
   ThreadLocalObjectPool::stop_reclaimer();
*/

class ThreadLocalObjectPool {
public:
    struct PerThreadData {
        std::unique_ptr<ObjectPool> pool;
        std::mutex pending_mtx;
        std::vector<void*> pending;
        std::thread::id tid;
        PerThreadData(size_t global_max = 1000)
            : pool(std::make_unique<ObjectPool>(global_max)), tid(std::this_thread::get_id()) {}
        // Drain pending into local pool (caller must own pending_mtx)
        void drain_pending_to_pool()
        {
            std::vector<void*> tmp;
            {
                std::lock_guard<std::mutex> lock(pending_mtx);
                tmp.swap(pending);
            }
            for (void* obj : tmp) {
                pool->release(obj);
            }
        }
    };

    // Register and return current thread's PerThreadData (thread_local)
    static std::shared_ptr<PerThreadData> get_per_thread_data()
    {
        thread_local std::shared_ptr<PerThreadData> tdata = nullptr;
        if (!tdata) {
            tdata = std::make_shared<PerThreadData>(global_max_pool_size_);
            instance().register_thread_data(tdata);
        }
        return tdata;
    }

    // Create API (matches ObjectPool)
    template<typename T, typename... Args>
    static std::shared_ptr<T> create(Args&&... args)
    {
        auto tdata = get_per_thread_data();
        // create raw in the per-thread pool
        T* raw = tdata->pool->create_raw<T>(std::forward<Args>(args)...);

        // create shared_ptr with custom deleter that returns to origin thread's pending queue
        std::weak_ptr<PerThreadData> weak_td(tdata);
        return std::shared_ptr<T>(raw, [weak_td](T* p) {
            auto td = weak_td.lock();
            if (td) {
                // try fast path: if we're on same thread, return directly
                if (std::this_thread::get_id() == td->tid) {
                    td->pool->release(static_cast<void*>(p));
                } else {
                    // push into origin thread's pending queue
                    std::lock_guard<std::mutex> lock(td->pending_mtx);
                    td->pending.push_back(static_cast<void*>(p));
                }
            } else {
                // origin thread gone, fallback: delete
                ::operator delete(static_cast<void*>(p));
            }
        });
    }

    template<typename T, typename InitFunc>
    static std::shared_ptr<T> create_with_init(InitFunc&& init)
    {
        auto sp = create<T>();
        std::invoke(std::forward<InitFunc>(init), *sp);
        return sp;
    }

    template<typename T>
    static void reserve(size_t n) { get_per_thread_data()->pool->reserve<T>(n); }

    template<typename T>
    static size_t available() { return get_per_thread_data()->pool->available<T>(); }

    template<typename T>
    static void set_max_pool_size(size_t size) { get_per_thread_data()->pool->set_max_pool_size<T>(size); }

    static void set_global_max_pool_size(size_t size) {
        global_max_pool_size_ = size;
        // Update existing per-thread pools
        instance().update_all_global_max(size);
    }

    // --- Reclaimer control (automatic background thread) -----------------
    static void start_reclaimer(std::chrono::milliseconds period = std::chrono::milliseconds(200))
    {
        instance().start_worker(period);
    }

    static void stop_reclaimer()
    {
        instance().stop_worker();
    }

    // Drain current thread pending queue synchronously
    static void drain_current_thread_returns()
    {
        auto td = get_per_thread_data();
        td->drain_pending_to_pool();
    }

#ifdef ENABLE_PROMETHEUS_METRICS
    static void set_registry(std::shared_ptr<prometheus::Registry> reg)
    {
        daric::object_pool::internal::PrometheusMetrics::instance().set_registry(reg);
    }
#endif


private:
    ThreadLocalObjectPool() : running_(false) {}
    ~ThreadLocalObjectPool() { stop_worker(); }

    static ThreadLocalObjectPool& instance()
    {
        static ThreadLocalObjectPool inst;
        return inst;
    }

    void register_thread_data(std::shared_ptr<PerThreadData> td)
    {
        std::lock_guard<std::mutex> lock(registry_mtx_);
        registry_[td->tid] = td;
    }

    void update_all_global_max(size_t size)
    {
        std::lock_guard<std::mutex> lock(registry_mtx_);
        for (auto& kv : registry_) {
            kv.second->pool->set_global_max_pool_size(size);
        }
    }

    // background worker
    void start_worker(std::chrono::milliseconds period)
    {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true)) return; // already running

        worker_period_ = period;
        worker_thread_ = std::thread([this] {
            while (running_.load()) {
                std::this_thread::sleep_for(worker_period_);
                // snapshot registry to avoid holding lock during drains
                std::vector<std::shared_ptr<PerThreadData>> snapshot;
                {
                    std::lock_guard<std::mutex> lock(registry_mtx_);
                    snapshot.reserve(registry_.size());
                    for (auto& kv : registry_) snapshot.push_back(kv.second);
                }
                // drain each per-thread pending queue into that thread's pool
                for (auto& td : snapshot) {
                    // move pending into temp under lock and then release into pool
                    std::vector<void*> tmp;
                    {
                        std::lock_guard<std::mutex> lock(td->pending_mtx);
                        tmp.swap(td->pending);
                    }
                    for (void* obj : tmp) {
                        td->pool->release(obj);
                    }
                }
            }
        });
    }

    void stop_worker()
    {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) return;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    std::mutex registry_mtx_;
    std::unordered_map<std::thread::id, std::shared_ptr<PerThreadData>> registry_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    std::chrono::milliseconds worker_period_{200};

    static inline std::atomic<size_t> global_max_pool_size_{1000};
};
}