// #ifdef BUILD_OBJECT_POOL

#include "daric/daric.h"

#include <prometheus/registry.h>
#include <prometheus/exposer.h>

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

// Example message class
struct Message {
    std::string src;
    std::string dst;
    std::string text;

    Message(std::string s, std::string d, std::string t)
        : src(std::move(s)), dst(std::move(d)), text(std::move(t)) {
        std::cout << "[+] Message constructed: " << text << "\n";
    }

    void reset() {
        src.clear();
        dst.clear();
        text.clear();
    }

    ~Message() {
        std::cout << "[-] Message destructed (raw delete)\n";
    }
};

// Worker that creates and uses pooled messages
void worker_thread(int id)
{
    for (int i = 0; i < 5; ++i) {
        auto msg = ThreadLocalObjectPool::create<Message>(
            "SRC_" + std::to_string(id),
            "DST_" + std::to_string(i),
            "Hello from thread " + std::to_string(id)
        );

        std::cout << "Thread " << id << " created: " << msg->text << "\n";

        // Simulate passing message to another thread
        std::thread([m = msg, id] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout << "Thread " << std::this_thread::get_id()
                      << " destroying msg from " << id << "\n";
        }).detach();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "Thread " << id << " finished creating.\n";
}

int main()
{
    using namespace std::chrono_literals;

    auto exposer = std::make_unique<prometheus::Exposer>("0.0.0.0:9999");
    auto registry = std::make_shared<prometheus::Registry>();
    PrometheusMetrics::instance().set_registry(registry);

    // Start the background reclaimer
    ThreadLocalObjectPool::start_reclaimer(200ms);

    // Launch worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < 3; ++i)
        workers.emplace_back(worker_thread, i);

    for (auto& t : workers)
        t.join();

    std::cout << "All threads done creating.\n";

    // Give time for cross-thread releases to accumulate and be reclaimed
    std::this_thread::sleep_for(1s);

    // Manually drain current thread (main thread) pending returns
    ThreadLocalObjectPool::drain_current_thread_returns();

    // Stop background reclaimer (cleanup)
    ThreadLocalObjectPool::stop_reclaimer();

    std::cout << "Program exiting cleanly.\n";
}

// #endif
