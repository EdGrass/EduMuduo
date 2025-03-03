#pragma once

#include <functional>
#include <utility>

#include "Channel.hpp"
#include "EventLoop.hpp"
#include "InetAddress.hpp"
#include "Logger.hpp"
#include "Socket.hpp"
#include "Noncopyable.hpp"

class Acceptor : Noncopyable {
public:
    using NewConnectionCallback = std::function<void(int, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor() noexcept;

    void setNewConnectionCallback(NewConnectionCallback cb) noexcept;
    [[nodiscard]] bool listenning() const noexcept;
    void listen();

private:
    static int createNonblocking();
    void handleRead();
    void handleAcceptError(int err);

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback NewConnectionCallback_;
    bool listenning_;

    std::function<void()> highWaterMarkCallback_;
};