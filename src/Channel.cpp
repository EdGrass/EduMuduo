#include <sys/epoll.h>

#include <muduo/Channel.hpp>

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false)
{}

Channel::~Channel() = default;

void Channel::handleEvent(Timestamp receiveTime) {
    Logger::instance().log(LogLevel::Debug, "Channel::handleEvent fd={} events={:#x}", fd_, revents_);
    
    if (tied_) {
        if (auto guard = tie_.lock()) {
            handleEventWithGuard(receiveTime);
        } else {
            Logger::instance().log(LogLevel::Error, "Channel::handleEvent tie expired fd={}", fd_);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::tie(const std::shared_ptr<void>& obj) { 
    tie_ = obj; 
    tied_ = true; 
}

void Channel::enableReading() { 
    events_ |= kReadEvent; 
    update(); 
    Logger::instance().log(LogLevel::Debug, "Channel::enableReading fd={}", fd_);
}

void Channel::disableReading() { 
    events_ &= ~kReadEvent; 
    update(); 
    Logger::instance().log(LogLevel::Debug, "Channel::disableReading fd={}", fd_);
}

void Channel::enableWriting() { 
    events_ |= kWriteEvent; 
    update(); 
    Logger::instance().log(LogLevel::Debug, "Channel::enableWriting fd={}", fd_);
}

void Channel::disableWriting() { 
    events_ &= ~kWriteEvent; 
    update(); 
    Logger::instance().log(LogLevel::Debug, "Channel::disableWriting fd={}", fd_);
}

void Channel::disableAll() { 
    events_ = kNoneEvent; 
    update(); 
    Logger::instance().log(LogLevel::Debug, "Channel::disableAll fd={}", fd_);
}

void Channel::remove() { 
    Logger::instance().log(LogLevel::Debug, "Channel::remove fd={}", fd_);
    loop_->removeChannel(this); 
}

void Channel::update() { 
    loop_->updateChannel(this); 
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    Logger::instance().log(LogLevel::Debug, "Channel::handleEventWithGuard fd={} events={:#x}", fd_, revents_);
    
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        Logger::instance().log(LogLevel::Error, "Channel::handle_event() EPOLLHUP fd={}", fd_);
        if (closeCallback_) closeCallback_();
    }

    if (revents_ & EPOLLERR) {
        Logger::instance().log(LogLevel::Error, "Channel::handle_event() EPOLLERR fd={}", fd_);
        if (errorCallback_) errorCallback_();
    }

    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        Logger::instance().log(LogLevel::Debug, "Channel::handle_event() read event fd={}", fd_);
        if (readCallback_) readCallback_(receiveTime);
    }

    if (revents_ & EPOLLOUT) {
        Logger::instance().log(LogLevel::Debug, "Channel::handle_event() write event fd={}", fd_);
        if (writeCallback_) writeCallback_();
    }
}