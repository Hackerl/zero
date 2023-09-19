#include <zero/concurrent/channel.h>

const char *zero::concurrent::Category::name() const noexcept {
    return "zero::atomic::channel";
}

std::string zero::concurrent::Category::message(int value) const {
    std::string msg;

    if (value == CHANNEL_EOF)
        return "channel eof";

    return "unknown";
}

const std::error_category &zero::concurrent::category() {
    static Category instance;
    return instance;
}

std::error_code zero::concurrent::make_error_code(Error e) {
    return {static_cast<int>(e), category()};
}
