#include <zero/time/date.h>
#include <chrono>

std::string zero::time::getTimeString() {
    std::tm tm = {};
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif

    char buffer[64] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);

    return buffer;
}
