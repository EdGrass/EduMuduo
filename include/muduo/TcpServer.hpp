#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <sstream>

#include "Acceptor.hpp"
#include "EventLoop.hpp"
#include "EventLoopThreadPool.hpp"
#include "InetAddress.hpp"
#include "Logger.hpp"
#include "TcpConnection.hpp"
#include "Noncopyable.hpp"
#include "Callbacks.hpp"

class TcpServer : Noncopyable {
public:
    enum class Option { kNoReusePort, kReusePort };

    TcpServer(EventLoop* loop,
              const InetAddress& listenAddr,
              std::string name,
              Option option = Option::kNoReusePort)
        : loop_(assertLoopNotNull(loop)),
          ipPort_(listenAddr.toIpPort()),
          name_(std::move(name)),
          acceptor_(std::make_unique<Acceptor>(loop, listenAddr, option == Option::kReusePort)),
          threadPool_(std::make_shared<EventLoopThreadPool>(loop, name_)),
          nextConnId_(1),
          started_(false) {
        
        acceptor_->setNewConnectionCallback([this](int sockfd, const InetAddress& peerAddr) {
            newConnection(sockfd, peerAddr);
        });
    }

    ~TcpServer() {
        for (auto& [name, conn] : connections_) {
            TcpConnectionPtr localConn(std::move(conn));
            localConn->getLoop()->runInLoop(
                [localConn] { localConn->connectDestroyed(); });
        }
    }

    void setThreadNum(size_t numThreads) noexcept {
        threadPool_->setThreadNum(numThreads);
    }

    template<typename F>
    void setThreadInitCallback(F&& cb) noexcept {
        threadInitCallback_ = std::forward<F>(cb);
    }

    template<typename F>
    void setConnectionCallback(F&& cb) noexcept {
        connectionCallback_ = std::forward<F>(cb);
    }

    template<typename F>
    void setMessageCallback(F&& cb) noexcept {
        messageCallback_ = std::forward<F>(cb);
    }

    template<typename F>
    void setWriteCompleteCallback(F&& cb) noexcept {
        writeCompleteCallback_ = std::forward<F>(cb);
    }

    void start() {
        if (!started_.exchange(true)) {
            threadPool_->start(threadInitCallback_);
            loop_->runInLoop([this] { acceptor_->listen(); });
        }
    }

private:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    static EventLoop* assertLoopNotNull(EventLoop* loop) {
        if (!loop) LOG_DEBUG("TcpServer requires valid EventLoop");
        return loop;
    }

    void newConnection(int sockfd, const InetAddress& peerAddr) {
        EventLoop* ioLoop = threadPool_->getNextLoop();
        
        std::ostringstream oss;
        oss << name_ << "-" << ipPort_ << "#" << nextConnId_++;
        const std::string connName = oss.str();

        sockaddr_in local;
        socklen_t addrlen = sizeof(local);
        if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local), &addrlen) != 0) {
            LOG_ERROR("Failed to get local address for fd: {}", sockfd);
            ::close(sockfd);
            return;
        }

        TcpConnectionPtr conn = std::make_shared<TcpConnection>(
            ioLoop, connName, sockfd, InetAddress(local), peerAddr
        );

        connections_.emplace(connName, conn);
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setWriteCompleteCallback(writeCompleteCallback_);
        conn->setCloseCallback([this](const auto& c) { removeConnection(c); });

        ioLoop->runInLoop([conn] { 
            conn->connectEstablished(); 
        });
    }

    void removeConnection(const TcpConnectionPtr& conn) {
        loop_->runInLoop([this, conn] { removeConnectionInLoop(conn); });
    }

    void removeConnectionInLoop(const TcpConnectionPtr& conn) {
        LOG_DEBUG("Removing connection: {}", conn->name());
        
        if (connections_.erase(conn->name()) > 0) {
            EventLoop* ioLoop = conn->getLoop();
            ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
        }
    }

    EventLoop* loop_;
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    
    std::atomic_uint nextConnId_;
    std::atomic_bool started_;
    ConnectionMap connections_;

    std::function<void(EventLoop*)> threadInitCallback_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
};
