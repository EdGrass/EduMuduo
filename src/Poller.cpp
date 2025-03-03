#include <muduo/Poller.hpp>
#include <muduo/Channel.hpp>
#include <muduo/Logger.hpp>
#include <muduo/EpollPoller.hpp>

Poller::Poller(EventLoop* loop) : ownerLoop_(loop) {}

bool Poller::hasChannel(Channel* channel) const {
    auto it = channels_.find(channel->fd());
    const bool exists = (it != channels_.end() && it->second == channel);
    
    Logger::instance().log(LogLevel::Debug,
        "[Poller] Check channel existence - FD: {}, Result: {}",
        channel->fd(), 
        exists ? "Exists" : "Not Found");
    
    return exists;
}

Poller* Poller::newDefaultPoller(EventLoop* loop) {
    return new EPollPoller(loop);
}