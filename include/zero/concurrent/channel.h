#ifndef ZERO_CONCURRENT_CHANNEL_H
#define ZERO_CONCURRENT_CHANNEL_H

#include <chrono>
#include <optional>
#include <condition_variable>
#include <zero/error.h>
#include <zero/atomic/circular_buffer.h>

namespace zero::concurrent {
    template<typename T>
    struct ChannelCore {
        struct Context {
            bool waiting{};
            std::condition_variable cv;
            std::atomic<std::size_t> counter;
        };

        explicit ChannelCore(const std::size_t capacity) : buffer{capacity + 1} {
        }

        std::mutex mutex;
        std::atomic<bool> closed;
        atomic::CircularBuffer<T> buffer;
        Context sender;
        Context receiver;

        void notifySender() {
            {
                const std::lock_guard guard{mutex};

                if (!sender.waiting)
                    return;

                sender.waiting = false;
            }

            sender.cv.notify_all();
        }

        void notifyReceiver() {
            {
                const std::lock_guard guard{mutex};

                if (!receiver.waiting)
                    return;

                receiver.waiting = false;
            }

            receiver.cv.notify_all();
        }

        void close() {
            {
                const std::lock_guard guard{mutex};

                if (closed)
                    return;

                closed = true;
            }

            notifySender();
            notifyReceiver();
        }
    };

    Z_DEFINE_ERROR_CODE_EX(
        TrySendError,
        "zero::concurrent::Sender::trySend",
        Disconnected, "Sending on a disconnected channel", Z_DEFAULT_ERROR_CONDITION,
        Full, "Sending on a full channel", std::errc::operation_would_block
    )

    Z_DEFINE_ERROR_CODE_EX(
        SendError,
        "zero::concurrent::Sender::send",
        Disconnected, "Sending on a disconnected channel", Z_DEFAULT_ERROR_CONDITION,
        Timeout, "Send operation timed out", std::errc::timed_out
    )

    template<typename T>
    class Sender {
    public:
        explicit Sender(std::shared_ptr<ChannelCore<T>> core) : mCore{std::move(core)} {
            ++mCore->sender.counter;
        }

        Sender(const Sender &rhs) : mCore{rhs.mCore} {
            ++mCore->sender.counter;
        }

        Sender(Sender &&rhs) = default;

        Sender &operator=(const Sender &rhs) {
            mCore = rhs.mCore;
            ++mCore->sender.counter;
            return *this;
        }

        Sender &operator=(Sender &&rhs) noexcept = default;

        ~Sender() {
            if (!mCore)
                return;

            if (--mCore->sender.counter > 0)
                return;

            mCore->close();
        }

        template<typename U = T>
        std::expected<void, TrySendError> trySend(U &&element) {
            if (mCore->closed)
                return std::unexpected{TrySendError::Disconnected};

            const auto index = mCore->buffer.reserve();

            if (!index)
                return std::unexpected{TrySendError::Full};

            mCore->buffer[*index] = std::forward<U>(element);
            mCore->buffer.commit(*index);

            mCore->notifyReceiver();
            return {};
        }

        std::expected<void, std::pair<T, TrySendError>> trySendEx(T &&element) {
            if (mCore->closed)
                return std::unexpected{std::pair{std::move(element), TrySendError::Disconnected}};

            const auto index = mCore->buffer.reserve();

            if (!index)
                return std::unexpected{std::pair{std::move(element), TrySendError::Full}};

            mCore->buffer[*index] = std::move(element);
            mCore->buffer.commit(*index);

            mCore->notifyReceiver();
            return {};
        }

        template<typename U = T>
        std::expected<void, SendError>
        send(U &&element, const std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            if (mCore->closed)
                return std::unexpected{SendError::Disconnected};

            while (true) {
                const auto index = mCore->buffer.reserve();

                if (index) {
                    mCore->buffer[*index] = std::forward<U>(element);
                    mCore->buffer.commit(*index);
                    mCore->notifyReceiver();
                    return {};
                }

                std::unique_lock lock{mCore->mutex};

                if (mCore->closed)
                    return std::unexpected{SendError::Disconnected};

                if (!mCore->buffer.full())
                    continue;

                mCore->sender.waiting = true;

                if (!timeout) {
                    mCore->sender.cv.wait(lock);
                    continue;
                }

                if (mCore->sender.cv.wait_for(lock, *timeout) == std::cv_status::timeout)
                    return std::unexpected{SendError::Timeout};
            }
        }

        std::expected<void, std::pair<T, SendError>>
        sendEx(T &&element, const std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            if (mCore->closed)
                return std::unexpected{std::pair{std::move(element), SendError::Disconnected}};

            while (true) {
                const auto index = mCore->buffer.reserve();

                if (index) {
                    mCore->buffer[*index] = std::move(element);
                    mCore->buffer.commit(*index);
                    mCore->notifyReceiver();
                    return {};
                }

                std::unique_lock lock{mCore->mutex};

                if (mCore->closed)
                    return std::unexpected{std::pair{std::move(element), SendError::Disconnected}};

                if (!mCore->buffer.full())
                    continue;

                mCore->sender.waiting = true;

                if (!timeout) {
                    mCore->sender.cv.wait(lock);
                    continue;
                }

                if (mCore->sender.cv.wait_for(lock, *timeout) == std::cv_status::timeout)
                    return std::unexpected{std::pair{std::move(element), SendError::Timeout}};
            }
        }

        void close() {
            mCore->close();
        }

        [[nodiscard]] std::size_t size() const {
            return mCore->buffer.size();
        }

        [[nodiscard]] std::size_t capacity() const {
            return mCore->buffer.capacity() - 1;
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

    Z_DEFINE_ERROR_CODE_EX(
        TryReceiveError,
        "zero::concurrent::Receiver::tryReceive",
        Disconnected, "Receiving on an empty and disconnected channel", Z_DEFAULT_ERROR_CONDITION,
        Empty, "Receiving on an empty channel", std::errc::operation_would_block
    )

    Z_DEFINE_ERROR_CODE_EX(
        ReceiveError,
        "zero::concurrent::Receiver::receive",
        Disconnected, "Receiving on an empty and disconnected channel", Z_DEFAULT_ERROR_CONDITION,
        Timeout, "Receive operation timed out", std::errc::timed_out
    )

    template<typename T>
    class Receiver {
    public:
        explicit Receiver(std::shared_ptr<ChannelCore<T>> core) : mCore{std::move(core)} {
            ++mCore->receiver.counter;
        }

        Receiver(const Receiver &rhs) : mCore{rhs.mCore} {
            ++mCore->receiver.counter;
        }

        Receiver(Receiver &&rhs) = default;

        Receiver &operator=(const Receiver &rhs) {
            mCore = rhs.mCore;
            ++mCore->receiver.counter;
            return *this;
        }

        Receiver &operator=(Receiver &&rhs) noexcept = default;

        ~Receiver() {
            if (!mCore)
                return;

            if (--mCore->receiver.counter > 0)
                return;

            mCore->close();
        }

        std::expected<T, TryReceiveError> tryReceive() {
            const auto index = mCore->buffer.acquire();

            if (!index)
                return std::unexpected{mCore->closed ? TryReceiveError::Disconnected : TryReceiveError::Empty};

            auto element = std::move(mCore->buffer[*index]);
            mCore->buffer.release(*index);

            mCore->notifySender();
            return element;
        }

        std::expected<T, ReceiveError> receive(const std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
            while (true) {
                const auto index = mCore->buffer.acquire();

                if (index) {
                    auto element = std::move(mCore->buffer[*index]);
                    mCore->buffer.release(*index);
                    mCore->notifySender();
                    return element;
                }

                std::unique_lock lock{mCore->mutex};

                if (!mCore->buffer.empty())
                    continue;

                if (mCore->closed)
                    return std::unexpected{ReceiveError::Disconnected};

                mCore->receiver.waiting = true;

                if (!timeout) {
                    mCore->receiver.cv.wait(lock);
                    continue;
                }

                if (mCore->receiver.cv.wait_for(lock, *timeout) == std::cv_status::timeout)
                    return std::unexpected{ReceiveError::Timeout};
            }
        }

        [[nodiscard]] std::size_t size() const {
            return mCore->buffer.size();
        }

        [[nodiscard]] std::size_t capacity() const {
            return mCore->buffer.capacity() - 1;
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

    Z_DEFINE_ERROR_CONDITION_EX(
        ChannelError,
        "zero::concurrent::channel",
        Disconnected,
        "Channel disconnected",
        [](const std::error_code &ec) {
            return ec == make_error_code(TrySendError::Disconnected) ||
                ec == make_error_code(SendError::Disconnected) ||
                ec == make_error_code(TryReceiveError::Disconnected) ||
                ec == make_error_code(ReceiveError::Disconnected);
        }
    )

    template<typename T>
    using Channel = std::pair<Sender<T>, Receiver<T>>;

    template<typename T>
    Channel<T> channel(const std::size_t capacity = 1) {
        const auto core = std::make_shared<ChannelCore<T>>(capacity);
        return {Sender<T>{core}, Receiver<T>{core}};
    }
}

Z_DECLARE_ERROR_CODES(
    zero::concurrent::TrySendError,
    zero::concurrent::SendError,
    zero::concurrent::TryReceiveError,
    zero::concurrent::ReceiveError
)

Z_DECLARE_ERROR_CONDITION(zero::concurrent::ChannelError)

#endif //ZERO_CONCURRENT_CHANNEL_H
