#include <zero/os/procfs/procfs.h>

const char *zero::os::procfs::ErrorCategory::name() const noexcept {
    return "zero::os::procfs";
}

std::string zero::os::procfs::ErrorCategory::message(const int value) const {
    std::string msg;

    switch (value) {
    case UNEXPECTED_DATA:
        msg = "unexpected data";
        break;

    case MAYBE_ZOMBIE_PROCESS:
        msg = "maybe zombie process";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

const std::error_category &zero::os::procfs::errorCategory() {
    static ErrorCategory instance;
    return instance;
}

std::error_code zero::os::procfs::make_error_code(const Error e) {
    return {static_cast<int>(e), errorCategory()};
}
