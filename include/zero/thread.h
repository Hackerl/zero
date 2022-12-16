#ifndef ZERO_THREAD_H
#define ZERO_THREAD_H

#include <thread>

namespace zero {
    template<typename T>
    class Thread {
    public:
        explicit Thread(T *that) : mThat(that), mThread(nullptr) {

        }

    public:
        template<typename F, typename... Args>
        bool start(F &&f, Args &&... args) {
            if (mThread)
                return false;

            mThread = new std::thread(f, mThat, args...);

            return mThread != nullptr;
        }

        bool stop() {
            if (!mThread)
                return false;

            if (mThread->joinable())
                mThread->join();

            delete mThread;
            mThread = nullptr;

            return true;
        }

    private:
        T *mThat;
        std::thread *mThread;
    };
}

#endif //ZERO_THREAD_H
