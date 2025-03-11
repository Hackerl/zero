#ifndef ZERO_ATOMIC_EVENT_H
#define ZERO_ATOMIC_EVENT_H

#include <atomic>
#include <chrono>
#include <optional>
#include <expected>
#include <system_error>

namespace zero::atomic {
    class Event {
#ifdef __APPLE__
        using Value = std::uint64_t;
#else
        using Value = int;
#endif

    public:
        explicit Event(bool manual = false, bool initialState = false);

        std::expected<void, std::error_code> wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

        void set();
        void reset();

    private:
        bool mManual;
        std::atomic<Value> mState;
    };
}

#endif //ZERO_ATOMIC_EVENT_H
