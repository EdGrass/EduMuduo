#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread{
    inline thread_local int t_cachedTid = 0;

    inline constexpr const char* kTidEnvName = "THREAD_DEBUG";
	
    inline void cacheTid() noexcept {
        if (t_cachedTid == 0) [[unlikely]] {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
    [[nodiscard]] inline int tid() noexcept {
        if (t_cachedTid == 0) [[unlikely]] {
            cacheTid();
        }
        return t_cachedTid;
    }
}