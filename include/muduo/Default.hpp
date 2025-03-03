#pragma once

#include <stdlib.h>

#include "Poller.hpp"
#include "EpollPoller.hpp"

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; 
    }
    else
    {
        return new EPollPoller(loop); 
    }
}