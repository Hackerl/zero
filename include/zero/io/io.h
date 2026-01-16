#ifndef ZERO_IO_H
#define ZERO_IO_H

#include <span>
#include <vector>
#include <functional>
#include <zero/error.h>
#include <zero/expect.h>
#include <zero/interface.h>
#include <zero/detail/type_traits.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace zero::io {
    Z_DEFINE_ERROR_CONDITION(
        Error,
        "zero::io",
        UnexpectedEOF, "Unexpected end of file"
    )

#ifdef _WIN32
    using FileDescriptor = HANDLE;
#else
    using FileDescriptor = int;
#endif

    class IFileDescriptor : public virtual Interface {
    public:
        [[nodiscard]] virtual FileDescriptor fd() const = 0;
    };

    class ICloseable : public virtual Interface {
    public:
        virtual std::expected<void, std::error_code> close() = 0;
    };

    class IReader : public virtual Interface {
    public:
        Z_DEFINE_ERROR_CODE_INNER_EX(
            ReadExactlyError,
            "zero::io::IReader",
            UnexpectedEOF, "Unexpected end of file", make_error_condition(Error::UnexpectedEOF)
        )

        virtual std::expected<std::size_t, std::error_code> read(std::span<std::byte> data) = 0;
        virtual std::expected<void, std::error_code> readExactly(std::span<std::byte> data);
        virtual std::expected<std::vector<std::byte>, std::error_code> readAll();
    };

    class IWriter : public virtual Interface {
    public:
        virtual std::expected<std::size_t, std::error_code> write(std::span<const std::byte> data) = 0;
        virtual std::expected<void, std::error_code> writeAll(std::span<const std::byte> data);
    };

    class ISeekable : public virtual Interface {
    public:
        enum class Whence {
            Begin,
            Current,
            End
        };

        virtual std::expected<std::uint64_t, std::error_code> seek(std::int64_t offset, Whence whence) = 0;
        virtual std::expected<void, std::error_code> rewind();
        virtual std::expected<std::uint64_t, std::error_code> length();
        virtual std::expected<std::uint64_t, std::error_code> position();
    };

    class IBufReader : public virtual IReader {
    public:
        [[nodiscard]] virtual std::size_t available() const = 0;
        virtual std::expected<std::string, std::error_code> readLine() = 0;
        virtual std::expected<std::vector<std::byte>, std::error_code> readUntil(std::byte byte) = 0;
        virtual std::expected<void, std::error_code> peek(std::span<std::byte> data) = 0;
    };

    class IBufWriter : public virtual IWriter {
    public:
        [[nodiscard]] virtual std::size_t pending() const = 0;
        virtual std::expected<void, std::error_code> flush() = 0;
    };

    std::expected<std::size_t, std::error_code>
    copy(detail::Trait<IReader> auto &reader, detail::Trait<IWriter> auto &writer) {
        std::size_t written{0};

        while (true) {
            std::array<std::byte, 20480> data; // NOLINT(*-pro-type-member-init)

            const auto n = std::invoke(&IReader::read, reader, data);
            Z_EXPECT(n);

            if (*n == 0)
                break;

            Z_EXPECT(std::invoke(&IWriter::writeAll, writer, std::span{data.data(), *n}));
            written += *n;
        }

        return written;
    }

    class StringReader final : public IReader {
    public:
        explicit StringReader(std::string string);

        std::expected<std::size_t, std::error_code> read(std::span<std::byte> data) override;

    private:
        std::string mString;
    };

    class StringWriter final : public IWriter {
    public:
        std::expected<std::size_t, std::error_code> write(std::span<const std::byte> data) override;

        template<typename Self>
        auto &&data(this Self &&self) {
            return std::forward<Self>(self).mString;
        }

        template<typename Self>
        auto &&operator*(this Self &&self) {
            return std::forward<Self>(self).mString;
        }

    private:
        std::string mString;
    };

    class BytesReader final : public IReader {
    public:
        explicit BytesReader(std::vector<std::byte> bytes);

        std::expected<std::size_t, std::error_code> read(std::span<std::byte> data) override;

    private:
        std::vector<std::byte> mBytes;
    };

    class BytesWriter final : public IWriter {
    public:
        std::expected<std::size_t, std::error_code> write(std::span<const std::byte> data) override;

        template<typename Self>
        auto &&data(this Self &&self) {
            return std::forward<Self>(self).mBytes;
        }

        template<typename Self>
        auto &&operator*(this Self &&self) {
            return std::forward<Self>(self).mBytes;
        }

    private:
        std::vector<std::byte> mBytes;
    };
}

Z_DECLARE_ERROR_CONDITION(zero::io::Error)

Z_DECLARE_ERROR_CODE(zero::io::IReader::ReadExactlyError)

#endif //ZERO_IO_H
