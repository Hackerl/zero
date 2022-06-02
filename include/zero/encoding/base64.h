#ifndef ZERO_BASE64_H
#define ZERO_BASE64_H

#include <string>
#include <vector>
#include <optional>

namespace zero::encoding::base64 {
    std::string encode(const unsigned char *buffer, size_t length);
    std::optional<std::vector<unsigned char>> decode(const std::string &encoded);
}

#endif //ZERO_BASE64_H
