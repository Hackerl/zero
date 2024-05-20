#ifndef ZERO_DARWIN_ERROR_H
#define ZERO_DARWIN_ERROR_H

#include <optional>
#include <zero/error.h>
#include <mach/mach.h>

namespace zero::os::darwin {
    DEFINE_ERROR_TRANSFORMER_EX(
        Error,
        "zero::os::darwin",
        mach_error_string,
        [](const int value) -> std::optional<std::error_condition> {
            std::optional<std::error_condition> condition;

            switch (value) {
            case KERN_INVALID_ARGUMENT:
                condition = std::errc::invalid_argument;
                break;

            case KERN_OPERATION_TIMED_OUT:
                condition = std::errc::timed_out;
                break;

            default:
                break;
            }

            return condition;
        }
    )
}

DECLARE_ERROR_CODE(zero::os::darwin::Error)

#endif //ZERO_DARWIN_ERROR_H
