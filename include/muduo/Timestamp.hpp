#pragma once

#include <time.h>
#include <sys/time.h>
#include <chrono>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>

/*
 * Implements a Timestamp class for retrieving the current time  
 * and generating formatted output (e.g. "YYYY-MM-DD HH:mm:ss.ms").  
 */  

class Timestamp;

class Timestamp {
public:
    Timestamp() : mseconds(0) {}
    explicit Timestamp(int64_t _time) : mseconds(_time) {}

    static Timestamp now() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        int64_t microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        return Timestamp(microseconds);
    }

    std::string toString() const {
        time_t seconds = static_cast<time_t>(mseconds / 1000000);
        int microseconds = static_cast<int>(mseconds % 1000000);
        struct tm _time;

        localtime_r(&seconds, &_time);  

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << _time.tm_year + 1900 << "-"
            << std::setw(2) << _time.tm_mon + 1 << "-"
            << std::setw(2) << _time.tm_mday << " "
            << std::setw(2) << _time.tm_hour << ":"
            << std::setw(2) << _time.tm_min << ":"
            << std::setw(2) << _time.tm_sec << "."
            << std::setw(6) << microseconds;  

        return oss.str();
    }

	~Timestamp() = default;

private:
    int64_t mseconds;  
};
