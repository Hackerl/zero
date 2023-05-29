#include <zero/time/time.h>
#include <chrono>

std::string zero::time::now() {
    return stringify(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::string zero::time::stringify(std::time_t time) {
    std::tm tm = {};

#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    char buffer[64] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);

    return buffer;
}
