#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <chrono>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace zero::atomic {
    class Event {
    public:
        void wait(std::chrono::seconds timeout = std::chrono::seconds::zero());

    public:
        void notify();
        void broadcast();

    private:
#ifdef _WIN32
        LONG mState{};
#elif __linux__
        int mState{};
#endif
    };
}

#endif //ZERO_EVENT_H
