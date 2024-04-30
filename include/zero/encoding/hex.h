#ifndef ZERO_HEX_H
#define ZERO_HEX_H

#include <string>
#include <vector>
#include <cstddef>
#include <system_error>
#include <tl/expected.hpp>
#include <nonstd/span.hpp>

namespace zero::encoding::hex {
    enum class DecodeError {
        INVALID_LENGTH = 1,
        INVALID_HEX_CHARACTER
    };

    class DecodeErrorCategory final : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
        [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override;
    };

    std::error_code make_error_code(DecodeError e);

    std::string encode(nonstd::span<const std::byte> buffer);
    tl::expected<std::vector<std::byte>, DecodeError> decode(std::string_view encoded);
}

template<>
struct std::is_error_code_enum<zero::encoding::hex::DecodeError> : std::true_type {
};

#endif //ZERO_HEX_H
