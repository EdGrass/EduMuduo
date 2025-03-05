#pragma once

class Noncopyable{
protected:
    Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;

    Noncopyable(Noncopyable&&) = default;
    Noncopyable& operator=(Noncopyable&&) = default;

    ~Noncopyable() = default; 
	
};

/*
Why are non-copyable classes needed?

1. **Network programming**: Numerous objects hold non-copyable resources (e.g., socket descriptors, file handles, memory buffers).  
2. **Race conditions**: In multi-threaded environments, copy operations could compromise the object's internal state.  
3. **Container storage**: Non-copyable objects can be stored in containers via smart pointers, e.g., `std::vector<std::shared_ptr<T>>`.
*/