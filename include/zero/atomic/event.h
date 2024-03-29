#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <chrono>
#include <optional>
#include <system_error>
#include <tl/expected.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <atomic>
#endif

namespace zero::atomic {
#ifdef _WIN32
    class Event {
    public:
        explicit Event(bool manual = false, bool initialState = false);
        Event(const Event &) = delete;
        Event &operator=(const Event &) = delete;
        ~Event();

        tl::expected<void, std::error_code> wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

        void set();
        void reset();

    private:
        HANDLE mEvent;
    };
#else
    class Event {
#ifdef __APPLE__
        using Value = std::uint64_t;
#else
        using Value = int;
#endif

    public:
        explicit Event(bool manual = false, bool initialState = false);

        tl::expected<void, std::error_code> wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

        void set();
        void reset();

    private:
        bool mManual;
        std::atomic<Value> mState;
    };
#endif
}

#endif //ZERO_EVENT_H
