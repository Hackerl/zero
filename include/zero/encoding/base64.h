#ifndef ZERO_BASE64_H
#define ZERO_BASE64_H

#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <system_error>
#include <tl/expected.hpp>

namespace zero::encoding::base64 {
    enum DecodeError {
        INVALID_LENGTH = 1
    };

    class DecodeErrorCategory final : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
        [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override;
    };

    std::error_code make_error_code(DecodeError e);

    std::string encode(std::span<const std::byte> buffer);
    tl::expected<std::vector<std::byte>, DecodeError> decode(std::string_view encoded);
}

template<>
struct std::is_error_code_enum<zero::encoding::base64::DecodeError> : std::true_type {
};

#endif //ZERO_BASE64_H
