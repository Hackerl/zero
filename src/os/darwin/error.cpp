#include <zero/os/darwin/error.h>
#include <mach/mach.h>

const char *zero::os::darwin::ErrorCategory::name() const noexcept {
    return "zero::os::darwin";
}

std::string zero::os::darwin::ErrorCategory::message(const int value) const {
    return mach_error_string(value);
}

std::error_condition zero::os::darwin::ErrorCategory::default_error_condition(const int value) const noexcept {
    std::error_condition condition;

    switch (value) {
    case KERN_INVALID_ARGUMENT:
        condition = std::errc::invalid_argument;
        break;

    case KERN_OPERATION_TIMED_OUT:
        condition = std::errc::timed_out;
        break;

    default:
        condition = error_category::default_error_condition(value);
        break;
    }

    return condition;
}

const std::error_category &zero::os::darwin::errorCategory() {
    static ErrorCategory instance;
    return instance;
}

std::error_code zero::os::darwin::make_error_code(const Error e) {
    return {static_cast<int>(e), errorCategory()};
}
