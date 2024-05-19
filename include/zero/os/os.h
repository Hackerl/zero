#ifndef ZERO_OS_H
#define ZERO_OS_H

#include <string>
#include <expected>
#include <system_error>

namespace zero::os {
    std::expected<std::string, std::error_code> hostname();
    std::expected<std::string, std::error_code> username();
}

#endif //ZERO_OS_H
