#ifndef ZERO_IO_BUFFER_H
#define ZERO_IO_BUFFER_H

#include "io.h"
#include <cassert>
#include <algorithm>

namespace zero::io {
    Z_DEFINE_ERROR_CODE_EX(
        BufReaderError,
        "zero::io::BufReader",
        INVALID_ARGUMENT, "invalid argument", std::errc::invalid_argument,
        UNEXPECTED_EOF, "unexpected end of file", Error::UNEXPECTED_EOF
    )

    template<detail::Trait<IReader> T>
    class BufReader final : public IBufReader {
        static constexpr auto DEFAULT_BUFFER_CAPACITY = 8192;

    public:
        explicit BufReader(T reader, const std::size_t capacity = DEFAULT_BUFFER_CAPACITY)
            : mReader{std::move(reader)}, mCapacity{capacity}, mHead{0}, mTail{0},
              mBuffer{std::make_unique<std::byte[]>(capacity)} {
        }

        [[nodiscard]] std::size_t capacity() const {
            return mCapacity;
        }

        std::expected<std::size_t, std::error_code> read(const std::span<std::byte> data) override {
            if (available() == 0) {
                if (data.size() >= mCapacity)
                    return std::invoke(&IReader::read, mReader, data);

                mHead = 0;
                mTail = 0;

                const auto n = std::invoke(&IReader::read, mReader, std::span{mBuffer.get(), mCapacity});
                Z_EXPECT(n);

                if (*n == 0)
                    return 0;

                mTail = *n;
            }

            const auto size = (std::min)(available(), data.size());

            std::copy_n(mBuffer.get() + mHead, size, data.begin());
            mHead += size;

            return size;
        }

        [[nodiscard]] std::size_t available() const override {
            return mTail - mHead;
        }

        std::expected<std::string, std::error_code> readLine() override {
            auto data = readUntil(std::byte{'\n'});
            Z_EXPECT(data);

            if (!data->empty() && data->back() == std::byte{'\r'})
                data->pop_back();

            return std::string{reinterpret_cast<const char *>(data->data()), data->size()};
        }

        std::expected<std::vector<std::byte>, std::error_code> readUntil(const std::byte byte) override {
            std::vector<std::byte> data;

            while (true) {
                const auto first = mBuffer.get() + mHead;
                const auto last = mBuffer.get() + mTail;

                if (const auto it = std::find(first, last, byte); it != last) {
                    data.append_range(std::ranges::subrange{first, it});
                    mHead += std::distance(first, it) + 1;
                    return data;
                }

                data.append_range(std::ranges::subrange{first, last});

                mHead = 0;
                mTail = 0;

                const auto n = std::invoke(&IReader::read, mReader, std::span{mBuffer.get(), mCapacity});
                Z_EXPECT(n);

                if (*n == 0)
                    return std::unexpected{make_error_code(BufReaderError::UNEXPECTED_EOF)};

                mTail = *n;
            }
        }

        std::expected<void, std::error_code> peek(const std::span<std::byte> data) override {
            if (data.size() > mCapacity)
                return std::unexpected{make_error_code(BufReaderError::INVALID_ARGUMENT)};

            if (const auto available = this->available(); available < data.size()) {
                if (mHead > 0) {
                    std::copy(mBuffer.get() + mHead, mBuffer.get() + mTail, mBuffer.get());
                    mHead = 0;
                    mTail = available;
                }

                while (mTail < data.size()) {
                    const auto n = std::invoke(
                        &IReader::read,
                        mReader,
                        std::span{mBuffer.get() + mTail, mCapacity - mTail}
                    );
                    Z_EXPECT(n);

                    if (*n == 0)
                        return std::unexpected{make_error_code(BufReaderError::UNEXPECTED_EOF)};

                    mTail += *n;
                }
            }

            assert(available() >= data.size());
            std::copy_n(mBuffer.get() + mHead, data.size(), data.begin());
            return {};
        }

    private:
        T mReader;
        std::size_t mCapacity;
        std::size_t mHead;
        std::size_t mTail;
        std::unique_ptr<std::byte[]> mBuffer;
    };

    template<detail::Trait<IWriter> T>
    class BufWriter final : public IBufWriter {
        static constexpr auto DEFAULT_BUFFER_CAPACITY = 8192;

    public:
        explicit BufWriter(T writer, const std::size_t capacity = DEFAULT_BUFFER_CAPACITY)
            : mWriter{std::move(writer)}, mCapacity{capacity}, mPending{0},
              mBuffer{std::make_unique<std::byte[]>(capacity)} {
        }

    private:
        std::expected<std::size_t, std::error_code> writeOnce(const std::span<const std::byte> data) {
            assert(mPending <= mCapacity);

            if (mPending == mCapacity) {
                Z_EXPECT(flush());
            }

            const auto size = (std::min)(mCapacity - mPending, data.size());
            std::copy_n(data.begin(), size, mBuffer.get() + mPending);

            mPending += size;
            return size;
        }

    public:
        [[nodiscard]] std::size_t capacity() const {
            return mCapacity;
        }

        std::expected<std::size_t, std::error_code> write(const std::span<const std::byte> data) override {
            std::size_t offset{0};

            while (offset < data.size()) {
                const auto n = writeOnce(data.subspan(offset));

                if (!n) {
                    if (offset > 0)
                        break;

                    return std::unexpected{n.error()};
                }

                assert(*n != 0);
                offset += *n;
            }

            return offset;
        }

        [[nodiscard]] std::size_t pending() const override {
            return mPending;
        }

        std::expected<void, std::error_code> flush() override {
            std::size_t offset{0};

            while (offset < mPending) {
                const auto n = std::invoke(
                    &IWriter::write,
                    mWriter,
                    std::span{mBuffer.get() + offset, mPending - offset}
                );
                Z_EXPECT(n);

                offset += *n;
            }

            if (offset > 0 && offset < mPending)
                std::copy(mBuffer.get() + offset, mBuffer.get() + mPending, mBuffer.get());

            mPending -= offset;
            return {};
        }

    private:
        T mWriter;
        std::size_t mCapacity;
        std::size_t mPending;
        std::unique_ptr<std::byte[]> mBuffer;
    };
}

Z_DECLARE_ERROR_CODES(zero::io::BufReaderError)

#endif // ZERO_IO_BUFFER_H
