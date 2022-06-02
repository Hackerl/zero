#ifndef ZERO_CIRCULAR_BUFFER_H
#define ZERO_CIRCULAR_BUFFER_H

#include <atomic>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace zero::atomic {
    template<typename T, size_t N>
    class CircularBuffer {
    private:
        static constexpr auto MODULO = SIZE_MAX - (SIZE_MAX % N);

        enum State : long {
            IDLE,
            PUTTING,
            VALID,
            TAKING
        };

    public:
        bool enqueue(const T &item) {
            size_t index = mTail;

            do {
                if (full())
                    return false;
            } while (!mTail.compare_exchange_weak(index, (index + 1) % MODULO));

            index %= N;

#ifdef _WIN32
            while (InterlockedCompareExchange((LONG *)&mState[index], PUTTING, IDLE) != IDLE) {

            }

            mBuffer[index] = item;

            InterlockedExchange((LONG *)&mState[index], VALID);
#elif __linux__
            while (!__sync_bool_compare_and_swap(&mState[index], IDLE, PUTTING)) {

            }

            mBuffer[index] = item;

            __atomic_store_n(&mState[index], VALID, __ATOMIC_SEQ_CST);
#endif

            return true;
        }

        bool dequeue(T &item) {
            size_t index = mHead;

            do {
                if (empty())
                    return false;
            } while (!mHead.compare_exchange_weak(index, (index + 1) % MODULO));

            index %= N;

#ifdef _WIN32
            while (InterlockedCompareExchange((LONG *)&mState[index], TAKING, VALID) != VALID) {

            }

            item = mBuffer[index];

            InterlockedExchange((LONG *)&mState[index], IDLE);
#elif __linux__
            while (!__sync_bool_compare_and_swap(&mState[index], VALID, TAKING)) {

            }

            item = mBuffer[index];

            __atomic_store_n(&mState[index], IDLE, __ATOMIC_SEQ_CST);
#endif

            return true;
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
        std::atomic<size_t> mHead{0};
        std::atomic<size_t> mTail{0};
    };
}

#endif //ZERO_CIRCULAR_BUFFER_H
