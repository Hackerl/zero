#include <zero/time/time.h>

std::string zero::time::now() {
    return stringify(std::chrono::system_clock::now());
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

std::string zero::time::stringify(std::chrono::time_point<std::chrono::system_clock> time) {
    return stringify(std::chrono::system_clock::to_time_t(time));
}
