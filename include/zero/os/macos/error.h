#ifndef ZERO_MACOS_ERROR_H
#define ZERO_MACOS_ERROR_H

#include <optional>
#include <zero/error.h>
#include <mach/mach.h>

namespace zero::os::macos {
    DEFINE_ERROR_TRANSFORMER_EX(
        Error,
        "zero::os::macos",
        mach_error_string,
        [](const int value) -> std::optional<std::error_condition> {
            switch (value) {
            case KERN_INVALID_ARGUMENT:
                return std::errc::invalid_argument;

            case KERN_OPERATION_TIMED_OUT:
                return std::errc::timed_out;

            default:
                return std::nullopt;
            }
        }
    )
}

DECLARE_ERROR_CODE(zero::os::macos::Error)

#endif //ZERO_MACOS_ERROR_H
