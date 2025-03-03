#pragma once

#include <vector>
#include <string>
#include <system_error>
#include <array>

class Buffer {
public:
    static constexpr size_t kCheapPrepend = 8;
    static constexpr size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize) noexcept;
    
    [[nodiscard]] size_t readableBytes() const noexcept { return writerIndex_ - readerIndex_; }
    [[nodiscard]] size_t writableBytes() const noexcept { return buffer_.size() - writerIndex_; }
    [[nodiscard]] size_t prependableBytes() const noexcept { return readerIndex_; }

    [[nodiscard]] const char* peek() const noexcept { return begin() + readerIndex_; }
    
    void retrieve(size_t len) noexcept;
    void retrieveAll() noexcept;
    [[nodiscard]] std::string retrieveAsString(size_t len);
    [[nodiscard]] std::string retrieveAllAsString();
    void append(const char* data, size_t len);

    [[nodiscard]] ssize_t readFd(int fd, std::error_code& ec) noexcept;
    [[nodiscard]] ssize_t writeFd(int fd, std::error_code& ec) noexcept;

private:
    static constexpr size_t kExtraBufSize = 65536;
    alignas(64) std::array<char, kExtraBufSize> extrabuf_;

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

    [[nodiscard]] char* begin() noexcept { return buffer_.data(); }
    [[nodiscard]] const char* begin() const noexcept { return buffer_.data(); }

    [[nodiscard]] char* beginWrite() noexcept { return begin() + writerIndex_; }
    [[nodiscard]] const char* beginWrite() const noexcept { return begin() + writerIndex_; }

    void resetIndices() noexcept {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    void ensureWritableSpace(size_t len);
    void makeSpace(size_t len);
    void handleReadResult(size_t n, size_t writable) noexcept;
};