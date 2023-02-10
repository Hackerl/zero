#include <zero/atomic/event.h>

#ifdef __linux__
#include <linux/futex.h>
#include <unistd.h>
#include <syscall.h>
#include <cerrno>
#include <climits>
#endif

void zero::atomic::Event::wait(std::optional<std::chrono::milliseconds> timeout) {
    while (true) {
#ifdef _WIN32
        LONG state = InterlockedCompareExchange(&mState, 0, 1);

        if (state == 1)
            break;

        if (!timeout) {
            WaitOnAddress(&mState, &state, sizeof(LONG), INFINITE);
            continue;
        }

        if (!WaitOnAddress(&mState, &state, sizeof(LONG), (DWORD) timeout->count()) && GetLastError() == ERROR_TIMEOUT)
            break;
#elif __linux__
        if (__sync_bool_compare_and_swap(&mState, 1, 0))
            break;

        if (!timeout) {
            syscall(SYS_futex, &mState, FUTEX_WAIT, 0, nullptr, nullptr, 0);
            continue;
        }

        timespec ts = {
                (time_t) (timeout->count() / 1000),
                (long) ((timeout->count() % 1000) * 1000000)
        };

        if (syscall(SYS_futex, &mState, FUTEX_WAIT, 0, &ts, nullptr, 0) < 0 && errno == ETIMEDOUT)
            break;
#endif
    }
}

void zero::atomic::Event::notify() {
#ifdef _WIN32
    if (InterlockedCompareExchange(&mState, 1, 0) == 0) {
        WakeByAddressSingle(&mState);
    }
#elif __linux__
    if (__sync_bool_compare_and_swap(&mState, 0, 1)) {
        syscall(SYS_futex, &mState, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }
#endif
}

void zero::atomic::Event::broadcast() {
#ifdef _WIN32
    if (InterlockedCompareExchange(&mState, 1, 0) == 0) {
        WakeByAddressAll(&mState);
    }
#elif __linux__
    if (__sync_bool_compare_and_swap(&mState, 0, 1)) {
        syscall(SYS_futex, &mState, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
    }
#endif
}
