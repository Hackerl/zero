#ifndef ZERO_PROCFS_H
#define ZERO_PROCFS_H

#include <system_error>
#include <tl/expected.hpp>

namespace zero::os::procfs {
    enum Error {
        NO_SUCH_IMAGE = 1,
        NO_SUCH_MEMORY_MAPPING,
        UNEXPECTED_DATA,
        MAYBE_ZOMBIE_PROCESS
    };

    class ErrorCategory final : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
    };

    const std::error_category &errorCategory();
    std::error_code make_error_code(Error e);
}

template<>
struct std::is_error_code_enum<zero::os::procfs::Error> : std::true_type {

};

#endif //ZERO_PROCFS_H
