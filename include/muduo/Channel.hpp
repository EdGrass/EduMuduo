#pragma once

#include <functional>
#include <memory>
#include <sys/epoll.h>

#include "Noncopyable.hpp"
#include "Timestamp.hpp"
#include "Logger.hpp"
#include "EventLoop.hpp"

class EventLoop;

class Channel : Noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent(Timestamp receiveTime);

    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    void tie(const std::shared_ptr<void>& obj);
    
    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }
    int index() const { return index_; }
    void set_index(int idx) { index_ = idx; }
    EventLoop* ownerLoop() { return loop_; }

    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static inline const int kNoneEvent = 0;
    static inline const int kReadEvent = EPOLLIN | EPOLLPRI;
    static inline const int kWriteEvent = EPOLLOUT;

    EventLoop* loop_;
    const int fd_;
    int events_;
    int revents_;
    int index_;
    
    std::weak_ptr<void> tie_;
    bool tied_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};