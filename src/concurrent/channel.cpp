#include <zero/concurrent/channel.h>
#include <zero/singleton.h>

const char *zero::concurrent::ChannelErrorCategory::name() const noexcept {
    return "zero::concurrent::channel";
}

std::string zero::concurrent::ChannelErrorCategory::message(const int value) const {
    if (static_cast<ChannelError>(value) == ChannelError::DISCONNECTED)
        return "channel disconnected";

    return "unknown";
}

bool zero::concurrent::ChannelErrorCategory::equivalent(const std::error_code &code, const int value) const noexcept {
    if (static_cast<ChannelError>(value) == ChannelError::DISCONNECTED)
        return code == TrySendError::DISCONNECTED ||
            code == SendError::DISCONNECTED ||
            code == TryReceiveError::DISCONNECTED ||
            code == ReceiveError::DISCONNECTED;

    return error_category::equivalent(code, value);
}

std::error_condition zero::concurrent::make_error_condition(const ChannelError e) {
    return {static_cast<int>(e), Singleton<ChannelErrorCategory>::getInstance()};
}

const char *zero::concurrent::TrySendErrorCategory::name() const noexcept {
    return "zero::concurrent::Sender::trySend";
}

std::string zero::concurrent::TrySendErrorCategory::message(const int value) const {
    std::string msg;

    switch (static_cast<TrySendError>(value)) {
    case TrySendError::DISCONNECTED:
        msg = "sending on a disconnected channel";
        break;

    case TrySendError::FULL:
        msg = "sending on a full channel";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

std::error_condition zero::concurrent::TrySendErrorCategory::default_error_condition(const int value) const noexcept {
    if (static_cast<TrySendError>(value) == TrySendError::FULL)
        return std::errc::operation_would_block;

    return error_category::default_error_condition(value);
}

std::error_code zero::concurrent::make_error_code(const TrySendError e) {
    return {static_cast<int>(e), Singleton<TrySendErrorCategory>::getInstance()};
}

const char *zero::concurrent::SendErrorCategory::name() const noexcept {
    return "zero::concurrent::Sender::send";
}

std::string zero::concurrent::SendErrorCategory::message(const int value) const {
    std::string msg;

    switch (static_cast<SendError>(value)) {
    case SendError::DISCONNECTED:
        msg = "sending on a disconnected channel";
        break;

    case SendError::TIMEOUT:
        msg = "timed out waiting on send operation";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

std::error_condition zero::concurrent::SendErrorCategory::default_error_condition(const int value) const noexcept {
    if (static_cast<SendError>(value) == SendError::TIMEOUT)
        return std::errc::timed_out;

    return error_category::default_error_condition(value);
}

std::error_code zero::concurrent::make_error_code(const SendError e) {
    return {static_cast<int>(e), Singleton<SendErrorCategory>::getInstance()};
}

const char *zero::concurrent::TryReceiveErrorCategory::name() const noexcept {
    return "zero::concurrent::Receiver::tryReceive";
}

std::string zero::concurrent::TryReceiveErrorCategory::message(const int value) const {
    std::string msg;

    switch (static_cast<TryReceiveError>(value)) {
    case TryReceiveError::DISCONNECTED:
        msg = "receiving on an empty and disconnected channel";
        break;

    case TryReceiveError::EMPTY:
        msg = "receiving on an empty channel";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

std::error_condition
zero::concurrent::TryReceiveErrorCategory::default_error_condition(const int value) const noexcept {
    if (static_cast<TryReceiveError>(value) == TryReceiveError::EMPTY)
        return std::errc::operation_would_block;

    return error_category::default_error_condition(value);
}

std::error_code zero::concurrent::make_error_code(const TryReceiveError e) {
    return {static_cast<int>(e), Singleton<TryReceiveErrorCategory>::getInstance()};
}

const char *zero::concurrent::ReceiveErrorCategory::name() const noexcept {
    return "zero::concurrent::Receiver::receive";
}

std::string zero::concurrent::ReceiveErrorCategory::message(const int value) const {
    std::string msg;

    switch (static_cast<ReceiveError>(value)) {
    case ReceiveError::DISCONNECTED:
        msg = "channel is empty and disconnected";
        break;

    case ReceiveError::TIMEOUT:
        msg = "timed out waiting on receive operation";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

std::error_condition zero::concurrent::ReceiveErrorCategory::default_error_condition(const int value) const noexcept {
    if (static_cast<ReceiveError>(value) == ReceiveError::TIMEOUT)
        return std::errc::timed_out;

    return error_category::default_error_condition(value);
}

std::error_code zero::concurrent::make_error_code(const ReceiveError e) {
    return {static_cast<int>(e), Singleton<ReceiveErrorCategory>::getInstance()};
}
