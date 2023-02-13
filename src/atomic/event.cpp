#include <zero/atomic/event.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <linux/futex.h>
#include <unistd.h>
#include <syscall.h>
#include <cerrno>
#include <climits>
#endif

zero::atomic::Event::Event() : mState() {

}

void zero::atomic::Event::wait(std::optional<std::chrono::milliseconds> timeout) {
    while (true) {
#ifdef _WIN32
        LONG expected = 1;

        if (mState.compare_exchange_strong(expected, 0))
            break;

        if (!WaitOnAddress(&mState, &expected, sizeof(LONG), timeout ? (DWORD) timeout->count() : INFINITE) &&
            GetLastError() == ERROR_TIMEOUT)
            break;
#elif __linux__
        int expected = 1;

        if (mState.compare_exchange_strong(expected, 0))
            break;

        std::optional<timespec> ts;

        if (timeout)
            ts = {
                    (time_t) (timeout->count() / 1000),
                    (long) ((timeout->count() % 1000) * 1000000)
            };

        if (syscall(SYS_futex, &mState, FUTEX_WAIT, 0, ts ? &*ts : nullptr, nullptr, 0) < 0 && errno == ETIMEDOUT)
            break;
#endif
    }
}

void zero::atomic::Event::notify() {
#ifdef _WIN32
    LONG expected = 0;

    if (mState.compare_exchange_strong(expected, 1)) {
        WakeByAddressSingle(&mState);
    }
#elif __linux__
    int expected = 0;

    if (mState.compare_exchange_strong(expected, 1)) {
        syscall(SYS_futex, &mState, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }
#endif
}

void zero::atomic::Event::broadcast() {
#ifdef _WIN32
    LONG expected = 0;

    if (mState.compare_exchange_strong(expected, 1)) {
        WakeByAddressAll(&mState);
    }
#elif __linux__
    int expected = 0;

    if (mState.compare_exchange_strong(expected, 1)) {
        syscall(SYS_futex, &mState, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
    }
#endif
}