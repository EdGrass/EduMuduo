#pragma once

#include <functional> 
#include <thread>     
#include <memory>     
#include <unistd.h>   
#include <string>     
#include <atomic>     
#include <semaphore.h>

#include "Noncopyable.hpp" 
#include "CurrentThread.hpp"

class Thread : public Noncopyable {
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string &name = std::string())
        : started_(false),
          joined_(false),
          tid_(0),
          func_(std::move(func)),
          name_(name) 
    {
        setDefaultName();
    }

    ~Thread() {
        if (started_ && !joined_) {
            if (thread_ && thread_->joinable()) { 
	            thread_->detach();
	        }
        }
    }

	bool joinable() const;

    void start() {
        started_ = true;
        sem_t sem;
        sem_init(&sem, false, 0); 

        thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
            tid_ = CurrentThread::tid();
            sem_post(&sem);             
            func_();                    
        }));

        sem_wait(&sem); 
        sem_destroy(&sem);
    }

    void join() {
        joined_ = true;
        thread_->join();
    }

    bool started() const { return started_; }  
    pid_t tid() const { return tid_; }         
    const std::string& name() const { return name_; } 

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName() {
        int num = ++numCreated_; 
        if (name_.empty()) {
            char buf[32] = {0};
            snprintf(buf, sizeof buf, "Thread%d", num);
            name_ = buf;
        }
    }

    bool started_;          
    bool joined_;           
    std::shared_ptr<std::thread> thread_; 
    pid_t tid_;             
    ThreadFunc func_;       
    std::string name_;      

    static std::atomic_int numCreated_; 
};

