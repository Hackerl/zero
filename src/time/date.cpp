#include <zero/time/date.h>
#include <ctime>

std::string zero::time::getTimeString() {
    tm m = {};
    time_t t = {};

    std::time(&t);

#ifdef _WIN32
    localtime_s(&m, &t);
#else
    localtime_r(&t, &m);
#endif

    char buffer[64] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &m);

    return buffer;
}
