#include <zero/encoding/base64.h>

constexpr auto ENCODE_MAP = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr unsigned char DECODE_MAP[] = {
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 62, 255, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 255, 255, 255,
        255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 63,
        255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255
};

std::string zero::encoding::base64::encode(const unsigned char *buffer, size_t length) {
    size_t missing = (length + 2) / 3 * 3 - length;
    size_t size = (length + missing) * 4 / 3;

    std::string encoded;
    encoded.reserve(size);

    for (unsigned int i = 0; i < length; i += 3) {
        unsigned char b3[3] = {};

        b3[0] = buffer[i];
        b3[1] = (i + 1 < length) ? buffer[i + 1] : 0;
        b3[2] = (i + 2 < length) ? buffer[i + 2] : 0;

        unsigned char b4[4] = {};

        b4[0] = (b3[0] & 0xfc) >> 2;
        b4[1] = ((b3[0] & 0x03) << 4) + ((b3[1] & 0xf0) >> 4);
        b4[2] = ((b3[1] & 0x0f) << 2) + ((b3[2] & 0xc0) >> 6);
        b4[3] = b3[2] & 0x3f;

        encoded.push_back(ENCODE_MAP[b4[0]]);
        encoded.push_back(ENCODE_MAP[b4[1]]);
        encoded.push_back(ENCODE_MAP[b4[2]]);
        encoded.push_back(ENCODE_MAP[b4[3]]);
    }

    for (size_t i = 0; i < missing; i++)
        encoded[size - i - 1] = '=';

    return encoded;
}

std::optional<std::vector<unsigned char>> zero::encoding::base64::decode(const std::string &encoded) {
    if (encoded.length() % 4)
        return std::nullopt;

    size_t size = encoded.size();

    std::vector<unsigned char> buffer;
    buffer.reserve(3 * size / 4);

    for (size_t i = 0; i < size; i += 4) {
        unsigned char b4[4] = {};

        b4[0] = (encoded[i] <= 'z') ? DECODE_MAP[encoded[i]] : 0xff;
        b4[1] = (encoded[i + 1] <= 'z') ? DECODE_MAP[encoded[i + 1]] : 0xff;
        b4[2] = (encoded[i + 2] <= 'z') ? DECODE_MAP[encoded[i + 2]] : 0xff;
        b4[3] = (encoded[i + 3] <= 'z') ? DECODE_MAP[encoded[i + 3]] : 0xff;

        unsigned char b3[3] = {};

        b3[0] = ((b4[0] & 0x3f) << 2) + ((b4[1] & 0x30) >> 4);
        b3[1] = ((b4[1] & 0x0f) << 4) + ((b4[2] & 0x3c) >> 2);
        b3[2] = ((b4[2] & 0x03) << 6) + (b4[3] & 0x3f);

        if (b4[1] != 0xff)
            buffer.push_back(b3[0]);

        if (b4[2] != 0xff)
            buffer.push_back(b3[1]);

        if (b4[3] != 0xff)
            buffer.push_back(b3[2]);
    }

    return buffer;
}
