#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <system_error>
#include <tuple>

#include "Buffer.hpp"
#include "Callbacks.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "InetAddress.hpp"
#include "Logger.hpp"
#include "Noncopyable.hpp"
#include "Socket.hpp"
#include "Timestamp.hpp"

class TcpConnection : Noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    enum class State : uint8_t {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting
    };

    TcpConnection(EventLoop* loop,
                  std::string name,
                  int sockfd,
                  InetAddress localAddr,
                  InetAddress peerAddr)
        : loop_(assertLoopNotNull(loop)),
          name_(std::move(name)),
          state_(State::Connecting),
          socket_(std::make_unique<Socket>(sockfd)),
          channel_(std::make_unique<Channel>(loop, sockfd)),
          localAddr_(std::move(localAddr)),
          peerAddr_(std::move(peerAddr)) {

        configureSocketOptions();
        setupChannelCallbacks();
        LOG_DEBUG("TcpConnection[{}] constructed at fd={}", name_, sockfd);
    }

    ~TcpConnection() {
        LOG_DEBUG("TcpConnection[{}] destroyed fd={} state={}",
                 name_, channel_->fd(), toString(state_));
    }

    EventLoop* getLoop() const noexcept { return loop_; }
    const std::string& name() const noexcept { return name_; }
    bool connected() const noexcept { return state_ == State::Connected; }

    void send(std::string_view data) {
        if (state_.load() != State::Connected) {
            LOG_DEBUG("Attempt to send data on disconnected connection: {}", name_);
            return;
        }

        if (loop_->isInLoopThread()) {
            sendInLoop(data.data(), data.size());
        } else {
            loop_->queueInLoop([this, data = std::string(data)] {
                sendInLoop(data.data(), data.size());
            });
        }
    }

	const InetAddress& peerAddress() const { return peerAddr_; } 

    void shutdown() noexcept {
        if (state_.exchange(State::Disconnecting) == State::Connected) {
            loop_->runInLoop([this] { shutdownInLoop(); });
        }
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

    template<typename F>
    void setCloseCallback(F&& cb) noexcept {
        closeCallback_ = std::forward<F>(cb);
    }

    template<typename F>
    void setHighWaterMarkCallback(F&& cb, size_t mark) noexcept {
        highWaterMarkCallback_ = std::forward<F>(cb);
        highWaterMark_.store(mark, std::memory_order_relaxed);
    }

    void connectEstablished() {
        state_.store(State::Connected);
        channel_->tie(shared_from_this());
        channel_->enableReading();
        if (connectionCallback_) connectionCallback_(shared_from_this());
    }

    void connectDestroyed() {
        if (state_.exchange(State::Disconnected) == State::Connected) {
            channel_->disableAll();
            if (connectionCallback_) connectionCallback_(shared_from_this());
        }
        channel_->remove();
    }

private:
    static EventLoop* assertLoopNotNull(EventLoop* loop) {
        if (!loop) LOG_DEBUG("TcpConnection requires valid EventLoop");
        return loop;
    }

    void configureSocketOptions() noexcept {
        socket_->setTcpNoDelay(Socket::ENABLE);
        socket_->setKeepAlive(Socket::ENABLE);
    }

    void setupChannelCallbacks() noexcept {
        channel_->setReadCallback([this](Timestamp t) { handleRead(t); });
        channel_->setWriteCallback([this] { handleWrite(); });
        channel_->setCloseCallback([this] { handleClose(); });
        channel_->setErrorCallback([this] { handleError(); });
    }

    void sendInLoop(const void* data, size_t len) noexcept {
        loop_->isInLoopThread();
        
        ssize_t nwrote = 0;
        size_t remaining = len;
        bool error = false;

        if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
            nwrote = ::write(channel_->fd(), data, len);
            if (nwrote >= 0) {
                remaining = len - nwrote;
                if (remaining == 0 && writeCompleteCallback_) {
                    loop_->queueInLoop([this] {
                        writeCompleteCallback_(shared_from_this());
                    });
                }
            } else {
                nwrote = 0;
                if (!isBlockingError(errno)) {
                    LOG_ERROR("TcpConnection::sendInLoop");
                    error = true;
                }
            }
        }

        if (!error && remaining > 0) {
            const auto oldLen = outputBuffer_.readableBytes();
            outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining);

            if (oldLen < highWaterMark_ && 
                (oldLen + remaining) >= highWaterMark_ &&
                highWaterMarkCallback_) {
                loop_->queueInLoop([this, total = oldLen + remaining] {
                    highWaterMarkCallback_(shared_from_this(), total);
                });
            }

            if (!channel_->isWriting()) {
                channel_->enableWriting();
            }
        }
    }

    void shutdownInLoop() noexcept {
        loop_->isInLoopThread();
        if (!channel_->isWriting()) {
            socket_->shutdownWrite();
        }
    }

    void handleRead(Timestamp receiveTime) noexcept {
        loop_->isInLoopThread();
        
        std::error_code ec;
        const auto n = inputBuffer_.readFd(channel_->fd(), ec);
        
        if (n > 0) {
            if (messageCallback_) {
                messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
            }
        } else if (n == 0) {
            handleClose();
        } else {
            LOG_ERROR("Read error[{}] on connection {}: {}", 
                      ec.value(), name_, ec.message());
            handleError();
        }
    }

    void handleWrite() noexcept {
        loop_->isInLoopThread();
        
        if (channel_->isWriting()) {
            std::error_code ec;
            const auto n = outputBuffer_.writeFd(channel_->fd(), ec);
            
            if (n > 0) {
                outputBuffer_.retrieve(n);
                if (outputBuffer_.readableBytes() == 0) {
                    channel_->disableWriting();
                    if (writeCompleteCallback_) {
                        loop_->queueInLoop([this] {
                            writeCompleteCallback_(shared_from_this());
                        });
                    }
                    if (state_ == State::Disconnecting) {
                        shutdownInLoop();
                    }
                }
            } else {
                LOG_ERROR("Write error[{}] on connection {}: {}",
                         ec.value(), name_, ec.message());
            }
        }
    }

    void handleClose() noexcept {
        loop_->isInLoopThread();
        state_.store(State::Disconnected);
        channel_->disableAll();

        const auto self = shared_from_this();
        if (connectionCallback_) connectionCallback_(self);
        if (closeCallback_) closeCallback_(self);
    }

    void handleError() noexcept {
        std::error_code ec = socket_->getSocketError();
        LOG_ERROR("Socket error[{}] on connection {}: {}", 
                 ec.value(), name_, ec.message());
    }

    static bool isBlockingError(int err) noexcept {
        return err == EAGAIN || err == EWOULDBLOCK;
    }

    static const char* toString(State s) noexcept {
        switch(s) {
            case State::Disconnected: return "Disconnected";
            case State::Connecting:   return "Connecting";
            case State::Connected:   return "Connected";
            case State::Disconnecting:return "Disconnecting";
            default:                  return "Unknown";
        }
    }

    EventLoop* loop_;
    const std::string name_;
    std::atomic<State> state_;
    
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    std::atomic<size_t> highWaterMark_{64 * 1024 * 1024}; // 64MB
};