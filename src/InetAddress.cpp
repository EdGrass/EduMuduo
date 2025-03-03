#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <array>

#include <muduo/InetAddress.hpp>
#include <muduo/Logger.hpp>

InetAddress::InetAddress(uint16_t port, std::string_view ip) {
    addr_ = {};
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.data(), &addr_.sin_addr) <= 0) {
        const auto err = errno;
        LOG_FATAL("InetAddress construction failed - IP: {}, Port: {}, Errno: {} ({})",
            ip, port, err, strerror(err));
        
        throw std::system_error(
            err,
            std::generic_category(),
            "Invalid address: " + std::string(ip) + ":" + std::to_string(port)
        );
    }
}

InetAddress::InetAddress(const sockaddr_in& addr) noexcept : addr_(addr) {}

std::string InetAddress::toIp() const {
    std::array<char, INET_ADDRSTRLEN> buf{};
    if (const char* ret = inet_ntop(AF_INET, &addr_.sin_addr, buf.data(), buf.size()); 
        ret != nullptr) {
        return buf.data();
    }
    return "";
}

uint16_t InetAddress::toPort() const noexcept {
    return ntohs(addr_.sin_port);
}

std::string InetAddress::toIpPort() const {
    return toIp() + ":" + std::to_string(toPort());
}