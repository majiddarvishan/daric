#pragma once

#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/exposer.h>
#include <unordered_map>
#include <typeindex>
#include <mutex>
#include <thread>
#include <sstream>
#include <memory>

class PrometheusMetrics {
public:
    static PrometheusMetrics& instance() {
        static PrometheusMetrics inst;
        return inst;
    }

    // void start_http_server(uint16_t port = 8080) {
    //     if (!exposer_) {
    //         exposer_ = std::make_unique<prometheus::Exposer>("0.0.0.0:" + std::to_string(port));
    //         exposer_->RegisterCollectable(registry_);
    //     }
    // }

    void set_registry(std::shared_ptr<prometheus::Registry> registry)
    {
        if (!registry_)
        {
            registry_ = registry;
            // exposer_->RegisterCollectable(registry_);
        }
    }

    struct MetricHandles {
        prometheus::Counter* created;
        prometheus::Counter* reused;
        prometheus::Counter* released;
        prometheus::Counter* dropped;
        prometheus::Gauge* in_pool;
        prometheus::Gauge* max_size;
        prometheus::Histogram* lifetime_hist;
    };

    MetricHandles& get_handles(const std::type_index& typeId, std::thread::id threadId) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto key = std::make_pair(typeId, threadId);
        auto it = metrics_.find(key);
        if (it != metrics_.end()) return it->second;

        std::string typeName = typeId.name();
        std::ostringstream tid;
        tid << threadId;

        auto& created_family = prometheus::BuildCounter()
            .Name("pool_objects_created_total")
            .Help("Total objects created per type/thread")
            .Register(*registry_);

        auto& reused_family = prometheus::BuildCounter()
            .Name("pool_objects_reused_total")
            .Help("Total objects reused per type/thread")
            .Register(*registry_);

        auto& released_family = prometheus::BuildCounter()
            .Name("pool_objects_released_total")
            .Help("Total objects released per type/thread")
            .Register(*registry_);

        auto& dropped_family = prometheus::BuildCounter()
            .Name("pool_objects_dropped_total")
            .Help("Objects deleted due to max pool size limit")
            .Register(*registry_);

        auto& in_pool_family = prometheus::BuildGauge()
            .Name("pool_objects_in_pool")
            .Help("Objects currently in pool per type/thread")
            .Register(*registry_);

        auto& max_family = prometheus::BuildGauge()
            .Name("pool_max_size")
            .Help("Configured maximum pool size per type/thread")
            .Register(*registry_);

        // Histogram: lifetime in seconds (log-scale buckets)
        auto& lifetime_family = prometheus::BuildHistogram()
            .Name("pool_message_lifetime_seconds")
            .Help("Lifetime of pooled messages (seconds)")
            .Register(*registry_);

        auto& h = metrics_[key];
        h.created = &created_family.Add({{"type", typeName}, {"thread", tid.str()}});
        h.reused = &reused_family.Add({{"type", typeName}, {"thread", tid.str()}});
        h.released = &released_family.Add({{"type", typeName}, {"thread", tid.str()}});
        h.dropped = &dropped_family.Add({{"type", typeName}, {"thread", tid.str()}});
        h.in_pool = &in_pool_family.Add({{"type", typeName}, {"thread", tid.str()}});
        h.max_size = &max_family.Add({{"type", typeName}, {"thread", tid.str()}});
        h.lifetime_hist = &lifetime_family.Add(
            {{"type", typeName}, {"thread", tid.str()}},
            prometheus::Histogram::BucketBoundaries{0.0005, 0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1, 5}
        );
        return h;
    }

private:
    PrometheusMetrics()
    // : registry_(std::make_shared<prometheus::Registry>())
    {}

    std::shared_ptr<prometheus::Registry> registry_;
    // std::unique_ptr<prometheus::Exposer> exposer_;
    std::mutex mtx_;

    struct KeyHash {
        std::size_t operator()(const std::pair<std::type_index, std::thread::id>& p) const noexcept {
            return std::hash<std::type_index>{}(p.first) ^ std::hash<std::thread::id>{}(p.second);
        }
    };
    struct KeyEq {
        bool operator()(const std::pair<std::type_index, std::thread::id>& a,
                        const std::pair<std::type_index, std::thread::id>& b) const noexcept {
            return a.first == b.first && a.second == b.second;
        }
    };

    std::unordered_map<std::pair<std::type_index, std::thread::id>,
                       MetricHandles, KeyHash, KeyEq> metrics_;
};
