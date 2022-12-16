#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <chrono>
#include <optional>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace zero::atomic {
    class Event {
    public:
        void wait(std::optional<std::chrono::milliseconds> timeout = {});

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
