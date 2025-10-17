// object_pool.h
#pragma once

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "internal/prometheus_metrics.h"

namespace daric::object_pool
{
class ObjectPool : public std::enable_shared_from_this<ObjectPool>
{
  public:
    ObjectPool() = default;
    ~ObjectPool() = default;

    // Set global maximum pool size for all subpools
    void set_global_max_pool_size(size_t ms)
    {
        std::lock_guard<std::mutex> g(pools_mtx_);
        global_max_pool_size_ = ms;
        // update existing subpools safely
        for (auto& p : pools_)
        {
            p.second->set_max_size(ms);
        }
    }

    // Create/obtain an object of type T (forward constructor args)
    template<typename T, typename... Args>
    std::shared_ptr<T> create(Args&&... args)
    {
        // Obtain or create the typed subpool
        SubPool<T>* sub = getOrCreateSubPool<T>();

        T* raw = nullptr;
        {
            std::lock_guard<std::mutex> lg(sub->mtx_);
            if (!sub->pool.empty())
            {
                std::unique_ptr<T> up = std::move(sub->pool.back());
                sub->pool.pop_back();
                raw = up.release();
            }
        }

        if (raw)
        {
            // reuse: destroy and placement-new with new args
            raw->~T();
            try
            {
                ::new (static_cast<void*>(raw)) T(std::forward<Args>(args)...);
            }
            catch (...)
            {
                // If ctor throws, free memory
                delete raw;
                throw;
            }
        }
        else
        {
            // allocate new
            raw = new T(std::forward<Args>(args)...);
        }

        // track lifetime metadata (thread-safe)
        {
            std::lock_guard<std::mutex> lg(lifetime_mtx_);
            LifetimeInfo info;
            info.start = std::chrono::steady_clock::now();
            info.type = std::type_index(typeid(T));
            info.subpool = sub;
            lifetime_map_[raw] = std::move(info);
        }

        // create shared_ptr with safe deleter that captures a weak_ptr to this pool
        std::weak_ptr<ObjectPool> weak_pool = this->weak_from_this();
        auto deleter = [weak_pool](T* p) {
            if (auto pool = weak_pool.lock())
            {
                pool->release(static_cast<void*>(p));
            }
            else
            {
                // Pool no longer exists: delete the object to avoid leak.
                delete p;
            }
        };
        return std::shared_ptr<T>(raw, std::move(deleter));
    }

    // Number of objects currently available in all subpools
    size_t available() const
    {
        size_t total = 0;
        std::lock_guard<std::mutex> g(pools_mtx_);
        for (auto const& kv : pools_)
        {
            ISubPool* base = kv.second.get();
            total += base->in_pool_size();
        }
        return total;
    }

  private:
    struct ISubPool
    {
        virtual ~ISubPool() = default;
        virtual void release(void* obj) = 0;
        virtual size_t in_pool_size() const = 0;
        virtual void set_max_size(size_t) = 0;
    };

    template<typename T>
    struct SubPool final : ISubPool
    {
        std::vector<std::unique_ptr<T>> pool;
        size_t max_pool_size_{1000};
        mutable std::mutex mtx_;  // protects pool and max_pool_size_

        void release(void* obj) override
        {
            T* t = static_cast<T*>(obj);
            bool returned = false;
            {
                std::lock_guard<std::mutex> lg(mtx_);
                if (pool.size() < max_pool_size_)
                {
                    // wrap raw pointer into unique_ptr and push back
                    pool.emplace_back(std::unique_ptr<T>(t));
                    returned = true;
                }
            }

            if (!returned)
            {
                // pool full: destroy object and update metrics
                delete t;
                try
                {
                    internal::PrometheusMetrics::instance()
                        .get_handles(std::type_index(typeid(T)),
                                     std::this_thread::get_id())
                        .dropped->Increment();
                }
                catch (...)
                {
                    // metric failures must not throw on the hot path
                }
            }
        }

        size_t in_pool_size() const override
        {
            std::lock_guard<std::mutex> lg(mtx_);
            return pool.size();
        }
        void set_max_size(size_t s) override
        {
            std::lock_guard<std::mutex> lg(mtx_);
            max_pool_size_ = s;
        }
    };

    struct LifetimeInfo
    {
        std::chrono::steady_clock::time_point start;
        std::type_index type{typeid(void)};
        ISubPool* subpool{nullptr};
    };

    // release called from deleter
    void release(void* obj)
    {
        // Remove from lifetime map and return to correct subpool
        LifetimeInfo info;
        {
            std::lock_guard<std::mutex> lg(lifetime_mtx_);
            auto it = lifetime_map_.find(obj);
            if (it == lifetime_map_.end())
            {
                // Unexpected: not tracked by this pool
                std::cerr << "ObjectPool::release(): unknown object pointer; possible "
                             "double free or external pointer.\n";
                assert(false && "release() called for unknown pointer");
                ::operator delete(obj);  // fallback: free raw memory (best-effort)
                return;
            }
            info = it->second;
            lifetime_map_.erase(it);
        }

        // use stored subpool pointer to avoid extra lookup
        if (info.subpool)
        {
            info.subpool->release(obj);
            // update metrics
            try
            {
                internal::PrometheusMetrics::instance()
                    .get_handles(info.type, std::this_thread::get_id())
                    .returning->Increment();
            }
            catch (...)
            {
            }
        }
        else
        {
            // should not happen, but be defensive
            std::cerr << "ObjectPool::release(): subpool lost; deleting object\n";
            delete static_cast<char*>(
                obj);  // fallback - but better to delete with proper type, we can't
        }
    }

    // type-indexed subpools
    template<typename T>
    SubPool<T>* getOrCreateSubPool()
    {
        std::type_index idx(typeid(T));
        {
            std::lock_guard<std::mutex> g(pools_mtx_);
            auto it = pools_.find(idx);
            if (it != pools_.end())
            {
                return static_cast<SubPool<T>*>(it->second.get());
            }

            auto up = std::make_unique<SubPool<T>>();
            up->max_pool_size_ = global_max_pool_size_;
            SubPool<T>* raw = up.get();
            pools_.emplace(idx, std::move(up));
            return raw;
        }
    }

    // Members
    size_t global_max_pool_size_{1000};

    // Protects pools_ map
    mutable std::mutex pools_mtx_;
    std::unordered_map<std::type_index, std::unique_ptr<ISubPool>> pools_;

    // lifetime tracking
    mutable std::mutex lifetime_mtx_;
    std::unordered_map<void*, LifetimeInfo> lifetime_map_;
};
}  // namespace daric::object_pool