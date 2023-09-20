#ifndef ZERO_HEX_H
#define ZERO_HEX_H

#include <string>
#include <vector>
#include <cstddef>
#include <system_error>
#include <tl/expected.hpp>
#include <nonstd/span.hpp>

namespace zero::encoding::hex {
    std::string encode(nonstd::span<const std::byte> buffer);
    tl::expected<std::vector<std::byte>, std::error_code> decode(std::string_view encoded);
}

#endif //ZERO_HEX_H
