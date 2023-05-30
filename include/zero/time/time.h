#ifndef ZERO_DATE_H
#define ZERO_DATE_H

#include <string>
#include <chrono>

namespace zero::time {
    std::string now();
    std::string stringify(std::time_t time);
    std::string stringify(std::chrono::time_point<std::chrono::system_clock> time);
}

#endif //ZERO_DATE_H
