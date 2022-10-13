#ifndef ZERO_HEX_H
#define ZERO_HEX_H

#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace zero::encoding::hex {
    std::string encode(const std::byte *buffer, size_t length);
    std::optional<std::vector<std::byte>> decode(std::string_view encoded);
}

#endif //ZERO_HEX_H
