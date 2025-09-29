#include "daric/daric.h"
#include <chrono>

int main() {
    auto& logger = daric::Logger::instance();
    logger.log(daric::LogLevel::Info, "Starting Daric example");

    daric::ThreadPool pool(4);

    for(int i = 0; i < 10; ++i) {
        pool.enqueue([i, &logger]{
            logger.log(daric::LogLevel::Info, "Task " + std::to_string(i) + " running");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            logger.log(daric::LogLevel::Info, "Task " + std::to_string(i) + " finished");
        });
    }

    logger.log(daric::LogLevel::Info, "Main thread done, waiting for tasks...");
    std::this_thread::sleep_for(std::chrono::seconds(2)); // wait tasks finish
    return 0;
}
