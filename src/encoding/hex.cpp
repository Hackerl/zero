#include <zero/encoding/hex.h>
#include <zero/strings/strings.h>
#include <cassert>

constexpr std::array HEX_MAP = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

std::string zero::encoding::hex::encode(const std::span<const std::byte> data) {
    std::string encoded;

    for (const auto &byte: data) {
        encoded.push_back(HEX_MAP[std::to_integer<unsigned char>((byte & std::byte{0xf0}) >> 4)]);
        encoded.push_back(HEX_MAP[std::to_integer<unsigned char>(byte & std::byte{0x0f})]);
    }

    return encoded;
}

std::expected<std::vector<std::byte>, zero::encoding::hex::DecodeError>
zero::encoding::hex::decode(const std::string_view encoded) {
    if (encoded.length() % 2)
        return std::unexpected(DecodeError::INVALID_LENGTH);

    std::expected<std::vector<std::byte>, DecodeError> result;

    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        auto n = strings::toNumber<unsigned int>(encoded.substr(i, 2), 16);

        if (!n) {
            assert(n.error() == std::errc::invalid_argument);
            result = std::unexpected(DecodeError::INVALID_HEX_CHARACTER);
            break;
        }

        result->push_back(static_cast<std::byte>(*n & 0xff));
    }

    return result;
}
