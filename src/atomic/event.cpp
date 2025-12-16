#include <zero/atomic/event.h>
#include <zero/defer.h>

#ifdef _WIN32
#include <zero/expect.h>
#include <zero/os/windows/error.h>
#elif defined(__linux__)
#include <climits>
#include <unistd.h>
#include <syscall.h>
#include <linux/futex.h>
#include <zero/error.h>
#include <zero/os/unix/error.h>
#elif defined(__APPLE__)
#include <zero/error.h>
#include <zero/expect.h>
#include <zero/os/unix/error.h>

constexpr auto ULF_WAKE_ALL = 0x00000100;
constexpr auto UL_COMPARE_AND_WAIT = 1;

extern "C" int __ulock_wait(uint32_t operation, void *addr, uint64_t value, uint32_t timeout);
extern "C" int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);
#endif

zero::atomic::Event::Event(const bool manual, const bool initialState) : mManual{manual}, mState(initialState ? 1 : 0) {
}

std::expected<void, std::error_code> zero::atomic::Event::wait(const std::optional<std::chrono::milliseconds> timeout) {
    while (true) {
        if (mManual) {
            if (mState)
                return {};
        }
        else {
            if (Value expected{1}; mState.compare_exchange_strong(expected, 0))
                return {};
        }

        ++mWaiterCount;
        Z_DEFER(--mWaiterCount);

#ifdef _WIN32
        Z_EXPECT(os::windows::expected([&] {
            Value expected{0};
            return WaitOnAddress(
                &mState,
                &expected,
                sizeof(Value),
                timeout ? static_cast<DWORD>(timeout->count()) : INFINITE
            );
        }));
#elif defined(__linux__)
        std::optional<timespec> ts;

        if (timeout)
            ts = {
                static_cast<decltype(timespec::tv_sec)>(timeout->count() / 1000),
                static_cast<decltype(timespec::tv_nsec)>(timeout->count() % 1000 * 1000000)
            };

        if (const auto result = os::unix::ensure([&] {
            return syscall(SYS_futex, &mState, FUTEX_WAIT, 0, ts ? &*ts : nullptr, nullptr, 0);
        }); !result && result.error() != std::errc::resource_unavailable_try_again)
            return std::unexpected{result.error()};
#elif defined(__APPLE__)
        Z_EXPECT(os::unix::ensure([&] {
            return __ulock_wait(
                UL_COMPARE_AND_WAIT,
                &mState,
                0,
                !timeout ? 0 : duration_cast<std::chrono::microseconds>(*timeout).count()
            );
        }));
#else
#error "unsupported platform"
#endif
    }
}

void zero::atomic::Event::set() {
    if (Value expected{0}; mState.compare_exchange_strong(expected, 1)) {
        if (mWaiterCount == 0)
            return;

#ifdef _WIN32
        WakeByAddressAll(&mState);
#elif defined(__linux__)
        error::guard(os::unix::expected([this] {
            return syscall(SYS_futex, &mState, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
        }));
#elif defined(__APPLE__)
        error::guard(os::unix::expected([this] {
            return __ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL, &mState, 0);
        }).or_else([](const auto &ec) -> std::expected<int, std::error_code> {
            if (ec != std::errc::no_such_file_or_directory)
                return std::unexpected{ec};

            return {};
        }));
#else
#error "unsupported platform"
#endif
    }
}

void zero::atomic::Event::reset() {
    mState = 0;
}

bool zero::atomic::Event::isSet() const {
    return mState == 1;
}
