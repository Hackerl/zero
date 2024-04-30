#include <zero/os/procfs/procfs.h>
#include <zero/singleton.h>

const char *zero::os::procfs::ErrorCategory::name() const noexcept {
    return "zero::os::procfs";
}

std::string zero::os::procfs::ErrorCategory::message(const int value) const {
    if (static_cast<Error>(value) == Error::UNEXPECTED_DATA)
        return "unexpected data";

    return "unknown";
}

std::error_code zero::os::procfs::make_error_code(const Error e) {
    return {static_cast<int>(e), Singleton<ErrorCategory>::getInstance()};
}
