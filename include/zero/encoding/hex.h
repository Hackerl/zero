#ifndef ZERO_HEX_H
#define ZERO_HEX_H

#include <string>
#include <vector>

namespace zero {
    namespace encoding {
        namespace hex {
            std::string encode(const unsigned char *buffer, size_t length);
            std::vector<unsigned char> decode(const std::string &encoded);
        }
    }
}

#endif //ZERO_HEX_H
