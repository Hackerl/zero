#include <zero/io/io.h>
#include <cassert>
#include <algorithm>

std::expected<void, std::error_code> zero::io::IReader::readExactly(const std::span<std::byte> data) {
    std::size_t offset{0};

    while (offset < data.size()) {
        const auto n = read(data.subspan(offset));
        Z_EXPECT(n);

        if (*n == 0)
            return std::unexpected{ReadExactlyError::UnexpectedEOF};

        offset += *n;
    }

    return {};
}

std::expected<std::vector<std::byte>, std::error_code> zero::io::IReader::readAll() {
    std::vector<std::byte> data;

    while (true) {
        std::array<std::byte, 10240> buffer; // NOLINT(*-pro-type-member-init)

        const auto n = read(buffer);
        Z_EXPECT(n);

        if (*n == 0)
            break;

        data.append_range(std::span{buffer.data(), *n});
    }

    return data;
}

std::expected<void, std::error_code> zero::io::IWriter::writeAll(const std::span<const std::byte> data) {
    std::size_t offset{0};

    while (offset < data.size()) {
        const auto n = write(data.subspan(offset));
        Z_EXPECT(n);

        assert(*n != 0);
        offset += *n;
    }

    return {};
}

std::expected<void, std::error_code> zero::io::ISeekable::rewind() {
    Z_EXPECT(seek(0, Whence::Begin));
    return {};
}

std::expected<std::uint64_t, std::error_code> zero::io::ISeekable::length() {
    const auto pos = position();
    Z_EXPECT(pos);

    const auto length = seek(0, Whence::End);
    Z_EXPECT(length);

    Z_EXPECT(seek(*pos, Whence::Begin));
    return *length;
}

std::expected<std::uint64_t, std::error_code> zero::io::ISeekable::position() {
    return seek(0, Whence::Current);
}

zero::io::StringReader::StringReader(std::string string) : mString{std::move(string)} {
}

std::expected<std::size_t, std::error_code> zero::io::StringReader::read(const std::span<std::byte> data) {
    if (mString.empty())
        return 0;

    const auto n = (std::min)(data.size(), mString.size());

    std::copy_n(mString.begin(), n, reinterpret_cast<char *>(data.data()));
    mString.erase(0, n);

    return n;
}

std::expected<std::size_t, std::error_code> zero::io::StringWriter::write(const std::span<const std::byte> data) {
    mString.append(reinterpret_cast<const char *>(data.data()), data.size());
    return data.size();
}

zero::io::BytesReader::BytesReader(std::vector<std::byte> bytes) : mBytes{std::move(bytes)} {
}

std::expected<std::size_t, std::error_code> zero::io::BytesReader::read(const std::span<std::byte> data) {
    if (mBytes.empty())
        return 0;

    const auto n = (std::min)(data.size(), mBytes.size());

    std::copy_n(mBytes.begin(), n, data.begin());
    mBytes.erase(mBytes.begin(), mBytes.begin() + static_cast<std::ptrdiff_t>(n));

    return n;
}

std::expected<std::size_t, std::error_code> zero::io::BytesWriter::write(const std::span<const std::byte> data) {
    mBytes.append_range(data);
    return data.size();
}

Z_DEFINE_ERROR_CATEGORY_INSTANCES(
    zero::io::Error,
    zero::io::IReader::ReadExactlyError
)
