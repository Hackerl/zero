#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <atomic>
#include <chrono>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#endif

namespace zero::atomic {
    class Event {
    public:
        Event();
#ifdef _WIN32
        ~Event();
#endif

    public:
        void wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    public:
        void notify();
        void broadcast();

    protected:
#ifdef _WIN32
        HANDLE mEvent;
        std::atomic<long> mState;
#elif __linux__
        std::atomic<int> mState;
#endif
    };
}

#endif //ZERO_EVENT_H
