#ifndef ZERO_OS_H
#define ZERO_OS_H

#include "resource.h"
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

    // Anonymous pipes on Windows do not support overlapped, so named pipes are used to simulate it.
    std::expected<std::pair<IOResource, IOResource>, std::error_code> pipe();
}

#ifndef _WIN32
DECLARE_ERROR_CODE(zero::os::GetUsernameError)
#endif

#endif //ZERO_OS_H
