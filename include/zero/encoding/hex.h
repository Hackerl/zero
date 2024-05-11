#ifndef ZERO_HEX_H
#define ZERO_HEX_H

#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <tl/expected.hpp>
#include <zero/error.h>

namespace zero::encoding::hex {
    DEFINE_ERROR_CODE_EX(
        DecodeError,
        "zero::encoding::hex::decode",
        INVALID_LENGTH, "invalid length for a hex string", std::errc::invalid_argument,
        INVALID_HEX_CHARACTER, "invalid hex character", std::errc::invalid_argument
    )

    std::string encode(std::span<const std::byte> data);
    tl::expected<std::vector<std::byte>, DecodeError> decode(std::string_view encoded);
}

DECLARE_ERROR_CODE(zero::encoding::hex::DecodeError)

#endif //ZERO_HEX_H
