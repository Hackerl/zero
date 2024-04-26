#include <zero/encoding/hex.h>
#include <zero/strings/strings.h>
#include <zero/singleton.h>
#include <cassert>
#include <array>

constexpr std::array HEX_MAP = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

const char *zero::encoding::hex::DecodeErrorCategory::name() const noexcept {
    return "zero::encoding::hex::decode";
}

std::string zero::encoding::hex::DecodeErrorCategory::message(const int value) const {
    std::string msg;

    switch (value) {
    case INVALID_LENGTH:
        msg = "invalid length for a hex string";
        break;

    case INVALID_HEX_CHARACTER:
        msg = "invalid hex character";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

std::error_condition
zero::encoding::hex::DecodeErrorCategory::default_error_condition(const int value) const noexcept {
    if (value == INVALID_LENGTH || value == INVALID_HEX_CHARACTER)
        return std::errc::invalid_argument;

    return error_category::default_error_condition(value);
}

std::error_code zero::encoding::hex::make_error_code(const DecodeError e) {
    return {e, Singleton<DecodeErrorCategory>::getInstance()};
}

std::string zero::encoding::hex::encode(const std::span<const std::byte> buffer) {
    std::string encoded;

    for (const auto &byte: buffer) {
        encoded.push_back(HEX_MAP[std::to_integer<unsigned char>((byte & std::byte{0xf0}) >> 4)]);
        encoded.push_back(HEX_MAP[std::to_integer<unsigned char>(byte & std::byte{0x0f})]);
    }

    return encoded;
}

tl::expected<std::vector<std::byte>, zero::encoding::hex::DecodeError>
zero::encoding::hex::decode(const std::string_view encoded) {
    if (encoded.length() % 2)
        return tl::unexpected(INVALID_LENGTH);

    tl::expected<std::vector<std::byte>, DecodeError> result;

    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        auto n = strings::toNumber<unsigned int>(encoded.substr(i, 2), 16);

        if (!n) {
            assert(n.error() == std::errc::invalid_argument);
            result = tl::unexpected(INVALID_HEX_CHARACTER);
            break;
        }

        result->push_back(static_cast<std::byte>(*n & 0xff));
    }

    return result;
}
