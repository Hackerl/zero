#ifndef ZERO_OS_H
#define ZERO_OS_H

#include <zero/error.h>
#include <tl/expected.hpp>

namespace zero::os {
    tl::expected<std::string, std::error_code> hostname();

#ifndef _WIN32
    DEFINE_ERROR_CODE(
        GetUsernameError,
        "zero::os::username",
        NO_SUCH_ENTRY, "no such entry"
    )
#endif

    tl::expected<std::string, std::error_code> username();
}

#ifndef _WIN32
DECLARE_ERROR_CODE(zero::os::GetUsernameError)
#endif

#endif //ZERO_OS_H
