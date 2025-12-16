#ifndef ZERO_ENCODING_BASE64_H
#define ZERO_ENCODING_BASE64_H

#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <zero/error.h>

namespace zero::encoding::base64 {
    Z_DEFINE_ERROR_CODE_EX(
        DecodeError,
        "zero::encoding::base64::decode",
        INVALID_LENGTH, "Invalid length for a base64 string", std::errc::invalid_argument
    )

    std::string encode(std::span<const std::byte> data);
    std::expected<std::vector<std::byte>, DecodeError> decode(std::string_view encoded);
}

Z_DECLARE_ERROR_CODE(zero::encoding::base64::DecodeError)

#endif //ZERO_ENCODING_BASE64_H
