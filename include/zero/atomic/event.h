#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <atomic>
#include <chrono>
#include <optional>
#include <nonstd/expected.hpp>

#ifdef ZERO_LEGACY_NT
#include <windows.h>
#endif

namespace zero::atomic {
    class Event {
#ifdef __APPLE__
        using Value = uint64_t;
#else
        using Value = int;
#endif
    public:
        enum Error {
            TIMEOUT,
            UNEXPECTED
        };

    public:
        Event();
#ifdef ZERO_LEGACY_NT
        ~Event();
#endif

    public:
        nonstd::expected<void, Error> wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    public:
        void notify();
        void broadcast();

    private:
#ifdef ZERO_LEGACY_NT
        HANDLE mEvent;
#endif
        std::atomic<Value> mState;
    };
}

#endif //ZERO_EVENT_H
