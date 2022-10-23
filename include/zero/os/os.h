#ifndef ZERO_OS_H
#define ZERO_OS_H

#include <string>
#include <optional>

namespace zero::os {
    std::optional<std::string> hostname();
    std::optional<std::string> username();
}

#endif //ZERO_OS_H
