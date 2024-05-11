#ifndef ZERO_BASE64_H
#define ZERO_BASE64_H

#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <zero/error.h>
#include <tl/expected.hpp>

namespace zero::encoding::base64 {
    DEFINE_ERROR_CODE_EX(
        DecodeError,
        "zero::encoding::base64::decode",
        INVALID_LENGTH, "invalid length for a base64 string", std::errc::invalid_argument
    )

    std::string encode(std::span<const std::byte> data);
    tl::expected<std::vector<std::byte>, DecodeError> decode(std::string_view encoded);
}

DECLARE_ERROR_CODE(zero::encoding::base64::DecodeError)

#endif //ZERO_BASE64_H
