#ifndef ZERO_OS_H
#define ZERO_OS_H

#include "resource.h"
#include <zero/error.h>

namespace zero::os {
    std::string hostname();

#ifndef _WIN32
    Z_DEFINE_ERROR_CODE(
        GetUsernameError,
        "zero::os::username",
        NoSuchEntry, "No such entry"
    )
#endif

    std::expected<std::string, std::error_code> username();

    // Anonymous pipes on Windows do not support overlapped, so named pipes are used to simulate it.
    std::pair<IOResource, IOResource> pipe();
}

#ifndef _WIN32
Z_DECLARE_ERROR_CODE(zero::os::GetUsernameError)
#endif

#endif //ZERO_OS_H
