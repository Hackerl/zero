#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <atomic>
#include <chrono>
#include <optional>
#include <system_error>
#include <tl/expected.hpp>

#ifdef ZERO_LEGACY_NT
#include <windows.h>
#endif

namespace zero::atomic {
    class Event {
#ifdef __APPLE__
        using Value = std::uint64_t;
#else
        using Value = int;
#endif
    public:
        Event();
#ifdef ZERO_LEGACY_NT
        ~Event();
#endif

        tl::expected<void, std::error_code> wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

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
