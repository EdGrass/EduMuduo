#pragma once

#include <string>
#include <mutex>
#include <iostream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fmt/format.h>

#include "Noncopyable.hpp"
#include "Timestamp.hpp"

enum class LogLevel : uint8_t {
    Debug,  
    Error,  
    Fatal  
};

class Logger : public Noncopyable {
public:
    static Logger& instance() noexcept {
        static Logger logger;
        return logger;
    }

    void set_level(LogLevel level) noexcept { 
        std::lock_guard lock(mutex_);
        current_level_ = level;
    }

    LogLevel level() const noexcept { 
        std::lock_guard lock(mutex_);
        return current_level_; 
    }

    template <typename... Args>
    void log(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args) {
        if (level < current_level_) return;

        try {
            std::string msg = fmt::format(fmt, std::forward<Args>(args)...);
            output(level, msg);
        } catch (const fmt::format_error& e) {
            output(LogLevel::Error, fmt::format("[FORMAT_ERROR] {}", e.what()));
        }
    }

    class LogStream {
    public:
        explicit LogStream(Logger& logger, LogLevel level) noexcept 
            : logger_(logger), level_(level) {}
        
        ~LogStream() {
            if (!buffer_.empty()) {
                logger_.log(level_, "{}", buffer_);
            }
        }

        template <typename T>
        LogStream& operator<<(T&& value) {
            buffer_ += fmt::to_string(std::forward<T>(value));
            return *this;
        }

    private:
        Logger& logger_;
        LogLevel level_;
        std::string buffer_;
    };

    LogStream stream(LogLevel level) noexcept { 
        return LogStream(*this, level); 
    }

    void log(const std::string& msg) {
        output(LogLevel::Debug, msg);
    }

private:
    Logger() = default;

    void output(LogLevel level, const std::string& message) {
        std::lock_guard lock(mutex_);

        const auto timestamp = Timestamp::now().toString();
        const auto level_str = [level] {
            switch (level) {
                case LogLevel::Debug: return "[DEBUG]";
                case LogLevel::Error: return "[ERROR]";
                case LogLevel::Fatal: return "[FATAL]";
                default:              return "[UNKNOWN]";
            }
        }();

        std::cerr << level_str << ' ' << timestamp << " | " << message << '\n';
    }

    LogLevel current_level_ = LogLevel::Debug;
    mutable std::mutex mutex_;
};

#define LOG_DEBUG(...)  \
    do { \
        if (Logger::instance().level() <= LogLevel::Debug) \
            Logger::instance().stream(LogLevel::Debug) << fmt::format(__VA_ARGS__); \
    } while(0)

#define LOG_ERROR(...)  \
    Logger::instance().log(LogLevel::Error, __VA_ARGS__)

#define LOG_FATAL(...)  \
    do { \
        Logger::instance().log(LogLevel::Fatal, __VA_ARGS__); \
        std::exit(EXIT_FAILURE); \
    } while(0)

#ifdef MUDEBUG
#  define DEBUG_LOG(...) LOG_DEBUG(__VA_ARGS__)
#else
#  define DEBUG_LOG(...) 
#endif
