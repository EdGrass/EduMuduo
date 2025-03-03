#pragma once

#include <unistd.h>
#include <sys/socket.h>
#include <system_error>
#include <netinet/tcp.h>
#include <memory>
#include <utility>

#include "Noncopyable.hpp"
#include "Logger.hpp"
#include "InetAddress.hpp"

class Socket : Noncopyable
{
public:
    enum OptionState : int { DISABLE = 0, ENABLE = 1 };

    explicit Socket(int sockfd) noexcept : sockfd_(sockfd) {}
    ~Socket() noexcept { if(sockfd_ >= 0) ::close(sockfd_); }

    Socket(Socket&& other) noexcept : sockfd_(other.release()) {}
    Socket& operator=(Socket&& other) noexcept
    {
        reset(other.release());
        return *this;
    }

    [[nodiscard]] int fd() const noexcept { return sockfd_; }

    void bindAddress(const InetAddress& localaddr)
    {
        const auto* addr = localaddr.getSockAddr();
        if (::bind(sockfd_, 
                  reinterpret_cast<const sockaddr*>(addr),
                  sizeof(sockaddr_in)) != 0)
        {
            LOG_FATAL("Bind failed on fd: %d", sockfd_);
        }
    }

    void listen()
    {
        constexpr int kBacklog = 1024;
        if (::listen(sockfd_, kBacklog) != 0)
        {
            LOG_FATAL("Listen failed on fd: %d", sockfd_);
        }
    }

    int accept(InetAddress* peeraddr) noexcept
    {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        
        const int connfd = ::accept4(sockfd_,
                                    reinterpret_cast<sockaddr*>(&addr),
                                    &len,
                                    SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connfd >= 0)
        {
            peeraddr->setSockAddr(addr);
        }
        return connfd;
    }

    void shutdownWrite() noexcept
    {
        if (::shutdown(sockfd_, SHUT_WR) < 0)
        {
            LOG_ERROR("Shutdown write error on fd: %d", sockfd_);
        }
    }

    template<int Level, int OptName>
    void setOption(OptionState state) noexcept
    {
        const int optval = static_cast<int>(state);
        if (::setsockopt(sockfd_, Level, OptName, &optval, sizeof(optval)) < 0)
        {
            LOG_ERROR("Set socket option %d failed on fd: %d", OptName, sockfd_);
        }
    }

    void setTcpNoDelay(OptionState on) noexcept
    {
        setOption<IPPROTO_TCP, TCP_NODELAY>(on);
    }

    void setReuseAddr(OptionState on) noexcept
    {
        setOption<SOL_SOCKET, SO_REUSEADDR>(on);
    }

    void setReusePort(OptionState on) noexcept
    {
        setOption<SOL_SOCKET, SO_REUSEPORT>(on);
    }

    void setKeepAlive(OptionState on) noexcept
    {
        setOption<SOL_SOCKET, SO_KEEPALIVE>(on);
    }

	std::error_code getSocketError() const {
	    int error = 0;
	    socklen_t len = sizeof(error);
	    if (getsockopt(fd(), SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
	        return std::error_code(errno, std::system_category());
	    }
	    return std::error_code(error, std::system_category());
	}

private:
    int release() noexcept { return std::exchange(sockfd_, -1); }
    void reset(int fd = -1) noexcept
    {
        if(sockfd_ >= 0) ::close(sockfd_);
        sockfd_ = fd;
    }

    int sockfd_;
};