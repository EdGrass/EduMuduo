#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <atomic>

#include "Noncopyable.hpp"
#include "EventLoop.hpp"
#include "EventLoopThread.hpp"

/*
 * getNextLoop: Retrieves the next subloop object via round-robin scheduling  
 * One loop per thread  
 */

class EventLoopThreadPool : Noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, std::string name);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) noexcept;
    void start(const ThreadInitCallback& cb = {});

    [[nodiscard]] EventLoop* getNextLoop() noexcept;
    [[nodiscard]] std::vector<EventLoop*> getAllLoops() const noexcept;
    [[nodiscard]] bool started() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;

private:
    EventLoop* baseLoop_;
    std::string name_;
    std::atomic_bool started_;
    int numThreads_;
    std::atomic_uint next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};