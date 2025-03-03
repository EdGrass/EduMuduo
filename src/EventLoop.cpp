#include <sys/eventfd.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include <muduo/EventLoop.hpp>
#include <muduo/Channel.hpp>
#include <muduo/Poller.hpp>

__thread EventLoop* EventLoop::t_loopInThisThread = nullptr;

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      callingPendingFunctors_(false) 
{
    DEBUG_LOG("[EventLoop] Created @{}", static_cast<void*>(this));
    
    if (t_loopInThisThread) {
        LOG_FATAL("[EventLoop] Already exists @{} in thread {}", 
                 static_cast<void*>(t_loopInThisThread), threadId_);
    t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback([this](Timestamp){ handleRead(); });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    DEBUG_LOG("[EventLoop] Destroying @{}", static_cast<void*>(this));
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    looping_.store(true, std::memory_order_relaxed);
    quit_.store(false, std::memory_order_relaxed);

    LOG_DEBUG("[EventLoop] Starting loop @{}", static_cast<void*>(this));

    while (!quit_.load(std::memory_order_acquire)) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        for (Channel* channel : activeChannels_) {
            DEBUG_LOG("[EventLoop] Processing channel FD:{}", channel->fd());
            channel->handleEvent(pollReturnTime_);
        }
        doPendingFunctors(); 
    }

    LOG_DEBUG("[EventLoop] Stopped loop @{}", static_cast<void*>(this));
    looping_.store(false, std::memory_order_release);
}

void EventLoop::quit() {
    quit_.store(true, std::memory_order_release);
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_.load()) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel* channel) { 
    DEBUG_LOG("[EventLoop] Updating channel FD:{}", channel->fd());
    poller_->updateChannel(channel); 
}

void EventLoop::removeChannel(Channel* channel) { 
    DEBUG_LOG("[EventLoop] Removing channel FD:{}", channel->fd());
    poller_->removeChannel(channel); 
}

bool EventLoop::hasChannel(Channel* channel) { 
    return poller_->hasChannel(channel); 
}

int EventLoop::createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("[EventLoop] Create eventfd failed: {} ({})", 
                 errno, strerror(errno));
    }
    return evtfd;
}

void EventLoop::handleRead() {
    uint64_t one;
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("[EventLoop] Read {} bytes from wakeup fd (expected 8)", n);
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("[EventLoop] Write {} bytes to wakeup fd (expected 8)", n);
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_.store(true);

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    DEBUG_LOG("[EventLoop] Executing {} pending functors", functors.size());
    for (const auto& functor : functors) {
        functor();
    }

    callingPendingFunctors_.store(false);
}