#pragma once

#include <vector>
#include <unordered_map>
#include "Noncopyable.hpp"
#include "Timestamp.hpp"
#include "EventLoop.hpp"

class Channel;

class Poller : Noncopyable {
public:
    using ChannelList = std::vector<Channel*>;

    explicit Poller(EventLoop* loop);
    virtual ~Poller() = default;

    bool hasChannel(Channel* channel) const;
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_;
};