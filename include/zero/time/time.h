#ifndef ZERO_DATE_H
#define ZERO_DATE_H

#include <string>
#include <ctime>

namespace zero::time {
    std::string now();
    std::string stringify(std::time_t time);
}

#endif //ZERO_DATE_H
