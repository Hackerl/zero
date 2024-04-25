#include <zero/concurrent/channel.h>
#include <zero/singleton.h>

const char *zero::concurrent::ChannelErrorCategory::name() const noexcept {
    return "zero::concurrent::Channel";
}

std::string zero::concurrent::ChannelErrorCategory::message(const int value) const {
    std::string msg;

    switch (value) {
    case CHANNEL_EOF:
        msg = "channel eof";
        break;

    case BROKEN_CHANNEL:
        msg = "broken channel";
        break;

    case SEND_TIMEOUT:
        msg = "channel send timeout";
        break;

    case RECEIVE_TIMEOUT:
        msg = "channel receive timeout";
        break;

    case EMPTY:
        msg = "channel empty";
        break;

    case FULL:
        msg = "channel full";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

std::error_condition zero::concurrent::ChannelErrorCategory::default_error_condition(const int value) const noexcept {
    std::error_condition condition;

    switch (value) {
    case BROKEN_CHANNEL:
        condition = std::errc::broken_pipe;
        break;

    case SEND_TIMEOUT:
    case RECEIVE_TIMEOUT:
        condition = std::errc::timed_out;
        break;

    case EMPTY:
    case FULL:
        condition = std::errc::operation_would_block;
        break;

    default:
        condition = error_category::default_error_condition(value);
        break;
    }

    return condition;
}

std::error_code zero::concurrent::make_error_code(const ChannelError e) {
    return {static_cast<int>(e), Singleton<ChannelErrorCategory>::getInstance()};
}
