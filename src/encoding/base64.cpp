#include <zero/encoding/base64.h>

std::string zero::encoding::base64::encode(const std::span<const std::byte> data) {
    constexpr std::string_view mapping{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

    const auto length = data.size();
    const auto missing = (length + 2) / 3 * 3 - length;
    const auto size = (length + missing) * 4 / 3;

    std::string encoded;
    encoded.reserve(size);

    for (std::size_t i{0}; i < length; i += 3) {
        std::array<std::byte, 3> b3{};

        b3[0] = data[i];
        b3[1] = i + 1 < length ? data[i + 1] : std::byte{0};
        b3[2] = i + 2 < length ? data[i + 2] : std::byte{0};

        std::array<std::byte, 4> b4{};

        b4[0] = (b3[0] & std::byte{0xfc}) >> 2;
        b4[1] = (b3[0] & std::byte{0x03}) << 4 | (b3[1] & std::byte{0xf0}) >> 4;
        b4[2] = (b3[1] & std::byte{0x0f}) << 2 | (b3[2] & std::byte{0xc0}) >> 6;
        b4[3] = b3[2] & std::byte{0x3f};

        encoded.push_back(mapping[std::to_integer<unsigned char>(b4[0])]);
        encoded.push_back(mapping[std::to_integer<unsigned char>(b4[1])]);
        encoded.push_back(mapping[std::to_integer<unsigned char>(b4[2])]);
        encoded.push_back(mapping[std::to_integer<unsigned char>(b4[3])]);
    }

    for (std::size_t i{0}; i < missing; ++i)
        encoded[size - i - 1] = '=';

    return encoded;
}

std::expected<std::vector<std::byte>, zero::encoding::base64::DecodeError>
zero::encoding::base64::decode(const std::string_view encoded) {
    if (encoded.length() % 4)
        return std::unexpected{DecodeError::INVALID_LENGTH};

    constexpr std::array mapping{
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 62, 255, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 255, 255, 255,
        255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 63,
        255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255
    };

    const auto size = encoded.size();

    std::vector<std::byte> buffer;
    buffer.reserve(3 * size / 4);

    for (std::size_t i{0}; i < size; i += 4) {
        std::array<std::byte, 4> b4{};

        b4[0] = encoded[i] <= 'z' ? static_cast<std::byte>(mapping[encoded[i]]) : std::byte{0xff};
        b4[1] = encoded[i + 1] <= 'z' ? static_cast<std::byte>(mapping[encoded[i + 1]]) : std::byte{0xff};
        b4[2] = encoded[i + 2] <= 'z' ? static_cast<std::byte>(mapping[encoded[i + 2]]) : std::byte{0xff};
        b4[3] = encoded[i + 3] <= 'z' ? static_cast<std::byte>(mapping[encoded[i + 3]]) : std::byte{0xff};

        std::array<std::byte, 3> b3{};

        b3[0] = (b4[0] & std::byte{0x3f}) << 2 | (b4[1] & std::byte{0x30}) >> 4;
        b3[1] = (b4[1] & std::byte{0x0f}) << 4 | (b4[2] & std::byte{0x3c}) >> 2;
        b3[2] = (b4[2] & std::byte{0x03}) << 6 | b4[3] & std::byte{0x3f};

        if (b4[1] != std::byte{0xff})
            buffer.push_back(b3[0]);

        if (b4[2] != std::byte{0xff})
            buffer.push_back(b3[1]);

        if (b4[3] != std::byte{0xff})
            buffer.push_back(b3[2]);
    }

    return buffer;
}

DEFINE_ERROR_CATEGORY_INSTANCE(zero::encoding::base64::DecodeError)
