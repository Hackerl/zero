#ifndef ZERO_BASE64_H
#define ZERO_BASE64_H

#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <system_error>
#include <tl/expected.hpp>

namespace zero::encoding::base64 {
    std::string encode(std::span<const std::byte> buffer);
    tl::expected<std::vector<std::byte>, std::error_code> decode(std::string_view encoded);
}

#endif //ZERO_BASE64_H
