#ifndef ZERO_HEX_H
#define ZERO_HEX_H

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <nonstd/span.hpp>

namespace zero::encoding::hex {
    std::string encode(nonstd::span<std::byte> buffer);
    std::optional<std::vector<std::byte>> decode(std::string_view encoded);
}

#endif //ZERO_HEX_H
