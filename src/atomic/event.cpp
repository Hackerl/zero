#include <zero/atomic/event.h>
#include <linux/futex.h>
#include <unistd.h>
#include <syscall.h>
#include <cerrno>
#include <climits>

void zero::atomic::CEvent::wait(const timespec *timeout) {
    while (true) {
        if (__sync_bool_compare_and_swap(&mCore, 1, 0))
            break;

        if (syscall(SYS_futex, &mCore, FUTEX_WAIT, 0, timeout, nullptr, 0) < 0 && errno == ETIMEDOUT) {
            break;
        }
    }
}

void zero::atomic::CEvent::notify() {
    if (__sync_bool_compare_and_swap(&mCore, 0, 1)) {
        syscall(SYS_futex, &mCore, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }
}

void zero::atomic::CEvent::broadcast() {
    if (__sync_bool_compare_and_swap(&mCore, 0, 1)) {
        syscall(SYS_futex, &mCore, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
    }
}
