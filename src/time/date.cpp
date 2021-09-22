#include <time/date.h>
#include <ctime>

std::string zero::time::getTimeString() {
    tm m = {};
    time_t t = {};

    std::time(&t);
    localtime_r(&t, &m);

    char buffer[64] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &m);

    return buffer;
}
