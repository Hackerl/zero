#ifndef ZERO_OS_H
#define ZERO_OS_H

#include <string>
#include <system_error>
#include <tl/expected.hpp>

namespace zero::os {
    tl::expected<std::string, std::error_code> hostname();
    tl::expected<std::string, std::error_code> username();
}

#endif //ZERO_OS_H
