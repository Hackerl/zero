#include <zero/atomic/event.h>

#ifdef __linux__
#include <linux/futex.h>
#include <unistd.h>
#include <syscall.h>
#include <cerrno>
#include <climits>
#endif

#ifdef _WIN32
zero::atomic::CEvent::CEvent() {
    mMEvent = CreateEvent(nullptr, true, false, nullptr);
    mAEvent = CreateEvent(nullptr, false, false, nullptr);
}

zero::atomic::CEvent::~CEvent() {
    CloseHandle(mMEvent);
    CloseHandle(mAEvent);
}

void zero::atomic::CEvent::wait(const int *timeout) {
    while (true) {
        if (InterlockedCompareExchange(&mCore, 0, 1) == 1)
            break;

        HANDLE objects[2] = {mMEvent, mAEvent};

        if (WaitForMultipleObjects(2, objects, false, timeout ? *timeout * 1000 : INFINITE) == WAIT_TIMEOUT)
            break;
    }
}

void zero::atomic::CEvent::notify() {
    if (InterlockedCompareExchange(&mCore, 1, 0) == 0) {
        SetEvent(mAEvent);
    }
}

void zero::atomic::CEvent::broadcast() {
    if (InterlockedCompareExchange(&mCore, 1, 0) == 0) {
        PulseEvent(mMEvent);
    }
}

#elif __linux__
void zero::atomic::CEvent::wait(const int *timeout) {
    while (true) {
        if (__sync_bool_compare_and_swap(&mCore, 1, 0))
            break;

        if (syscall(SYS_futex, &mCore, FUTEX_WAIT, 0, timeout, nullptr, 0) < 0 && errno == ETIMEDOUT)
            break;
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

#endif
