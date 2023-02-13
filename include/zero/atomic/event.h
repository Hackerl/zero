#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <atomic>
#include <chrono>
#include <optional>

namespace zero::atomic {
    class Event {
    public:
        Event();

    public:
        void wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    public:
        void notify();
        void broadcast();

    protected:
#ifdef _WIN32
        std::atomic<long> mState;
#elif __linux__
        std::atomic<int> mState;
#endif
    };
}

#endif //ZERO_EVENT_H
