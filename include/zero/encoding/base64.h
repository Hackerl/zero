#ifndef ZERO_BASE64_H
#define ZERO_BASE64_H

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <nonstd/span.hpp>

namespace zero::encoding::base64 {
    std::string encode(nonstd::span<std::byte> buffer);
    std::optional<std::vector<std::byte>> decode(std::string_view encoded);
}

#endif //ZERO_BASE64_H
