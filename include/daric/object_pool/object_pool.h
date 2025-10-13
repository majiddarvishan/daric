#pragma once

#include "prometheus_metrics.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <type_traits>
#include <functional>
#include <new>
#include <utility>
#include <cassert>
#include <mutex>
#include <chrono>

/*
 Generic ObjectPool
 - type-agnostic
 - create_raw<T>(args...) -> returns raw pointer and records lifetime/type
 - make_shared_with_owner<T>(raw, ownerPool) -> returns shared_ptr whose deleter calls owner->release
 - release(void* obj) -> returns object back into the proper subpool
 - metrics integrated via PrometheusMetrics
*/
class ObjectPool
{
public:
    explicit ObjectPool(size_t global_max_pool_size = 1000)
        : global_max_pool_size_(global_max_pool_size) {}

    ~ObjectPool() = default;

    // Create but return raw pointer (caller will construct shared_ptr with custom deleter)
    template<typename T, typename... Args>
    T* create_raw(Args&&... args)
    {
        static_assert(!std::is_void_v<T>, "T must be object type");
        auto& subPool = getOrCreateSubPool<T>();
        auto& metrics = PrometheusMetrics::instance().get_handles(std::type_index(typeid(T)),
                                                                  std::this_thread::get_id());

        T* raw = nullptr;

        if (!subPool.pool.empty()) {
            // reuse memory block owned by unique_ptr<T>
            std::unique_ptr<T> up = std::move(subPool.pool.back());
            subPool.pool.pop_back();
            raw = up.release();// raw points to a previously constructed T
            raw->~T();// explicitly destroy old instance

            // placement-new into the same memory. If ctor throws, delete raw to avoid leak.
            try {
                ::new (static_cast<void*>(raw)) T(std::forward<Args>(args)...);
                metrics.reused->Increment();
            } catch (...) {
                delete raw;
                throw;
            }
        } else {
            // allocate fresh when pool empty
            raw = new T(std::forward<Args>(args)...);
            metrics.created->Increment();
        }

        {
            std::lock_guard<std::mutex> lock(lifetime_mtx_);
            lifetime_map_[raw] = {std::chrono::steady_clock::now(), std::type_index(typeid(T))};
        }

        metrics.max_size->Set(static_cast<double>(subPool.max_pool_size_));

        return raw;
    }

    // Convenience: create shared_ptr that uses this pool as owner (deleter calls owner->release)
    template<typename T, typename... Args>
    std::shared_ptr<T> create_shared(Args&&... args)
    {
        T* raw = create_raw<T>(std::forward<Args>(args)...);
        return make_shared_with_owner<T>(raw, this);
    }

    // Construct shared_ptr from raw pointer and make pool the owner (deleter calls owner->release)
    template<typename T>
    std::shared_ptr<T> make_shared_with_owner(T* raw, ObjectPool* owner)
    {
        return std::shared_ptr<T>(raw, [owner](T* p) {
            if (owner) owner->release(static_cast<void*>(p));
            else delete p;
        });
    }

    // --- Create with initializer lambda (default construct then initialize)
    template<typename T, typename InitFunc>
    std::shared_ptr<T> create_with_init(InitFunc&& init)
    {
        auto sp = create_shared<T>();
        std::invoke(std::forward<InitFunc>(init), *sp);
        return sp;
    }

    // Release an object back to its subpool
    void release(void* obj)
    {
        if (!obj) return;

        LifetimeInfo info;
        {
            std::lock_guard<std::mutex> lock(lifetime_mtx_);
            auto it = lifetime_map_.find(obj);
            if (it == lifetime_map_.end()) {
                // Unknown object (lost metadata), just delete (best effort)
                ::operator delete(obj);
                return;
            }
            info = it->second;
            lifetime_map_.erase(it);
        }

        const double lifetime = std::chrono::duration<double>(std::chrono::steady_clock::now() - info.start).count();

        auto typeId = info.type;
        auto threadId = std::this_thread::get_id();
        auto& metrics = PrometheusMetrics::instance().get_handles(typeId, threadId);

        if (lifetime > 0)
            metrics.lifetime_hist->Observe(lifetime);

        auto it = pools_.find(typeId);
        if (it != pools_.end()) {
            it->second->release(obj);
            metrics.released->Increment();
            metrics.in_pool->Set(static_cast<double>(it->second->in_pool_size()));
        } else {
            // Fallback
            ::operator delete(obj);
        }
    }

    // --- Pre-allocate N default-constructed objects for type T
    template<typename T>
    void reserve(size_t n)
    {
        auto& subPool = getOrCreateSubPool<T>();
        subPool.pool.reserve(n);
        while (subPool.pool.size() < n && subPool.pool.size() < subPool.max_pool_size_) {
            subPool.pool.push_back(std::make_unique<T>());
        }
    }

    // --- Monitoring helper
    template<typename T>
    size_t available() const
    {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) return 0;
        return static_cast<const SubPool<T>*>(it->second.get())->pool.size();
    }

    // --- Configuration: global & per-type max sizes
    void set_global_max_pool_size(size_t size)
    {
        global_max_pool_size_ = size;
        for (auto& [_, base] : pools_)
            base->set_max_size(size);
    }

    template<typename T>
    void set_max_pool_size(size_t size)
    {
        getOrCreateSubPool<T>().max_pool_size_ = size;
        auto& metrics = PrometheusMetrics::instance().get_handles(std::type_index(typeid(T)),
                                                                  std::this_thread::get_id());
        metrics.max_size->Set(static_cast<double>(size));
    }

private:
    struct LifetimeInfo {
        std::chrono::steady_clock::time_point start;
        std::type_index type{typeid(void)};  // Default to void type
    };

    std::unordered_map<void*, LifetimeInfo> lifetime_map_;
    std::mutex lifetime_mtx_;
    size_t global_max_pool_size_;


    // Base abstraction for per-type subpools
    struct ISubPool {
        virtual ~ISubPool() = default;
        virtual void release(void* obj) = 0;
        virtual size_t in_pool_size() const = 0;
        virtual void set_max_size(size_t size) = 0;
    };

    // Typed subpool implementation
    template<typename T>
    struct SubPool : ISubPool {
        std::vector<std::unique_ptr<T>> pool;
        size_t max_pool_size_{1000};

        void release(void* obj) override
        {
            // Cast to correct type
            T* t = static_cast<T*>(obj);
            auto& metrics = PrometheusMetrics::instance()
                .get_handles(std::type_index(typeid(T)), std::this_thread::get_id());

            if constexpr (requires(T& o) { o.reset(); }) {
                try { t->reset(); } catch (...) {
                    // swallow reset exceptions to avoid pool corruption;
                    // if reset fails, we still push the object back to the pool.
                }
            }

            if (pool.size() < max_pool_size_) {
                // Re-wrap raw pointer into unique_ptr<T> and push back to pool.
                pool.emplace_back(t);
            } else {
                // Pool is full → delete the message to prevent unbounded growth
                delete t;
                metrics.dropped->Increment();
            }

            metrics.max_size->Set(static_cast<double>(max_pool_size_));
        }

        size_t in_pool_size() const override { return pool.size(); }
        void set_max_size(size_t size) override { max_pool_size_ = size; }
    };

    // Map: type_index → subpool
    std::unordered_map<std::type_index, std::unique_ptr<ISubPool>> pools_;

    // Get or create subpool for T
    template<typename T>
    SubPool<T>& getOrCreateSubPool()
    {
        auto idx = std::type_index(typeid(T));
        auto it = pools_.find(idx);
        if (it == pools_.end()) {
            auto up = std::make_unique<SubPool<T>>();
            SubPool<T>* raw = up.get();
            raw->max_pool_size_ = global_max_pool_size_;
            pools_.emplace(idx, std::move(up));

            auto& metrics = PrometheusMetrics::instance().get_handles(idx, std::this_thread::get_id());
            metrics.max_size->Set(static_cast<double>(global_max_pool_size_));
            return *raw;
        }
        return *static_cast<SubPool<T>*>(pools_[idx].get());
    }
};
