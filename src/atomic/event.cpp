#include <zero/atomic/event.h>

#if _WIN32 && !ZERO_LEGACY_NT
#include <windows.h>
#elif __linux__
#include <linux/futex.h>
#include <unistd.h>
#include <syscall.h>
#include <cerrno>
#include <climits>
#elif __APPLE__
#define ULF_WAKE_ALL 0x00000100
#define UL_COMPARE_AND_WAIT 1

extern "C" int __ulock_wait(uint32_t operation, void *addr, uint64_t value, uint32_t timeout);
extern "C" int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);
#endif

zero::atomic::Event::Event() : mState() {
#ifdef ZERO_LEGACY_NT
    mEvent = CreateEventA(nullptr, false, false, nullptr);
#endif
}

#ifdef ZERO_LEGACY_NT
zero::atomic::Event::~Event() {
    CloseHandle(mEvent);
}
#endif

nonstd::expected<void, zero::atomic::Event::Error>
zero::atomic::Event::wait(std::optional<std::chrono::milliseconds> timeout) {
    nonstd::expected<void, zero::atomic::Event::Error> result;

    while (true) {
        Value expected = 1;

        if (mState.compare_exchange_strong(expected, 0))
            break;

#ifdef ZERO_LEGACY_NT
        DWORD rc = WaitForSingleObject(mEvent, timeout ? (DWORD) timeout->count() : INFINITE);

        if (rc != WAIT_OBJECT_0) {
            result = nonstd::make_unexpected(rc == WAIT_TIMEOUT ? TIMEOUT : UNEXPECTED);
            break;
        }
#elif _WIN32
        if (!WaitOnAddress(&mState, &expected, sizeof(int), timeout ? (DWORD) timeout->count() : INFINITE)) {
            result = nonstd::make_unexpected(GetLastError() == ERROR_TIMEOUT ? TIMEOUT : UNEXPECTED);
            break;
        }
#elif __linux__
        std::optional<timespec> ts;

        if (timeout)
            ts = {
                    (time_t) (timeout->count() / 1000),
                    (long) ((timeout->count() % 1000) * 1000000)
            };

        if (syscall(SYS_futex, &mState, FUTEX_WAIT, 0, ts ? &*ts : nullptr, nullptr, 0) < 0) {
            if (errno == EAGAIN)
                continue;

            result = nonstd::make_unexpected(errno == ETIMEDOUT ? TIMEOUT : UNEXPECTED);
            break;
        }
#elif __APPLE__
        if (__ulock_wait(
                UL_COMPARE_AND_WAIT,
                &mState,
                0,
                !timeout ? 0 : std::chrono::duration_cast<std::chrono::microseconds>(*timeout).count()
        ) < 0) {
            result = nonstd::make_unexpected(errno == ETIMEDOUT ? TIMEOUT : UNEXPECTED);
            break;
        }
#else
#error "unsupported platform"
#endif
    }

    return result;
}

void zero::atomic::Event::notify() {
    Value expected = 0;

    if (mState.compare_exchange_strong(expected, 1)) {
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
    Value expected = 0;

    if (mState.compare_exchange_strong(expected, 1)) {
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