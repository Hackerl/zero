#include <zero/atomic/event.h>

#ifdef __linux__
#include <linux/futex.h>
#include <unistd.h>
#include <syscall.h>
#include <cerrno>
#include <climits>
#endif

#ifdef _WIN32
zero::atomic::Event::Event() {
    mMEvent = CreateEvent(nullptr, true, false, nullptr);
    mAEvent = CreateEvent(nullptr, false, false, nullptr);
}

zero::atomic::Event::~Event() {
    CloseHandle(mMEvent);
    CloseHandle(mAEvent);
}

void zero::atomic::Event::wait(const std::chrono::seconds &timeout) {
    while (true) {
        if (InterlockedCompareExchange(&mState, 0, 1) == 1)
            break;

        HANDLE objects[2] = {mMEvent, mAEvent};
        DWORD milliseconds = timeout != std::chrono::seconds::zero() ? (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count() : INFINITE;

        if (WaitForMultipleObjects(2, objects, false, milliseconds) == WAIT_TIMEOUT)
            break;
    }
}

void zero::atomic::Event::notify() {
    if (InterlockedCompareExchange(&mState, 1, 0) == 0) {
        SetEvent(mAEvent);
    }
}

void zero::atomic::Event::broadcast() {
    if (InterlockedCompareExchange(&mState, 1, 0) == 0) {
        PulseEvent(mMEvent);
    }
}

#elif __linux__
void zero::atomic::Event::wait(const std::chrono::seconds &timeout) {
    while (true) {
        if (__sync_bool_compare_and_swap(&mState, 1, 0))
            break;

        timespec t = {timeout.count()};

        if (syscall(SYS_futex, &mState, FUTEX_WAIT, 0, timeout !=  std::chrono::seconds::zero() ? &t : nullptr, nullptr, 0) < 0 && errno == ETIMEDOUT)
            break;
    }
}

void zero::atomic::Event::notify() {
    if (__sync_bool_compare_and_swap(&mState, 0, 1)) {
        syscall(SYS_futex, &mState, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }
}

void zero::atomic::Event::broadcast() {
    if (__sync_bool_compare_and_swap(&mState, 0, 1)) {
        syscall(SYS_futex, &mState, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
    }
}

#endif
