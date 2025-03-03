#include <sstream>
#include <utility>

#include <muduo/EventLoopThreadPool.hpp>

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, std::string name)
    : baseLoop_(baseLoop),
      name_(std::move(name)),
      started_(false),
      numThreads_(0),
      next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() = default;

void EventLoopThreadPool::setThreadNum(int numThreads) noexcept {
    numThreads_ = numThreads;
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    started_.store(true, std::memory_order_release);

    threads_.reserve(numThreads_);
    loops_.reserve(numThreads_);

    for (int i = 0; i < numThreads_; ++i) {
        std::ostringstream oss;
        oss << name_ << i;
        threads_.emplace_back(std::make_unique<EventLoopThread>(cb, oss.str()));
        loops_.push_back(threads_.back()->startLoop());
    }

    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() noexcept {
    if (loops_.empty()) {
        return baseLoop_;
    }
    return loops_[next_.fetch_add(1, std::memory_order_relaxed) % loops_.size()];
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() const noexcept {
    return loops_.empty() ? std::vector<EventLoop*>{baseLoop_} : loops_;
}

bool EventLoopThreadPool::started() const noexcept {
    return started_.load(std::memory_order_acquire);
}

const std::string& EventLoopThreadPool::name() const noexcept {
    return name_;
}