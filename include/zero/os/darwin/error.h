#ifndef ZERO_DARWIN_ERROR_H
#define ZERO_DARWIN_ERROR_H

#include <system_error>

namespace zero::os::darwin {
    enum class Error {
    };

    class ErrorCategory final : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
        [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override;
    };

    std::error_code make_error_code(Error e);
}

#endif //ZERO_DARWIN_ERROR_H
