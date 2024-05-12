#ifndef ZERO_DARWIN_ERROR_H
#define ZERO_DARWIN_ERROR_H

#include <zero/error.h>
#include <mach/mach.h>

namespace zero::os::darwin {
    DEFINE_ERROR_TRANSFORMER_EX(
        Error,
        "zero::os::darwin",
        mach_error_string,
        KERN_INVALID_ARGUMENT, std::errc::invalid_argument,
        KERN_OPERATION_TIMED_OUT, std::errc::timed_out
    )
}

DECLARE_ERROR_CODE(zero::os::darwin::Error)

#endif //ZERO_DARWIN_ERROR_H
