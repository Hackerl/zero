#ifndef ZERO_IO_BINARY_H
#define ZERO_IO_BINARY_H

#include "io.h"
#include <zero/expect.h>

namespace zero::io::binary {
    template<typename T>
        requires (std::is_arithmetic_v<T> && sizeof(T) > 1)
    std::expected<T, std::error_code> readLE(traits::Trait<IReader> auto &reader) {
        std::array<std::byte, sizeof(T)> bytes{};
        Z_EXPECT(std::invoke(&IReader::readExactly, reader, bytes));

        T v{};

        for (std::size_t i{0}; i < sizeof(T); ++i)
            v |= static_cast<T>(bytes[i]) << i * 8;

        return v;
    }

    template<typename T>
        requires (std::is_arithmetic_v<T> && sizeof(T) > 1)
    std::expected<T, std::error_code> readBE(traits::Trait<IReader> auto &reader) {
        std::array<std::byte, sizeof(T)> bytes{};
        Z_EXPECT(std::invoke(&IReader::readExactly, reader, bytes));

        T v{};

        for (std::size_t i{0}; i < sizeof(T); ++i)
            v |= static_cast<T>(bytes[i]) << (sizeof(T) - i - 1) * 8;

        return v;
    }

    template<typename T>
        requires (std::is_arithmetic_v<T> && sizeof(T) > 1)
    std::expected<void, std::error_code> writeLE(traits::Trait<IWriter> auto &writer, const T value) {
        std::array<std::byte, sizeof(T)> bytes{};

        for (std::size_t i{0}; i < sizeof(T); ++i)
            bytes[i] = static_cast<std::byte>(value >> i * 8);

        return std::invoke(&IWriter::writeAll, writer, bytes);
    }

    template<typename T>
        requires (std::is_arithmetic_v<T> && sizeof(T) > 1)
    std::expected<void, std::error_code> writeBE(traits::Trait<IWriter> auto &writer, const T value) {
        std::array<std::byte, sizeof(T)> bytes{};

        for (std::size_t i{0}; i < sizeof(T); ++i)
            bytes[i] = static_cast<std::byte>(value >> (sizeof(T) - i - 1) * 8);

        return std::invoke(&IWriter::writeAll, writer, bytes);
    }
}

#endif // ZERO_IO_BINARY_H
