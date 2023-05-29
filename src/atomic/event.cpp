#include <zero/atomic/event.h>

#if _WIN32 && !ZERO_LEGACY_NT
#include <windows.h>
#elif __linux__
#include <linux/futex.h>
#include <unistd.h>
#include <syscall.h>
#include <cerrno>
#include <climits>
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
        int expected = 1;

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
#endif
    }

    return result;
}

void zero::atomic::Event::notify() {
    int expected = 0;

    if (mState.compare_exchange_strong(expected, 1)) {
#ifdef ZERO_LEGACY_NT
        SetEvent(mEvent);
#elif _WIN32
        WakeByAddressSingle(&mState);
#elif __linux__
        syscall(SYS_futex, &mState, FUTEX_WAKE, 1, nullptr, nullptr, 0);
#endif
    }
}

void zero::atomic::Event::broadcast() {
    int expected = 0;

    if (mState.compare_exchange_strong(expected, 1)) {
#ifdef ZERO_LEGACY_NT
        PulseEvent(mEvent);
#elif _WIN32
        WakeByAddressAll(&mState);
#elif __linux__
        syscall(SYS_futex, &mState, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
#endif
    }
}