#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <typeindex>
#include <unordered_map>

// Minimal shim for prometheus client usage
// Replace with your project's includes when available.
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

namespace daric::object_pool::internal
{
class PrometheusMetrics
{
  public:
    static PrometheusMetrics& instance()
    {
        static PrometheusMetrics singleton;
        return singleton;
    }

    // Optionally set registry from outside; by default we create one so code is
    // safe.
    void set_registry(std::shared_ptr<prometheus::Registry> r)
    {
        std::lock_guard<std::mutex> lg(mtx_);
        if (r)
            registry_ = std::move(r);
    }

    struct Handles
    {
        prometheus::Counter* created{nullptr};
        prometheus::Counter* returning{nullptr};
        prometheus::Counter* dropped{nullptr};
        prometheus::Gauge* in_pool{nullptr};
    };

    // Get handles for a (type, thread). This registers metrics lazily and caches
    // them.
    Handles get_handles(std::type_index type, std::thread::id tid)
    {
        std::lock_guard<std::mutex> lg(mtx_);
        // key mix
        Key k{type, tid};
        auto it = handles_.find(k);
        if (it != handles_.end())
            return it->second;

        // lazy register
        if (!registry_)
        {
            // create default registry to avoid nullptr deref
            registry_ = std::make_shared<prometheus::Registry>();
        }

        try
        {
            auto& fam_created = prometheus::BuildCounter()
                                    .Name("object_pool_created_total")
                                    .Help("Objects created")
                                    .Register(*registry_);
            auto& fam_returning = prometheus::BuildCounter()
                                      .Name("object_pool_returning_total")
                                      .Help("Objects returned to pool")
                                      .Register(*registry_);
            auto& fam_dropped = prometheus::BuildCounter()
                                    .Name("object_pool_dropped_total")
                                    .Help("Objects dropped (pool full)")
                                    .Register(*registry_);
            auto& fam_inpool = prometheus::BuildGauge()
                                   .Name("object_pool_in_pool")
                                   .Help("Objects currently in pool")
                                   .Register(*registry_);

            Handles h;
            h.created = &fam_created.Add(
                {{"type", type.name()}, {"thread", thread_id_to_string(tid)}});
            h.returning = &fam_returning.Add(
                {{"type", type.name()}, {"thread", thread_id_to_string(tid)}});
            h.dropped = &fam_dropped.Add(
                {{"type", type.name()}, {"thread", thread_id_to_string(tid)}});
            h.in_pool = &fam_inpool.Add(
                {{"type", type.name()}, {"thread", thread_id_to_string(tid)}});

            handles_.emplace(k, h);
            return h;
        }
        catch (const std::exception& e)
        {
            std::cerr << "PrometheusMetrics::get_handles() registration failed: "
                      << e.what() << "\n";
            // return empty handles
            return Handles{};
        }
        catch (...)
        {
            std::cerr << "PrometheusMetrics::get_handles() unknown error\n";
            return Handles{};
        }
    }

  private:
    PrometheusMetrics()
      : registry_(std::make_shared<prometheus::Registry>())
    {}
    ~PrometheusMetrics() = default;

    // Key for cache
    struct Key
    {
        std::type_index type;
        std::thread::id tid;
        bool operator==(Key const& o) const
        {
            return type == o.type && tid == o.tid;
        }
    };

    struct KeyHash
    {
        size_t operator()(Key const& k) const noexcept
        {
            size_t a = std::hash<std::type_index>{}(k.type);
            size_t b = std::hash<std::thread::id>{}(k.tid);
            // combine
            return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
        }
    };

    std::string thread_id_to_string(std::thread::id tid)
    {
        std::ostringstream ss;
        ss << tid;
        return ss.str();
    }

    mutable std::mutex mtx_;
    std::shared_ptr<prometheus::Registry> registry_;

    std::unordered_map<Key, Handles, KeyHash> handles_;
};
}  // namespace daric::object_pool::internal
