#include <zero/async/coroutine.h>
#include <zero/singleton.h>

const char *zero::async::coroutine::ErrorCategory::name() const noexcept {
    return "zero::async::coroutine";
}

std::string zero::async::coroutine::ErrorCategory::message(const int value) const {
    std::string msg;

    switch (value) {
    case CANCELLED:
        msg = "task has been cancelled";
        break;

    case CANCELLATION_NOT_SUPPORTED:
        msg = "task does not support cancellation";
        break;

    case LOCKED:
        msg = "task has been locked";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

std::error_condition zero::async::coroutine::ErrorCategory::default_error_condition(const int value) const noexcept {
    std::error_condition condition;

    switch (value) {
    case CANCELLED:
        condition = std::errc::operation_canceled;
        break;

    case CANCELLATION_NOT_SUPPORTED:
        condition = std::errc::operation_not_supported;
        break;

    case LOCKED:
        condition = std::errc::resource_unavailable_try_again;
        break;

    default:
        condition = error_category::default_error_condition(value);
        break;
    }

    return condition;
}

std::error_code zero::async::coroutine::make_error_code(const Error e) {
    return {static_cast<int>(e), Singleton<ErrorCategory>::getInstance()};
}