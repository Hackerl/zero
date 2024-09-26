#ifndef ZERO_OS_H
#define ZERO_OS_H

#include <expected>
#include <zero/error.h>

namespace zero::os {
    std::expected<std::string, std::error_code> hostname();

#ifndef _WIN32
    DEFINE_ERROR_CODE(
        GetUsernameError,
        "zero::os::username",
        NO_SUCH_ENTRY, "no such entry"
    )
#endif

    std::expected<std::string, std::error_code> username();
}

#ifndef _WIN32
DECLARE_ERROR_CODE(zero::os::GetUsernameError)
#endif

#endif //ZERO_OS_H
