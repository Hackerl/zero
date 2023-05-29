#include <zero/log.h>
#include <catch2/catch_test_macros.hpp>

constexpr auto LOG_MAX_LENGTH = 100;

class TestProvider : public zero::ILogProvider {
public:
    TestProvider() : mLength() {

    }

public:
    bool init() override {
        return true;
    }

    bool rotate() override {
        mLength = 0;
        return true;
    }

    bool flush() override {
        return true;
    }

    zero::LogResult write(const zero::LogMessage &message) override {
        mLength += zero::stringify(message).length();

        if (mLength >= LOG_MAX_LENGTH)
            return zero::ROTATED;

        return zero::SUCCEEDED;
    }

private:
    std::atomic<size_t> mLength;
};

void generate() {
    for (int i = 0; i < 50000; i++)
        LOG_INFO("hello world");
}

TEST_CASE("logging module", "[log]") {
    GLOBAL_LOGGER->addProvider(
            zero::INFO_LEVEL,
            std::make_unique<TestProvider>()
    );

    std::list<std::thread> threads;

    for (int i = 0; i < 20; i++)
        threads.emplace_back(generate);

    for (auto &thread: threads)
        thread.join();
}
