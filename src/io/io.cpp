#include <zero/io/io.h>
#include <cassert>

std::expected<void, std::error_code> zero::io::IReader::readExactly(const std::span<std::byte> data) {
    std::size_t offset{0};

    while (offset < data.size()) {
        const auto n = read(data.subspan(offset));
        EXPECT(n);

        if (*n == 0)
            return std::unexpected{ReadExactlyError::UNEXPECTED_EOF};

        offset += *n;
    }

    return {};
}

std::expected<std::vector<std::byte>, std::error_code> zero::io::IReader::readAll() {
    std::vector<std::byte> data;

    while (true) {
        std::array<std::byte, 10240> buffer; // NOLINT(*-pro-type-member-init)

        const auto n = read(buffer);
        EXPECT(n);

        if (*n == 0)
            break;

        data.insert(data.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(*n));
    }

    return data;
}

std::expected<void, std::error_code> zero::io::IWriter::writeAll(const std::span<const std::byte> data) {
    std::size_t offset{0};

    while (offset < data.size()) {
        const auto n = write(data.subspan(offset));
        EXPECT(n);

        assert(*n != 0);
        offset += *n;
    }

    return {};
}

std::expected<void, std::error_code> zero::io::ISeekable::rewind() {
    EXPECT(seek(0, Whence::BEGIN));
    return {};
}

std::expected<std::uint64_t, std::error_code> zero::io::ISeekable::length() {
    const auto pos = position();
    EXPECT(pos);

    const auto length = seek(0, Whence::END);
    EXPECT(length);

    EXPECT(seek(*pos, Whence::BEGIN));
    return *length;
}

std::expected<std::uint64_t, std::error_code> zero::io::ISeekable::position() {
    return seek(0, Whence::CURRENT);
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
    mBytes.insert(mBytes.end(), data.begin(), data.end());
    return data.size();
}

DEFINE_ERROR_CATEGORY_INSTANCES(
    zero::io::Error,
    zero::io::IReader::ReadExactlyError
)
