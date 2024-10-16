#include <zero/encoding/hex.h>
#include <zero/strings/strings.h>
#include <cassert>

std::string zero::encoding::hex::encode(const nonstd::span<const std::byte> data) {
    constexpr std::array mapping{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::string encoded;

    for (const auto &byte: data) {
        encoded.push_back(mapping[std::to_integer<unsigned char>((byte & std::byte{0xf0}) >> 4)]);
        encoded.push_back(mapping[std::to_integer<unsigned char>(byte & std::byte{0x0f})]);
    }

    return encoded;
}

tl::expected<std::vector<std::byte>, zero::encoding::hex::DecodeError>
zero::encoding::hex::decode(const std::string_view encoded) {
    if (encoded.length() % 2)
        return tl::unexpected(DecodeError::INVALID_LENGTH);

    std::vector<std::byte> data;

    // waiting for libc++ to implement ranges::views::chunk
    for (std::size_t i{0}; i < encoded.size(); i += 2) {
        const auto n = strings::toNumber<unsigned int>({encoded.data() + i, 2}, 16);

        if (!n) {
            assert(n.error() == std::errc::invalid_argument);
            return tl::unexpected(DecodeError::INVALID_HEX_CHARACTER);
        }

        data.push_back(static_cast<std::byte>(*n & 0xff));
    }

    return data;
}

DEFINE_ERROR_CATEGORY_INSTANCE(zero::encoding::hex::DecodeError)
