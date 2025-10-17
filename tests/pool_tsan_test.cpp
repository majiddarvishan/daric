// pool_tsan_test.cpp
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

// #include "object_pool.h"
#include "daric/daric.h"

using namespace daric::object_pool;

// A small test object
struct TestObj {
    int x;
    TestObj() : x(0) {}
    TestObj(int v) : x(v) {}
    ~TestObj() {
        // cheap destructor
        x = -1;
    }
};

int main() {
    std::cout << "TSAN test: starting\n";

    // make_shared so weak_from_this works
    auto pool = std::make_shared<ObjectPool>();
    pool->set_global_max_pool_size(1000);

    const int nthreads = 16;
    const int ops_per_thread = 100000;

    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    threads.reserve(nthreads);

    for (int t = 0; t < nthreads; ++t) {
        threads.emplace_back([&, t]() {
            // spin until all threads ready
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < ops_per_thread; ++i) {
                {
                    auto obj = pool->create<TestObj>(i);
                    // use object a bit
                    obj->x = i ^ t;
                }
                // let shared_ptr go out of scope -> should call pool deleter concurrently
                if ((i & 0xFFF) == 0) {
                    // occasional sleep to create interleaving
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        });
    }

    auto t0 = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);

    for (auto &th : threads) th.join();
    auto t1 = std::chrono::steady_clock::now();

    std::chrono::duration<double> dt = t1 - t0;
    std::cout << "Completed: " << (nthreads * ops_per_thread) << " ops in " << dt.count() << "s\n";
    std::cout << "Available objects in pool: " << pool->available() << "\n";
    std::cout << "TSAN test: finished\n";
    return 0;
}
