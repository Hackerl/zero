#ifndef ZERO_CIRCULAR_BUFFER_H
#define ZERO_CIRCULAR_BUFFER_H

#include <cstddef>
#include <atomic>
#include <optional>

#ifdef _WIN32
#include <Windows.h>
#endif

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
        std::optional<size_t> reserve() {
            size_t index = mTail;

            do {
                if (full())
                    return std::nullopt;
            } while (!mTail.compare_exchange_weak(index, (index + 1) % MODULO));

            index %= N;

#ifdef _WIN32
            while (InterlockedCompareExchange((LONG *) &mState[index], PUTTING, IDLE) != IDLE) {

            }
#elif __linux__
            while (!__sync_bool_compare_and_swap(&mState[index], IDLE, PUTTING)) {

            }
#endif

            return index;
        }

        void commit(size_t index) {
#ifdef _WIN32
            InterlockedExchange((LONG *) &mState[index], VALID);
#elif __linux__
            __atomic_store_n(&mState[index], VALID, __ATOMIC_SEQ_CST);
#endif
        }

    public:
        std::optional<size_t> acquire() {
            size_t index = mHead;

            do {
                if (empty())
                    return std::nullopt;
            } while (!mHead.compare_exchange_weak(index, (index + 1) % MODULO));

            index %= N;

#ifdef _WIN32
            while (InterlockedCompareExchange((LONG *) &mState[index], TAKING, VALID) != VALID) {

            }
#elif __linux__
            while (!__sync_bool_compare_and_swap(&mState[index], VALID, TAKING)) {

            }
#endif

            return index;
        }

        void release(size_t index) {
#ifdef _WIN32
            InterlockedExchange((LONG *) &mState[index], IDLE);
#elif __linux__
            __atomic_store_n(&mState[index], IDLE, __ATOMIC_SEQ_CST);
#endif
        }

    public:
        T &operator[](size_t index) {
            return mBuffer[index];
        }

    public:
        size_t size() {
            return (mTail % N + N - mHead % N) % N;
        }

        bool empty() {
            return mHead == mTail;
        }

        bool full() {
            return (mTail + 1) % N == mHead % N;
        }

    private:
        T mBuffer[N]{};
        State mState[N]{IDLE};

    private:
        std::atomic<size_t> mHead{};
        std::atomic<size_t> mTail{};
    };
}

#endif //ZERO_CIRCULAR_BUFFER_H
