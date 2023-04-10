#include <zero/log.h>
#include <catch2/catch_test_macros.hpp>

constexpr auto LOG_MAX_LENGTH = 100;

class TestProvider : public zero::ILogProvider {
public:
    bool init() override {
        return true;
    }

    bool rotate() override {
        REQUIRE(mLength >= LOG_MAX_LENGTH);
        mLength = 0;
        return true;
    }

    zero::LogResult write(std::string_view message) override {
        mLength += message.length();

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
            std::make_unique<zero::AsyncProvider<TestProvider, 1000>>()
    );

    std::list<std::thread> threads;

    for (int i = 0; i < 20; i++)
        threads.emplace_back(generate);

    for (auto &thread: threads)
        thread.join();
}
