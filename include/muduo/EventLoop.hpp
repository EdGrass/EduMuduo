#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "Noncopyable.hpp"
#include "Timestamp.hpp"
#include "CurrentThread.hpp"
#include "Logger.hpp"

/*
 * Reactors in the model, responsible for:  
 * - Event looping  
 * - Event dispatching  
 * - Event handling  
 * 
 * Contains the following key components:  
 * - activeChannels_ : Stores currently active Channels  
 * - wakeupFd_       : Used by mainloop to wake up subloops. Writing data to  
 *                     wakeupFd_ will trigger loop awakening.  
 * - pendingFunctors_: Ensures all tasks are executed within the loop's thread  
 *                     during a single iteration.  
 */

class Poller;
class Channel;

class EventLoop : Noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    bool isInLoopThread() const { 
        return threadId_ == CurrentThread::tid(); 
    }

    Timestamp pollReturnTime() const { 
        return pollReturnTime_; 
    }

private:
    static const int kPollTimeMs = 10000;
    static __thread EventLoop* t_loopInThisThread;

    static int createEventfd();
    void handleRead();
    void wakeup();
    void doPendingFunctors();

    std::atomic_bool looping_;
    std::atomic_bool quit_;
    const pid_t threadId_;
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;
    std::vector<Channel*> activeChannels_;
    std::atomic_bool callingPendingFunctors_;
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;
};