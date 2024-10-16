#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <chrono>
#include <optional>
#include <tl/expected.hpp>

#ifdef _WIN32
#include <windows.h>
#include <zero/error.h>
#else
#include <atomic>
#include <system_error>
#endif

namespace zero::atomic {
#ifdef _WIN32
    class Event {
    public:
        DEFINE_ERROR_CODE_EX(
            Error,
            "zero::atomic::Event",
            WAIT_EVENT_TIMEOUT, "wait event timeout", std::errc::timed_out
        )

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

#ifdef _WIN32
DECLARE_ERROR_CODE(zero::atomic::Event::Error)
#endif

#endif //ZERO_EVENT_H
