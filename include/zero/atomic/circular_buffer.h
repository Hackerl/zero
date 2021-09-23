#ifndef ZERO_CIRCULAR_BUFFER_H
#define ZERO_CIRCULAR_BUFFER_H

#include <atomic>
#include <climits>

namespace zero {
    namespace atomic {
        template<typename T, unsigned long N>
        class CircularBuffer {
        private:
            static constexpr auto MODULO = ULONG_MAX - (ULONG_MAX % N);

            enum emState {
                IDLE,
                PUTTING,
                VALID,
                TAKING
            };

        public:
            bool enqueue(const T &item) {
                unsigned long index = mTail;

                do {
                    if (full())
                        return false;
                } while (!mTail.compare_exchange_weak(index, (index + 1) % MODULO));

                index %= N;

                while (!__sync_bool_compare_and_swap(&mState[index], IDLE, PUTTING)) {

                }

                mBuffer[index] = item;

                __atomic_store_n(&mState[index], VALID, __ATOMIC_SEQ_CST);

                return true;
            }

            bool dequeue(T &item) {
                unsigned long index = mHead;

                do {
                    if (empty())
                        return false;
                } while (!mHead.compare_exchange_weak(index, (index + 1) % MODULO));

                index %= N;

                while (!__sync_bool_compare_and_swap(&mState[index], VALID, TAKING)) {

                }

                item = mBuffer[index];

                __atomic_store_n(&mState[index], IDLE, __ATOMIC_SEQ_CST);

                return true;
            }

        public:
            unsigned long size() {
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
            emState mState[N]{IDLE};

        private:
            std::atomic<unsigned long> mHead{0};
            std::atomic<unsigned long> mTail{0};
        };
    }
}

#endif //ZERO_CIRCULAR_BUFFER_H
