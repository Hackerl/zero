#include <zero/encoding/hex.h>
#include <zero/strings/strings.h>
#include <array>

constexpr auto HEX_MAP = std::array{
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

std::string zero::encoding::hex::encode(const std::span<const std::byte> buffer) {
    std::string encoded;

    for (const auto &byte: buffer) {
        encoded.push_back(HEX_MAP[std::to_integer<unsigned char>((byte & std::byte{0xf0}) >> 4)]);
        encoded.push_back(HEX_MAP[std::to_integer<unsigned char>(byte & std::byte{0x0f})]);
    }

    return encoded;
}

tl::expected<std::vector<std::byte>, std::error_code> zero::encoding::hex::decode(const std::string_view encoded) {
    if (encoded.length() % 2)
        return tl::unexpected(make_error_code(std::errc::invalid_argument));

    tl::expected<std::vector<std::byte>, std::error_code> result;

    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        auto n = strings::toNumber<unsigned int>(encoded.substr(i, 2), 16);

        if (!n) {
            result = tl::unexpected(n.error());
            break;
        }

        result->push_back(static_cast<std::byte>(*n & 0xff));
    }

    return result;
}
