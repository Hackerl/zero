#ifndef ZERO_BASE64_H
#define ZERO_BASE64_H

#include <string>
#include <vector>

namespace zero {
    namespace encoding {
        namespace base64 {
            std::string encode(const unsigned char *buffer, size_t length);
            std::vector<unsigned char> decode(std::string encoded);
        }
    }
}

#endif //ZERO_BASE64_H
