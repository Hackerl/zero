#ifndef ZERO_HEX_H
#define ZERO_HEX_H

#include <string>
#include <vector>
#include <optional>

namespace zero::encoding::hex {
    std::string encode(const unsigned char *buffer, size_t length);
    std::optional<std::vector<unsigned char>> decode(const std::string &encoded);
}

#endif //ZERO_HEX_H
