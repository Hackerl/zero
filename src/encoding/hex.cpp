#include <zero/encoding/hex.h>
#include <zero/strings/strings.h>

constexpr char HEX_MAP[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

std::string zero::encoding::hex::encode(const unsigned char *buffer, size_t length) {
    std::string encoded;

    for (size_t i = 0; i < length; i++) {
        encoded.push_back(HEX_MAP[(buffer[i] & 0xf0) >> 4]);
        encoded.push_back(HEX_MAP[buffer[i] & 0x0f]);
    }

    return encoded;
}

std::optional<std::vector<unsigned char>> zero::encoding::hex::decode(const std::string &encoded) {
    if (encoded.length() % 2)
        return std::nullopt;

    std::vector<unsigned char> buffer;

    for (size_t i = 0; i < encoded.size(); i += 2) {
        std::optional<unsigned int> number = zero::strings::toNumber<unsigned int>(encoded.substr(i, 2), 16);

        if (!number)
            return std::nullopt;

        buffer.push_back(*number & 0xff);
    }

    return buffer;
}
