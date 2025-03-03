#pragma once

#include <sys/epoll.h>
#include <vector>
#include <string>

#include "Poller.hpp"
#include "Channel.hpp"
#include "Logger.hpp"

class EPollPoller : public Poller {
public:
    static constexpr int kInitEventListSize = 16;
    static constexpr int KNew = -1;
    static constexpr int KAdded = 1;
    static constexpr int KDeleted = 2;

    explicit EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    using EventList = std::vector<struct epoll_event>;

    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    void updateOperation(int operation, Channel* channel);
    
    static const char* operationToString(int op);
    static const char* channelStateToString(int state);

    int epollfd_;
    EventList events_;
};