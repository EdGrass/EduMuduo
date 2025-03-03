#include <sys/socket.h>
#include <cerrno>
#include <unistd.h>

#include <muduo/Acceptor.hpp>

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false) {
    acceptSocket_.setReuseAddr(Socket::ENABLE);
    acceptSocket_.setReusePort(reuseport ? Socket::ENABLE : Socket::DISABLE);
    acceptSocket_.bindAddress(listenAddr);

    acceptChannel_.setReadCallback([this](Timestamp) { handleRead(); });
}

Acceptor::~Acceptor() noexcept {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb) noexcept {
    NewConnectionCallback_ = std::move(cb);
}

bool Acceptor::listenning() const noexcept { 
    return listenning_; 
}

void Acceptor::listen() {
    loop_->isInLoopThread();
    listenning_ = true;
    
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

int Acceptor::createNonblocking() {
    constexpr int socktype = SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC;
    int sockfd = ::socket(AF_INET, socktype, IPPROTO_TCP);
    if (sockfd < 0) {
        char buf[64];
		strerror_r(errno, buf, sizeof(buf));
		LOG_FATAL("Create acceptor socket error: {}", buf);
    }
    return sockfd;
}

void Acceptor::handleRead() {
    loop_->isInLoopThread();
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    
    if (connfd >= 0) {
        if (NewConnectionCallback_) {
            NewConnectionCallback_(connfd, peerAddr);
        } else {
            ::close(connfd);
            LOG_DEBUG("No connection callback set, closing fd: %d", connfd);
        }
    } else {
        handleAcceptError(errno);
    }
}

void Acceptor::handleAcceptError(int err) {
    constexpr int kEMFILE = EMFILE;
    constexpr int kENFILE = ENFILE;
    
   char buf[64];
    switch (err) {
    case kEMFILE:
    case kENFILE:
		strerror_r(errno, buf, sizeof(buf));
		LOG_FATAL("Create acceptor socket error: {}", buf);
        if (highWaterMarkCallback_) {
            highWaterMarkCallback_();
        }
        break;
    default:
		strerror_r(errno, buf, sizeof(buf));
		LOG_FATAL("Create acceptor socket error: {}", buf);
    }
}