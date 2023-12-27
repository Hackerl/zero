#include <zero/atomic/event.h>

#if _WIN32 && !ZERO_LEGACY_NT
#include <windows.h>
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

zero::atomic::Event::Event() : mState(0) {
#ifdef ZERO_LEGACY_NT
    mEvent = CreateEventA(nullptr, false, false, nullptr);
#endif
}

#ifdef ZERO_LEGACY_NT
zero::atomic::Event::~Event() {
    CloseHandle(mEvent);
}
#endif

tl::expected<void, std::error_code> zero::atomic::Event::wait(std::optional<std::chrono::milliseconds> timeout) {
    tl::expected<void, std::error_code> result;

    while (true) {
        if (Value expected = 1; mState.compare_exchange_strong(expected, 0))
            break;

#ifdef ZERO_LEGACY_NT
        if (const DWORD rc = WaitForSingleObject(mEvent, timeout ? static_cast<DWORD>(timeout->count()) : INFINITE);
            rc != WAIT_OBJECT_0) {
            result = tl::unexpected(
                rc == WAIT_TIMEOUT
                    ? make_error_code(std::errc::timed_out)
                    : std::error_code(static_cast<int>(GetLastError()), std::system_category())
            );
            break;
        }
#elif _WIN32
        Value expected = 0;

        if (!WaitOnAddress(
            &mState,
            &expected,
            sizeof(int),
            timeout ? static_cast<DWORD>(timeout->count()) : INFINITE
        )) {
            result = tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));
            break;
        }
#elif __linux__
        std::optional<timespec> ts;

        if (timeout)
            ts = {
                timeout->count() / 1000,
                timeout->count() % 1000 * 1000000
            };

        if (syscall(SYS_futex, &mState, FUTEX_WAIT, 0, ts ? &*ts : nullptr, nullptr, 0) < 0) {
            if (errno == EAGAIN)
                continue;

            result = tl::unexpected(std::error_code(errno, std::system_category()));
            break;
        }
#elif __APPLE__
        if (__ulock_wait(
            UL_COMPARE_AND_WAIT,
            &mState,
            0,
            !timeout ? 0 : std::chrono::duration_cast<std::chrono::microseconds>(*timeout).count()
        ) < 0) {
            result = tl::unexpected(std::error_code(errno, std::system_category()));
            break;
        }
#else
#error "unsupported platform"
#endif
    }

    return result;
}

void zero::atomic::Event::notify() {
    if (Value expected = 0; mState.compare_exchange_strong(expected, 1)) {
#ifdef ZERO_LEGACY_NT
        SetEvent(mEvent);
#elif _WIN32
        WakeByAddressSingle(&mState);
#elif __linux__
        syscall(SYS_futex, &mState, FUTEX_WAKE, 1, nullptr, nullptr, 0);
#elif __APPLE__
        __ulock_wake(UL_COMPARE_AND_WAIT, &mState, 0);
#else
#error "unsupported platform"
#endif
    }
}

void zero::atomic::Event::broadcast() {
    if (Value expected = 0; mState.compare_exchange_strong(expected, 1)) {
#ifdef ZERO_LEGACY_NT
        PulseEvent(mEvent);
#elif _WIN32
        WakeByAddressAll(&mState);
#elif __linux__
        syscall(SYS_futex, &mState, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
#elif __APPLE__
        __ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL, &mState, 0);
#else
#error "unsupported platform"
#endif
    }
}
