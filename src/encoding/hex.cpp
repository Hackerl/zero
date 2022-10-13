#include <zero/encoding/hex.h>
#include <zero/strings/strings.h>
#include <array>

constexpr auto HEX_MAP = std::array{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

std::string zero::encoding::hex::encode(const std::byte *buffer, size_t length) {
    std::string encoded;

    for (size_t i = 0; i < length; i++) {
        encoded.push_back(HEX_MAP[std::to_integer<unsigned char>((buffer[i] & std::byte{0xf0}) >> 4)]);
        encoded.push_back(HEX_MAP[std::to_integer<unsigned char>(buffer[i] & std::byte{0x0f})]);
    }

    return encoded;
}

std::optional<std::vector<std::byte>> zero::encoding::hex::decode(std::string_view encoded) {
    if (encoded.length() % 2)
        return std::nullopt;

    std::vector<std::byte> buffer;

    for (size_t i = 0; i < encoded.size(); i += 2) {
        std::optional<unsigned int> number = zero::strings::toNumber<unsigned int>(encoded.substr(i, 2), 16);

        if (!number)
            return std::nullopt;

        buffer.push_back(std::byte(*number & 0xff));
    }

    return buffer;
}
