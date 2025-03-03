#include <muduo/EventLoopThread.hpp>
#include <muduo/EventLoop.hpp>
#include <muduo/Logger.hpp>

EventLoopThread::EventLoopThread(ThreadInitCallback cb, std::string name)
    : exiting_(false),
      thread_([this] { threadFunc(); }, std::move(name)),
      callback_(std::move(cb)),
      loop_(nullptr) 
{}

EventLoopThread::~EventLoopThread() {
    exiting_.store(true, std::memory_order_relaxed);
    if (auto loop = loop_.load(std::memory_order_acquire)) {
        loop->quit();
    }
    if (thread_.joinable()) { 
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    thread_.start();
    
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return loop_.load() != nullptr; });
    return loop_.load(std::memory_order_relaxed);
}

void EventLoopThread::threadFunc() noexcept {
    try {
        EventLoop loop;
        
        if (callback_) {
            callback_(&loop);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            loop_.store(&loop, std::memory_order_release);
        }
        cond_.notify_one();

        loop.loop();
        
        loop_.store(nullptr, std::memory_order_release);
    } 
    catch (const std::exception& e) {  
        LOG_ERROR("[EventLoopThread] Exception: {}", e.what());
        exiting_.store(true, std::memory_order_relaxed);
    }
    catch (...) { 
        LOG_ERROR("[EventLoopThread] Unknown exception");
        exiting_.store(true, std::memory_order_relaxed);
    }
}