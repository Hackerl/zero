#ifndef ZERO_CHANNEL_H
#define ZERO_CHANNEL_H

#include <array>
#include <chrono>
#include <optional>
#include <system_error>
#include <tl/expected.hpp>
#include <condition_variable>
#include <zero/atomic/circular_buffer.h>

namespace zero::concurrent {
    enum ChannelError {
        CHANNEL_EOF = 1,
        BROKEN_CHANNEL,
        SEND_TIMEOUT,
        RECEIVE_TIMEOUT,
        EMPTY,
        FULL
    };

    class ChannelErrorCategory final : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
        [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override;
    };

    std::error_code make_error_code(ChannelError e);

    template<typename T>
    class Channel {
        static constexpr auto SENDER = 0;
        static constexpr auto RECEIVER = 1;

    public:
        explicit Channel(std::size_t capacity) : mClosed(false), mBuffer(capacity), mWaiting{false, false} {
        }

    private:
        template<int Index>
        void trigger() {
            {
                std::lock_guard guard(mMutex);

                if (!mWaiting[Index])
                    return;

                mWaiting[Index] = false;
            }

            mCVs[Index].notify_all();
        }

    public:
        tl::expected<T, ChannelError> tryReceive() {
            const auto index = mBuffer.acquire();

            if (!index)
                return tl::unexpected(mClosed ? CHANNEL_EOF : EMPTY);

            T element = std::move(mBuffer[*index]);
            mBuffer.release(*index);

            trigger<SENDER>();
            return element;
        }

        template<typename U>
        tl::expected<void, ChannelError> trySend(U &&element) {
            if (mClosed)
                return tl::unexpected(BROKEN_CHANNEL);

            const auto index = mBuffer.reserve();

            if (!index)
                return tl::unexpected(FULL);

            mBuffer[*index] = std::forward<U>(element);
            mBuffer.commit(*index);

            trigger<RECEIVER>();
            return {};
        }

        tl::expected<T, ChannelError> receive(const std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            tl::expected<T, ChannelError> result;

            while (true) {
                const auto index = mBuffer.acquire();

                if (!index) {
                    std::unique_lock lock(mMutex);

                    if (mClosed) {
                        result = tl::unexpected(CHANNEL_EOF);
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
                        result = tl::unexpected(RECEIVE_TIMEOUT);
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
        tl::expected<void, ChannelError>
        send(U &&element, const std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            if (mClosed)
                return tl::unexpected(BROKEN_CHANNEL);

            tl::expected<void, ChannelError> result;

            while (true) {
                const auto index = mBuffer.reserve();

                if (!index) {
                    std::unique_lock lock(mMutex);

                    if (mClosed) {
                        result = tl::unexpected(BROKEN_CHANNEL);
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
                        result = tl::unexpected(SEND_TIMEOUT);
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
            assert(!mClosed);

            {
                std::lock_guard guard(mMutex);

                if (mClosed)
                    return;

                mClosed = true;
            }

            trigger<SENDER>();
            trigger<RECEIVER>();
        }

        [[nodiscard]] std::size_t size() const {
            return mBuffer.size();
        }

        [[nodiscard]] std::size_t capacity() const {
            return mBuffer.capacity();
        }

        [[nodiscard]] bool empty() const {
            return mBuffer.empty();
        }

        [[nodiscard]] bool full() const {
            return mBuffer.full();
        }

        [[nodiscard]] bool closed() const {
            return mClosed;
        }

    private:
        std::mutex mMutex;
        std::atomic<bool> mClosed;
        atomic::CircularBuffer<T> mBuffer;
        std::array<std::atomic<bool>, 2> mWaiting;
        std::array<std::condition_variable, 2> mCVs;
    };
}

template<>
struct std::is_error_code_enum<zero::concurrent::ChannelError> : std::true_type {
};

#endif //ZERO_CHANNEL_H
