#ifndef ZERO_CIRCULAR_BUFFER_H
#define ZERO_CIRCULAR_BUFFER_H

#include <cstddef>
#include <atomic>
#include <memory>
#include <optional>
#include <cassert>

namespace zero::atomic {
    template<typename T>
    class CircularBuffer {
    private:
        enum State {
            IDLE,
            PUTTING,
            VALID,
            TAKING
        };

    public:
        explicit CircularBuffer(size_t capacity)
                : mHead(0), mTail(0), mCapacity(capacity), mModulo(SIZE_MAX / capacity * capacity),
                  mBuffer(std::make_unique<T[]>(capacity)), mState(std::make_unique<std::atomic<State>[]>(capacity)) {
            assert(mCapacity > 1);
        }

    public:
        std::optional<size_t> reserve() {
            size_t index = mTail;

            do {
                if (full())
                    return std::nullopt;
            } while (!mTail.compare_exchange_weak(index, (index + 1) % mModulo));

            index %= mCapacity;

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
            } while (!mHead.compare_exchange_weak(index, (index + 1) % mModulo));

            index %= mCapacity;

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
            return (mTail % mCapacity + mCapacity - mHead % mCapacity) % mCapacity;
        }

        [[nodiscard]] size_t capacity() const {
            return mCapacity;
        }

        [[nodiscard]] bool empty() const {
            return mHead == mTail;
        }

        [[nodiscard]] bool full() const {
            return (mTail + 1) % mCapacity == mHead % mCapacity;
        }

    private:
        size_t mModulo;
        size_t mCapacity;
        std::atomic<size_t> mHead;
        std::atomic<size_t> mTail;
        std::unique_ptr<T[]> mBuffer;
        std::unique_ptr<std::atomic<State>[]> mState;
    };
}

#endif //ZERO_CIRCULAR_BUFFER_H
