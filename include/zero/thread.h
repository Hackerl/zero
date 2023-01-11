#ifndef ZERO_THREAD_H
#define ZERO_THREAD_H

#include <thread>
#include <memory.h>

namespace zero {
    template<typename T>
    class Thread {
    public:
        explicit Thread(T *that) : mThat(that) {

        }

        ~Thread() {
            if (!mThread)
                return;

            if (mThread->joinable())
                mThread->join();

            mThread.reset();
        }

    public:
        template<typename F, typename... Args>
        bool start(F &&f, Args &&... args) {
            if (mThread)
                return false;

            mThread = std::make_unique<std::thread>(f, mThat, args...);

            return true;
        }

        bool stop() {
            if (!mThread)
                return false;

            if (mThread->joinable())
                mThread->join();

            mThread.reset();

            return true;
        }

    private:
        T *mThat;
        std::unique_ptr<std::thread> mThread;
    };
}

#endif //ZERO_THREAD_H
