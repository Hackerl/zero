#ifndef ZERO_BASE64_H
#define ZERO_BASE64_H

#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace zero::encoding::base64 {
    std::string encode(const std::byte *buffer, size_t length);
    std::optional<std::vector<std::byte>> decode(std::string_view encoded);
}

#endif //ZERO_BASE64_H
