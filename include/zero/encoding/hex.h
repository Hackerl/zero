#ifndef ZERO_HEX_H
#define ZERO_HEX_H

#include <span>
#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace zero::encoding::hex {
    std::string encode(std::span<const std::byte> buffer);
    std::optional<std::vector<std::byte>> decode(std::string_view encoded);
}

#endif //ZERO_HEX_H
