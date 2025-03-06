#pragma once

#include <netinet/in.h>
#include <string>
#include <string_view>
#include <system_error>

/*  
 * Encapsulates the IPv4 address structure  
 */  

class InetAddress {
public:
    explicit InetAddress(uint16_t port = 0, std::string_view ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in& addr) noexcept;
    
    [[nodiscard]] std::string toIp() const;
    [[nodiscard]] uint16_t toPort() const noexcept;
    [[nodiscard]] std::string toIpPort() const;
    
    [[nodiscard]] const sockaddr_in* getSockAddr() const noexcept { return &addr_; }
    void setSockAddr(const sockaddr_in& addr) noexcept { addr_ = addr; }

private:
    sockaddr_in addr_{};
};