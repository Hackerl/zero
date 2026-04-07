#include <zero/async/promise.h>

std::shared_ptr<zero::async::promise::InlineExecutor> zero::async::promise::InlineExecutor::instance() {
    static const auto instance = std::make_shared<InlineExecutor>();
    return instance;
}

void zero::async::promise::InlineExecutor::post(const std::function<void()> f) {
    f();
}
