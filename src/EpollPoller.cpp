#include <unistd.h>
#include <cerrno>
#include <cstring>

#include <muduo/EpollPoller.hpp>

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) 
{
    if (epollfd_ < 0) {
        LOG_FATAL("[EPollPoller] epoll_create1 failed - error: {} ({})",
            errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    LOG_DEBUG("[EPollPoller] Created - FD: {}", epollfd_);
}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
    LOG_DEBUG("[EPollPoller] Destroyed - FD: {}", epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    LOG_DEBUG("[EPollPoller] Polling with timeout {}ms (monitoring {} channels)",
        timeoutMs, channels_.size());

    const int numEvents = ::epoll_wait(epollfd_, events_.data(), 
                              static_cast<int>(events_.size()), timeoutMs);
    const int saveErrno = errno;
    const auto now = Timestamp::now();

    if (numEvents > 0) {
        LOG_DEBUG("[EPollPoller] {} events triggered", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        
        if (static_cast<size_t>(numEvents) == events_.size()) {
            events_.resize(events_.size() * 2);
            LOG_DEBUG("[EPollPoller] Expanded event list to {} capacity", events_.size());
        }
    } else if (numEvents == 0) {
        LOG_DEBUG("[EPollPoller] Poll timeout reached");
    } else {
        if (saveErrno != EINTR) {
            char errorBuf[512];
            strerror_r(saveErrno, errorBuf, sizeof(errorBuf));
            LOG_ERROR("[EPollPoller] epoll_wait error: {} ({})", saveErrno, errorBuf);
        }
    }
    return now;
}

void EPollPoller::updateChannel(Channel* channel) {
    const int fd = channel->fd();
    const int index = channel->index();
    
    LOG_DEBUG("[EPollPoller] Updating channel - FD: {}, Events: {:#x}, State: {}",
        fd, channel->events(), channelStateToString(index));

    if (index == KNew || index == KDeleted) {
        if (index == KNew) {
            channels_[fd] = channel;
        }
        channel->set_index(KAdded);
        updateOperation(EPOLL_CTL_ADD, channel);
    } else {
        if (channel->isNoneEvent()) {
            updateOperation(EPOLL_CTL_DEL, channel);
            channel->set_index(KDeleted);
        } else {
            updateOperation(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel* channel) {
    const int fd = channel->fd();
    LOG_DEBUG("[EPollPoller] Removing channel - FD: {}", fd);

    channels_.erase(fd);
    
    if (channel->index() == KAdded) {
        updateOperation(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(KNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
        auto* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->emplace_back(channel);
        
        LOG_DEBUG("[EPollPoller] Activated channel - FD: {}, Events: {:#x}",
            channel->fd(), events_[i].events);
    }
}

void EPollPoller::updateOperation(int operation, Channel* channel) {
    struct epoll_event event;
    ::memset(&event, 0, sizeof(event));
    
    const int fd = channel->fd();
    event.events = channel->events();
    event.data.ptr = channel;

	uint32_t events = event.events;

    LOG_DEBUG("[EPollPoller] epoll_ctl OP: {}, FD: {}, Events: {:#x}",
        operationToString(operation), fd, events);

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        char errorBuf[512];
        strerror_r(errno, errorBuf, sizeof(errorBuf));
        
        LOG_ERROR("[EPollPoller] epoll_ctl failed - OP: {}, FD: {}, Error: {} ({})",
            operationToString(operation), fd, errno, errorBuf);

        if (operation == EPOLL_CTL_DEL) {
            LOG_DEBUG("[EPollPoller] Attempted to delete non-existent FD: {}", fd);
        } else {
            LOG_FATAL("[EPollPoller] Critical epoll_ctl error on FD: {}", fd);
            exit(EXIT_FAILURE);
        }
    }
}

const char* EPollPoller::operationToString(int op) {
    switch (op) {
        case EPOLL_CTL_ADD: return "ADD";
        case EPOLL_CTL_MOD: return "MOD";
        case EPOLL_CTL_DEL: return "DEL";
        default: return "UNKNOWN_OP";
    }
}

const char* EPollPoller::channelStateToString(int state) {
    switch (state) {
        case KNew: return "New";
        case KAdded: return "Added";
        case KDeleted: return "Deleted";
        default: return "Invalid";
    }
}