#include <algorithm>
#include <sys/uio.h>
#include <unistd.h>
#include <cerrno>

#include <muduo/Buffer.hpp>
#include <muduo/Logger.hpp>

Buffer::Buffer(size_t initialSize) noexcept
    : buffer_(kCheapPrepend + initialSize),
      readerIndex_(kCheapPrepend),
      writerIndex_(kCheapPrepend) {}

void Buffer::retrieve(size_t len) noexcept {
    len = std::min(len, readableBytes());
    readerIndex_ += len;
    if (readerIndex_ == writerIndex_) resetIndices();
}

void Buffer::retrieveAll() noexcept { 
    resetIndices(); 
}

std::string Buffer::retrieveAsString(size_t len) {
    len = std::min(len, readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

void Buffer::append(const char* data, size_t len) {
    ensureWritableSpace(len);
    std::copy_n(data, len, beginWrite());
    writerIndex_ += len;
}

ssize_t Buffer::readFd(int fd, std::error_code& ec) noexcept {
    struct iovec vec[2];
    const size_t writable = writableBytes();
    
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf_.data();
    vec[1].iov_len = extrabuf_.size();

    const int iovcnt = (writable < extrabuf_.size()) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    
    if (n < 0) {
        ec.assign(errno, std::system_category());
        return -1;
    }

    handleReadResult(static_cast<size_t>(n), writable);
    return n;
}

ssize_t Buffer::writeFd(int fd, std::error_code& ec) noexcept {
    const size_t readable = readableBytes();
    const ssize_t n = ::write(fd, peek(), readable);
    if (n < 0) {
        ec.assign(errno, std::system_category());
        return -1;
    }
    readerIndex_ += static_cast<size_t>(n);  
    return n;
}

void Buffer::ensureWritableSpace(size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);
    }
}

void Buffer::makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        buffer_.resize(writerIndex_ + len);
    } else {
        const size_t readable = readableBytes();
        std::copy(begin() + readerIndex_, begin() + writerIndex_, 
                  begin() + kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;
    }
}

void Buffer::handleReadResult(size_t n, size_t writable) noexcept {
    if (n <= writable) {
        writerIndex_ += n;
    } else {
        writerIndex_ = buffer_.size();
        append(extrabuf_.data(), n - writable);
    }
}