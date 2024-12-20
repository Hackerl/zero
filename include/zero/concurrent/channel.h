#ifndef ZERO_CHANNEL_H
#define ZERO_CHANNEL_H

#include <chrono>
#include <optional>
#include <expected>
#include <condition_variable>
#include <zero/error.h>
#include <zero/atomic/circular_buffer.h>

namespace zero::concurrent {
    static constexpr auto SENDER = 0;
    static constexpr auto RECEIVER = 1;

    template<typename T>
    struct ChannelCore {
        explicit ChannelCore(const std::size_t capacity) : buffer{capacity} {
        }

        std::mutex mutex;
        std::atomic<bool> closed;
        atomic::CircularBuffer<T> buffer;
        std::array<std::atomic<bool>, 2> waiting;
        std::array<std::condition_variable, 2> cvs;
        std::array<std::atomic<std::size_t>, 2> counters;

        void trigger(const std::size_t index) {
            {
                std::lock_guard guard{mutex};

                if (!waiting[index])
                    return;

                waiting[index] = false;
            }

            cvs[index].notify_all();
        }

        void close() {
            {
                std::lock_guard guard{mutex};

                if (closed)
                    return;

                closed = true;
            }

            trigger(SENDER);
            trigger(RECEIVER);
        }
    };

    DEFINE_ERROR_CODE_EX(
        TrySendError,
        "zero::concurrent::Sender::trySend",
        DISCONNECTED, "sending on a disconnected channel", DEFAULT_ERROR_CONDITION,
        FULL, "sending on a full channel", std::errc::operation_would_block
    )

    DEFINE_ERROR_CODE_EX(
        SendError,
        "zero::concurrent::Sender::send",
        DISCONNECTED, "sending on a disconnected channel", DEFAULT_ERROR_CONDITION,
        TIMEOUT, "timed out waiting on send operation", std::errc::timed_out
    )

    template<typename T>
    class Sender {
    public:
        explicit Sender(std::shared_ptr<ChannelCore<T>> core) : mCore{std::move(core)} {
            ++mCore->counters[SENDER];
        }

        Sender(const Sender &rhs) : mCore{rhs.mCore} {
            ++mCore->counters[SENDER];
        }

        Sender(Sender &&rhs) = default;

        Sender &operator=(const Sender &rhs) {
            mCore = rhs.mCore;
            ++mCore->counters[SENDER];
            return *this;
        }

        Sender &operator=(Sender &&rhs) noexcept = default;

        ~Sender() {
            if (!mCore)
                return;

            if (--mCore->counters[SENDER] > 0)
                return;

            mCore->close();
        }

        template<typename U = T>
        std::expected<void, TrySendError> trySend(U &&element) {
            if (mCore->closed)
                return std::unexpected{TrySendError::DISCONNECTED};

            const auto index = mCore->buffer.reserve();

            if (!index)
                return std::unexpected{TrySendError::FULL};

            mCore->buffer[*index] = std::forward<U>(element);
            mCore->buffer.commit(*index);

            mCore->trigger(RECEIVER);
            return {};
        }

        template<typename U = T>
        std::expected<void, SendError>
        send(U &&element, const std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            if (mCore->closed)
                return std::unexpected{SendError::DISCONNECTED};

            while (true) {
                const auto index = mCore->buffer.reserve();

                if (index) {
                    mCore->buffer[*index] = std::forward<U>(element);
                    mCore->buffer.commit(*index);
                    mCore->trigger(RECEIVER);
                    return {};
                }

                std::unique_lock lock{mCore->mutex};

                if (mCore->closed)
                    return std::unexpected{SendError::DISCONNECTED};

                if (!mCore->buffer.full())
                    continue;

                mCore->waiting[SENDER] = true;

                if (!timeout) {
                    mCore->cvs[SENDER].wait(lock);
                    continue;
                }

                if (mCore->cvs[SENDER].wait_for(lock, *timeout) == std::cv_status::timeout)
                    return std::unexpected{SendError::TIMEOUT};
            }
        }

        void close() {
            mCore->close();
        }

        [[nodiscard]] std::size_t size() const {
            return mCore->buffer.size();
        }

        [[nodiscard]] std::size_t capacity() const {
            return mCore->buffer.capacity();
        }

        [[nodiscard]] bool empty() const {
            return mCore->buffer.empty();
        }

        [[nodiscard]] bool full() const {
            return mCore->buffer.full();
        }

        [[nodiscard]] bool closed() const {
            return mCore->closed;
        }

    private:
        std::shared_ptr<ChannelCore<T>> mCore;
    };

    DEFINE_ERROR_CODE_EX(
        TryReceiveError,
        "zero::concurrent::Sender::tryReceive",
        DISCONNECTED, "receiving on an empty and disconnected channel", DEFAULT_ERROR_CONDITION,
        EMPTY, "receiving on an empty channel", std::errc::operation_would_block
    )

    DEFINE_ERROR_CODE_EX(
        ReceiveError,
        "zero::concurrent::Sender::receive",
        DISCONNECTED, "channel is empty and disconnected", DEFAULT_ERROR_CONDITION,
        TIMEOUT, "timed out waiting on receive operation", std::errc::timed_out
    )

    template<typename T>
    class Receiver {
    public:
        explicit Receiver(std::shared_ptr<ChannelCore<T>> core) : mCore{std::move(core)} {
            ++mCore->counters[RECEIVER];
        }

        Receiver(const Receiver &rhs) : mCore{rhs.mCore} {
            ++mCore->counters[RECEIVER];
        }

        Receiver(Receiver &&rhs) = default;

        Receiver &operator=(const Receiver &rhs) {
            mCore = rhs.mCore;
            ++mCore->counters[RECEIVER];
            return *this;
        }

        Receiver &operator=(Receiver &&rhs) noexcept = default;

        ~Receiver() {
            if (!mCore)
                return;

            if (--mCore->counters[RECEIVER] > 0)
                return;

            mCore->close();
        }

        std::expected<T, TryReceiveError> tryReceive() {
            const auto index = mCore->buffer.acquire();

            if (!index)
                return std::unexpected{mCore->closed ? TryReceiveError::DISCONNECTED : TryReceiveError::EMPTY};

            auto element = std::move(mCore->buffer[*index]);
            mCore->buffer.release(*index);

            mCore->trigger(SENDER);
            return element;
        }

        std::expected<T, ReceiveError> receive(const std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            while (true) {
                const auto index = mCore->buffer.acquire();

                if (index) {
                    auto element = std::move(mCore->buffer[*index]);
                    mCore->buffer.release(*index);
                    mCore->trigger(SENDER);
                    return element;
                }

                std::unique_lock lock{mCore->mutex};

                if (!mCore->buffer.empty())
                    continue;

                if (mCore->closed)
                    return std::unexpected{ReceiveError::DISCONNECTED};

                mCore->waiting[RECEIVER] = true;

                if (!timeout) {
                    mCore->cvs[RECEIVER].wait(lock);
                    continue;
                }

                if (mCore->cvs[RECEIVER].wait_for(lock, *timeout) == std::cv_status::timeout)
                    return std::unexpected{ReceiveError::TIMEOUT};
            }
        }

        [[nodiscard]] std::size_t size() const {
            return mCore->buffer.size();
        }

        [[nodiscard]] std::size_t capacity() const {
            return mCore->buffer.capacity();
        }

        [[nodiscard]] bool empty() const {
            return mCore->buffer.empty();
        }

        [[nodiscard]] bool full() const {
            return mCore->buffer.full();
        }

        [[nodiscard]] bool closed() const {
            return mCore->closed;
        }

    private:
        std::shared_ptr<ChannelCore<T>> mCore;
    };

    DEFINE_ERROR_CONDITION_EX(
        ChannelError,
        "zero::concurrent::channel",
        DISCONNECTED,
        "channel disconnected",
        [](const std::error_code &ec) {
            return ec == make_error_code(TrySendError::DISCONNECTED) ||
                ec == make_error_code(SendError::DISCONNECTED) ||
                ec == make_error_code(TryReceiveError::DISCONNECTED) ||
                ec == make_error_code(ReceiveError::DISCONNECTED);
        }
    )

    template<typename T>
    using Channel = std::pair<Sender<T>, Receiver<T>>;

    template<typename T>
    Channel<T> channel(const std::size_t capacity) {
        const auto core = std::make_shared<ChannelCore<T>>(capacity);
        return {Sender<T>{core}, Receiver<T>{core}};
    }
}

DECLARE_ERROR_CODES(
    zero::concurrent::TrySendError,
    zero::concurrent::SendError,
    zero::concurrent::TryReceiveError,
    zero::concurrent::ReceiveError
)

DECLARE_ERROR_CONDITION(zero::concurrent::ChannelError)

#endif //ZERO_CHANNEL_H
