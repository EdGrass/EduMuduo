#include <string>

#include <muduo/Logger.hpp>
#include <muduo/TcpServer.hpp>

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        server_.setThreadNum(3);
    }
    void start(){
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr &conn) {
        if (conn->connected()) {
            LOG_DEBUG("Connection UP : {}", conn->peerAddress().toIpPort());
        } else {
            LOG_DEBUG("Connection DOWN : {}", conn->peerAddress().toIpPort());
        }
    }

    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main() {
    EventLoop loop;
    InetAddress addr(8002);
    EchoServer server(&loop, addr, "EchoServer01");
    server.start();
    loop.loop();
    return 0;
}