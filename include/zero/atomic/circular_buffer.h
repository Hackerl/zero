#ifndef ZERO_CIRCULAR_BUFFER_H
#define ZERO_CIRCULAR_BUFFER_H

#include <limits>
#include <cstddef>
#include <atomic>
#include <memory>
#include <optional>
#include <cassert>

namespace zero::atomic {
    template<typename T>
    class CircularBuffer {
        enum class State {
            IDLE,
            PUTTING,
            VALID,
            TAKING
        };

    public:
        explicit CircularBuffer(const std::size_t capacity)
            : mModulo((std::numeric_limits<std::size_t>::max)() / capacity * capacity),
              mCapacity(capacity), mHead(0), mTail(0),
              mBuffer(std::make_unique<T[]>(capacity)), mState(std::make_unique<std::atomic<State>[]>(capacity)) {
            assert(mCapacity > 1);
        }

        std::optional<std::size_t> reserve() {
            std::size_t index = mTail;

            do {
                if (full())
                    return std::nullopt;
            }
            while (!mTail.compare_exchange_weak(index, (index + 1) % mModulo));

            index %= mCapacity;

            while (true) {
                if (State expected = State::IDLE; mState[index].compare_exchange_weak(expected, State::PUTTING))
                    break;
            }

            return index;
        }

        void commit(const std::size_t index) {
            mState[index] = State::VALID;
        }

        std::optional<std::size_t> acquire() {
            std::size_t index = mHead;

            do {
                if (empty())
                    return std::nullopt;
            }
            while (!mHead.compare_exchange_weak(index, (index + 1) % mModulo));

            index %= mCapacity;

            while (true) {
                if (State expected = State::VALID; mState[index].compare_exchange_weak(expected, State::TAKING))
                    break;
            }

            return index;
        }

        void release(const std::size_t index) {
            mState[index] = State::IDLE;
        }

        T &operator[](const std::size_t index) {
            return mBuffer[index];
        }

        [[nodiscard]] std::size_t size() const {
            return (mTail % mCapacity + mCapacity - mHead % mCapacity) % mCapacity;
        }

        [[nodiscard]] std::size_t capacity() const {
            return mCapacity;
        }

        [[nodiscard]] bool empty() const {
            return mHead == mTail;
        }

        [[nodiscard]] bool full() const {
            return (mTail + 1) % mCapacity == mHead % mCapacity;
        }

    private:
        std::size_t mModulo;
        std::size_t mCapacity;
        std::atomic<std::size_t> mHead;
        std::atomic<std::size_t> mTail;
        std::unique_ptr<T[]> mBuffer;
        std::unique_ptr<std::atomic<State>[]> mState;
    };
}

#endif //ZERO_CIRCULAR_BUFFER_H
