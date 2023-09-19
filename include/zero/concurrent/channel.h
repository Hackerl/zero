#ifndef ZERO_CHANNEL_H
#define ZERO_CHANNEL_H

#include <array>
#include <system_error>
#include <tl/expected.hpp>
#include <condition_variable>
#include <zero/atomic/circular_buffer.h>

namespace zero::concurrent {
    enum Error {
        CHANNEL_EOF = 1,
    };

    class Category : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
    };

    const std::error_category &category();
    std::error_code make_error_code(Error e);
}

namespace std {
    template<>
    struct is_error_code_enum<zero::concurrent::Error> : public true_type {

    };
}

namespace zero::concurrent {
    template<typename T>
    class Channel {
    private:
        static constexpr auto SENDER = 0;
        static constexpr auto RECEIVER = 1;

    public:
        explicit Channel(size_t capacity) : mClosed(false), mBuffer(capacity), mWaiting{false, false} {

        }

    public:
        tl::expected<T, std::error_code> tryReceive() {
            auto index = mBuffer.acquire();

            if (!index)
                return tl::unexpected(
                        mClosed ?
                        make_error_code(Error::CHANNEL_EOF) :
                        make_error_code(std::errc::operation_would_block)
                );

            T element = std::move(mBuffer[*index]);
            mBuffer.release(*index);

            trigger<SENDER>();
            return element;
        }

        template<typename U>
        tl::expected<void, std::error_code> trySend(U &&element) {
            if (mClosed)
                return tl::unexpected(Error::CHANNEL_EOF);

            auto index = mBuffer.reserve();

            if (!index)
                return tl::unexpected(make_error_code(std::errc::operation_would_block));

            mBuffer[*index] = std::forward<U>(element);
            mBuffer.commit(*index);

            trigger<RECEIVER>();
            return {};
        }

        tl::expected<T, std::error_code> receive(std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            tl::expected<T, std::error_code> result;

            while (true) {
                auto index = mBuffer.acquire();

                if (!index) {
                    std::unique_lock<std::mutex> lock(mMutex);

                    if (mClosed) {
                        result = tl::unexpected<std::error_code>(Error::CHANNEL_EOF);
                        break;
                    }

                    if (!mBuffer.empty())
                        continue;

                    mWaiting[RECEIVER] = true;

                    if (!timeout) {
                        mCVs[RECEIVER].wait(lock);
                        continue;
                    }

                    if (mCVs[RECEIVER].wait_for(lock, *timeout) == std::cv_status::timeout) {
                        result = tl::unexpected(make_error_code(std::errc::timed_out));
                        break;
                    }

                    continue;
                }

                result = std::move(mBuffer[*index]);
                mBuffer.release(*index);

                trigger<SENDER>();
                break;
            }

            return result;
        }

        template<typename U>
        tl::expected<void, std::error_code>
        send(U &&element, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            if (mClosed)
                return tl::unexpected(Error::CHANNEL_EOF);

            tl::expected<void, std::error_code> result;

            while (true) {
                auto index = mBuffer.reserve();

                if (!index) {
                    std::unique_lock<std::mutex> lock(mMutex);

                    if (mClosed) {
                        result = tl::unexpected<std::error_code>(Error::CHANNEL_EOF);
                        break;
                    }

                    if (!mBuffer.full())
                        continue;

                    mWaiting[SENDER] = true;

                    if (!timeout) {
                        mCVs[SENDER].wait(lock);
                        continue;
                    }

                    if (mCVs[SENDER].wait_for(lock, *timeout) == std::cv_status::timeout) {
                        result = tl::unexpected(make_error_code(std::errc::timed_out));
                        break;
                    }

                    continue;
                }

                mBuffer[*index] = std::forward<U>(element);
                mBuffer.commit(*index);

                trigger<RECEIVER>();
                break;
            }

            return result;
        }

        void close() {
            {
                std::lock_guard<std::mutex> guard(mMutex);

                if (mClosed)
                    return;

                mClosed = true;
            }

            trigger<SENDER>();
            trigger<RECEIVER>();
        }

    private:
        template<int Index>
        void trigger() {
            {
                std::lock_guard<std::mutex> guard(mMutex);

                if (!mWaiting[Index])
                    return;

                mWaiting[Index] = false;
            }

            mCVs[Index].notify_all();
        }

    private:
        std::mutex mMutex;
        std::atomic<bool> mClosed;
        zero::atomic::CircularBuffer<T> mBuffer;
        std::array<std::atomic<bool>, 2> mWaiting;
        std::array<std::condition_variable, 2> mCVs;
    };
}

#endif //ZERO_CHANNEL_H
