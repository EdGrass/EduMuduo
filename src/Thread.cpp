#include <muduo/Thread.hpp>

std::atomic_int Thread::numCreated_(0);

bool Thread::joinable() const {
    return thread_ && thread_->joinable(); 
}