#ifndef ZERO_CIRCULAR_BUFFER_H
#define ZERO_CIRCULAR_BUFFER_H

#include <cstddef>
#include <atomic>
#include <optional>

namespace zero::atomic {
    template<typename T, size_t N>
    class CircularBuffer {
    private:
        static constexpr auto MODULO = SIZE_MAX / N * N;

        enum State : long {
            IDLE,
            PUTTING,
            VALID,
            TAKING
        };

    public:
        CircularBuffer() : mHead(0), mTail(0), mState() {

        }

    public:
        std::optional<size_t> reserve() {
            size_t index = mTail;

            do {
                if (full())
                    return std::nullopt;
            } while (!mTail.compare_exchange_weak(index, (index + 1) % MODULO));

            index %= N;

            while (true) {
                State expected = IDLE;

                if (mState[index].compare_exchange_weak(expected, PUTTING))
                    break;
            }

            return index;
        }

        void commit(size_t index) {
            mState[index] = VALID;
        }

    public:
        std::optional<size_t> acquire() {
            size_t index = mHead;

            do {
                if (empty())
                    return std::nullopt;
            } while (!mHead.compare_exchange_weak(index, (index + 1) % MODULO));

            index %= N;

            while (true) {
                State expected = VALID;

                if (mState[index].compare_exchange_weak(expected, TAKING))
                    break;
            }

            return index;
        }

        void release(size_t index) {
            mState[index] = IDLE;
        }

    public:
        T &operator[](size_t index) {
            return mBuffer[index];
        }

    public:
        [[nodiscard]] size_t size() const {
            return (mTail % N + N - mHead % N) % N;
        }

        [[nodiscard]] bool empty() const {
            return mHead == mTail;
        }

        [[nodiscard]] bool full() const {
            return (mTail + 1) % N == mHead % N;
        }

    private:
        T mBuffer[N];
        std::atomic<State> mState[N];

    private:
        std::atomic<size_t> mHead;
        std::atomic<size_t> mTail;
    };
}

#endif //ZERO_CIRCULAR_BUFFER_H
