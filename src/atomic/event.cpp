#include <zero/atomic/event.h>

#ifdef _WIN32
#include <zero/singleton.h>
#elif __linux__
#include <cerrno>
#include <climits>
#include <unistd.h>
#include <syscall.h>
#include <linux/futex.h>
#elif __APPLE__
#define ULF_WAKE_ALL 0x00000100
#define UL_COMPARE_AND_WAIT 1

extern "C" int __ulock_wait(uint32_t operation, void *addr, uint64_t value, uint32_t timeout);
extern "C" int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);
#endif

#ifdef _WIN32
const char *zero::atomic::Event::ErrorCategory::name() const noexcept {
    return "zero::atomic::Event";
}

std::string zero::atomic::Event::ErrorCategory::message(const int value) const {
    if (value == WAIT_EVENT_TIMEOUT)
        return "wait event timeout";

    return "unknown";
}

std::error_condition zero::atomic::Event::ErrorCategory::default_error_condition(const int value) const noexcept {
    if (value == WAIT_EVENT_TIMEOUT)
        return std::errc::timed_out;

    return error_category::default_error_condition(value);
}

std::error_code zero::atomic::make_error_code(const Event::Error e) {
    return {e, Singleton<Event::ErrorCategory>::getInstance()};
}

zero::atomic::Event::Event(const bool manual, const bool initialState) {
    mEvent = CreateEventA(nullptr, manual, initialState, nullptr);

    if (!mEvent)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
}

zero::atomic::Event::~Event() {
    CloseHandle(mEvent);
}

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<void, std::error_code> zero::atomic::Event::wait(const std::optional<std::chrono::milliseconds> timeout) {
    if (const DWORD rc = WaitForSingleObject(mEvent, timeout ? static_cast<DWORD>(timeout->count()) : INFINITE);
        rc != WAIT_OBJECT_0) {
        return tl::unexpected(
            rc == WAIT_TIMEOUT
                ? make_error_code(WAIT_EVENT_TIMEOUT)
                : std::error_code(static_cast<int>(GetLastError()), std::system_category())
        );
    }

    return {};
}

// ReSharper disable once CppMemberFunctionMayBeConst
void zero::atomic::Event::set() {
    SetEvent(mEvent);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void zero::atomic::Event::reset() {
    ResetEvent(mEvent);
}
#else
zero::atomic::Event::Event(const bool manual, const bool initialState) : mManual(manual), mState(initialState ? 1 : 0) {
}

tl::expected<void, std::error_code> zero::atomic::Event::wait(const std::optional<std::chrono::milliseconds> timeout) {
    tl::expected<void, std::error_code> result;

    while (true) {
        if (mManual) {
            if (mState)
                break;
        }
        else {
            if (Value expected = 1; mState.compare_exchange_strong(expected, 0))
                break;
        }
#ifdef __linux__
        std::optional<timespec> ts;

        if (timeout)
            ts = {
                timeout->count() / 1000,
                timeout->count() % 1000 * 1000000
            };

        if (syscall(SYS_futex, &mState, FUTEX_WAIT, 0, ts ? &*ts : nullptr, nullptr, 0) < 0) {
            if (errno == EAGAIN)
                continue;

            result = tl::unexpected<std::error_code>(errno, std::system_category());
            break;
        }
#elif __APPLE__
        if (__ulock_wait(
            UL_COMPARE_AND_WAIT,
            &mState,
            0,
            !timeout ? 0 : std::chrono::duration_cast<std::chrono::microseconds>(*timeout).count()
        ) < 0) {
            result = tl::unexpected<std::error_code>(errno, std::system_category());
            break;
        }
#else
#error "unsupported platform"
#endif
    }

    return result;
}

void zero::atomic::Event::set() {
    if (Value expected = 0; mState.compare_exchange_strong(expected, 1)) {
#ifdef __linux__
        syscall(SYS_futex, &mState, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
#elif __APPLE__
        __ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL, &mState, 0);
#else
#error "unsupported platform"
#endif
    }
}

void zero::atomic::Event::reset() {
    mState = 0;
}
#endif
