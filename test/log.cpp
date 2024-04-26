#include <zero/log.h>
#include <zero/atomic/event.h>
#include <catch2/catch_test_macros.hpp>
#include <bitset>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std::chrono_literals;

class Provider final : public zero::ILogProvider {
public:
    Provider(std::shared_ptr<std::bitset<4>> bitset, std::shared_ptr<zero::atomic::Event> event)
        : mBitset(std::move(bitset)), mEvent(std::move(event)) {
    }

    tl::expected<void, std::error_code> init() override {
        mBitset->set(0);
        return {};
    }

    tl::expected<void, std::error_code> rotate() override {
        mBitset->set(2);
        return {};
    }

    tl::expected<void, std::error_code> flush() override {
        mBitset->set(3);
        mEvent->set();
        return {};
    }

    tl::expected<void, std::error_code> write(const zero::LogMessage &message) override {
        if (message.content == "hello world")
            mBitset->set(1);

        return {};
    }

private:
    std::shared_ptr<std::bitset<4>> mBitset;
    std::shared_ptr<zero::atomic::Event> mEvent;
};

TEST_CASE("logging module", "[log]") {
    zero::Logger logger;

    const auto bitset = std::make_shared<std::bitset<4>>();
    const auto event = std::make_shared<zero::atomic::Event>();
    auto provider = std::make_unique<Provider>(bitset, event);

    const auto tp = std::chrono::system_clock::now();

    SECTION("enable") {
        REQUIRE(!logger.enabled(zero::INFO_LEVEL));
        logger.addProvider(zero::INFO_LEVEL, std::move(provider), 50ms);

        REQUIRE(logger.enabled(zero::INFO_LEVEL));
        logger.log(zero::INFO_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");

        REQUIRE(event->wait());
        REQUIRE(std::chrono::system_clock::now() - tp > 45ms);
        REQUIRE(bitset->test(0));
        REQUIRE(bitset->test(1));
        REQUIRE(bitset->test(2));
        REQUIRE(bitset->test(3));
    }

    SECTION("disable") {
        REQUIRE(!logger.enabled(zero::INFO_LEVEL));
        logger.addProvider(zero::ERROR_LEVEL, std::move(provider), 50ms);

        REQUIRE(!logger.enabled(zero::INFO_LEVEL));
        logger.log(zero::INFO_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");

        REQUIRE(event->wait());
        REQUIRE(std::chrono::system_clock::now() - tp > 45ms);
        REQUIRE(bitset->test(0));
        REQUIRE(!bitset->test(1));
        REQUIRE(!bitset->test(2));
        REQUIRE(bitset->test(3));
    }

    SECTION("override") {
        SECTION("enable") {
#ifdef _WIN32
            SetEnvironmentVariableA("ZERO_LOG_LEVEL", "3");
#else
            setenv("ZERO_LOG_LEVEL", "3", 0);
#endif
            REQUIRE(!logger.enabled(zero::DEBUG_LEVEL));
            logger.addProvider(zero::ERROR_LEVEL, std::move(provider), 50ms);

            REQUIRE(logger.enabled(zero::DEBUG_LEVEL));
            logger.log(zero::DEBUG_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");

            REQUIRE(event->wait());
            REQUIRE(std::chrono::system_clock::now() - tp > 45ms);
            REQUIRE(bitset->test(0));
            REQUIRE(bitset->test(1));
            REQUIRE(bitset->test(2));
            REQUIRE(bitset->test(3));
#ifdef _WIN32
            SetEnvironmentVariableA("ZERO_LOG_LEVEL", nullptr);
#else
            unsetenv("ZERO_LOG_LEVEL");
#endif
        }

        SECTION("disable") {
#ifdef _WIN32
            SetEnvironmentVariableA("ZERO_LOG_LEVEL", "2");
#else
            setenv("ZERO_LOG_LEVEL", "2", 0);
#endif
            REQUIRE(!logger.enabled(zero::INFO_LEVEL));
            logger.addProvider(zero::ERROR_LEVEL, std::move(provider), 50ms);

            REQUIRE(logger.enabled(zero::INFO_LEVEL));
            REQUIRE(!logger.enabled(zero::DEBUG_LEVEL));
            logger.log(zero::DEBUG_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");

            REQUIRE(event->wait());
            REQUIRE(std::chrono::system_clock::now() - tp > 45ms);
            REQUIRE(bitset->test(0));
            REQUIRE(!bitset->test(1));
            REQUIRE(!bitset->test(2));
            REQUIRE(bitset->test(3));
#ifdef _WIN32
            SetEnvironmentVariableA("ZERO_LOG_LEVEL", nullptr);
#else
            unsetenv("ZERO_LOG_LEVEL");
#endif
        }
    }
}
