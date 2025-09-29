#pragma once
#include <string>
#include <mutex>
#include <iostream>

namespace daric {

enum class LogLevel { Info, Warning, Error };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string prefix;
        switch(level) {
            case LogLevel::Info: prefix = "[INFO] "; break;
            case LogLevel::Warning: prefix = "[WARN] "; break;
            case LogLevel::Error: prefix = "[ERROR] "; break;
        }
        std::cout << prefix << message << std::endl;
    }

private:
    Logger() = default;
    std::mutex mutex_;
};

} // namespace daric
