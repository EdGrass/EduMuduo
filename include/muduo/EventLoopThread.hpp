#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>
#include <memory>
#include <atomic>

#include "Noncopyable.hpp"
#include "Thread.hpp"

class EventLoop;

class EventLoopThread : Noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    explicit EventLoopThread(ThreadInitCallback cb = {}, 
                           std::string name = {});
    ~EventLoopThread();

    [[nodiscard]] EventLoop* startLoop();

private:
    void threadFunc() noexcept;

    std::atomic<EventLoop*> loop_;
    std::atomic_bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};