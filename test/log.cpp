#include <zero/log.h>
#include <zero/env.h>
#include <zero/atomic/event.h>
#include <zero/filesystem/fs.h>
#include <zero/filesystem/std.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <bitset>

class Provider final : public zero::log::IProvider {
public:
    Provider(std::shared_ptr<std::bitset<4>> bitset, std::shared_ptr<zero::atomic::Event> event)
        : mBitset{std::move(bitset)}, mEvent{std::move(event)} {
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

    tl::expected<void, std::error_code> write(const zero::log::Record &record) override {
        if (record.content == "hello world")
            mBitset->set(1);

        return {};
    }

private:
    std::shared_ptr<std::bitset<4>> mBitset;
    std::shared_ptr<zero::atomic::Event> mEvent;
};

TEST_CASE("logging module", "[log]") {
    SECTION("logger") {
        zero::log::Logger logger;

        const auto bitset = std::make_shared<std::bitset<4>>();
        const auto event = std::make_shared<zero::atomic::Event>();
        auto provider = std::make_unique<Provider>(bitset, event);

        const auto tp = std::chrono::system_clock::now();

        SECTION("enable") {
            using namespace std::chrono_literals;

            REQUIRE_FALSE(logger.enabled(zero::log::Level::INFO_LEVEL));
            logger.addProvider(zero::log::Level::INFO_LEVEL, std::move(provider), 50ms);

            REQUIRE(logger.enabled(zero::log::Level::INFO_LEVEL));
            logger.log(zero::log::Level::INFO_LEVEL, zero::log::sourceFilename(__FILE__), __LINE__, "hello world");

            REQUIRE(event->wait());
            REQUIRE(std::chrono::system_clock::now() - tp > 45ms);
            REQUIRE(bitset->test(0));
            REQUIRE(bitset->test(1));
            REQUIRE(bitset->test(2));
            REQUIRE(bitset->test(3));
        }

        SECTION("disable") {
            using namespace std::chrono_literals;

            REQUIRE_FALSE(logger.enabled(zero::log::Level::INFO_LEVEL));
            logger.addProvider(zero::log::Level::ERROR_LEVEL, std::move(provider), 50ms);

            REQUIRE_FALSE(logger.enabled(zero::log::Level::INFO_LEVEL));
            logger.log(zero::log::Level::INFO_LEVEL, zero::log::sourceFilename(__FILE__), __LINE__, "hello world");

            REQUIRE(event->wait());
            REQUIRE(std::chrono::system_clock::now() - tp > 45ms);
            REQUIRE(bitset->test(0));
            REQUIRE_FALSE(bitset->test(1));
            REQUIRE_FALSE(bitset->test(2));
            REQUIRE(bitset->test(3));
        }

        SECTION("override") {
            SECTION("enable") {
                using namespace std::chrono_literals;

                REQUIRE(zero::env::set("ZERO_LOG_LEVEL", "3"));

                REQUIRE_FALSE(logger.enabled(zero::log::Level::DEBUG_LEVEL));
                logger.addProvider(zero::log::Level::ERROR_LEVEL, std::move(provider), 50ms);

                REQUIRE(logger.enabled(zero::log::Level::DEBUG_LEVEL));
                logger.log(zero::log::Level::DEBUG_LEVEL, zero::log::sourceFilename(__FILE__), __LINE__, "hello world");

                REQUIRE(event->wait());
                REQUIRE(std::chrono::system_clock::now() - tp > 45ms);
                REQUIRE(bitset->test(0));
                REQUIRE(bitset->test(1));
                REQUIRE(bitset->test(2));
                REQUIRE(bitset->test(3));

                REQUIRE(zero::env::unset("ZERO_LOG_LEVEL"));
            }

            SECTION("disable") {
                using namespace std::chrono_literals;

                REQUIRE(zero::env::set("ZERO_LOG_LEVEL", "2"));

                REQUIRE_FALSE(logger.enabled(zero::log::Level::INFO_LEVEL));
                logger.addProvider(zero::log::Level::ERROR_LEVEL, std::move(provider), 50ms);

                REQUIRE(logger.enabled(zero::log::Level::INFO_LEVEL));
                REQUIRE_FALSE(logger.enabled(zero::log::Level::DEBUG_LEVEL));
                logger.log(zero::log::Level::DEBUG_LEVEL, zero::log::sourceFilename(__FILE__), __LINE__, "hello world");

                REQUIRE(event->wait());
                REQUIRE(std::chrono::system_clock::now() - tp > 45ms);
                REQUIRE(bitset->test(0));
                REQUIRE_FALSE(bitset->test(1));
                REQUIRE_FALSE(bitset->test(2));
                REQUIRE(bitset->test(3));

                REQUIRE(zero::env::unset("ZERO_LOG_LEVEL"));
            }
        }
    }

    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / "zero-test";
    REQUIRE(zero::filesystem::createDirectory(directory));

    SECTION("file provider") {
        zero::log::FileProvider provider{"zero-test", directory, 10, 3};
        REQUIRE(provider.init());

        zero::log::Record record;
        record.content = "hello world";

        SECTION("normal") {
            REQUIRE(provider.write(record));
            REQUIRE(provider.flush());

            const auto files = zero::filesystem::readDirectory(directory).transform([](const auto &it) {
                return it
                    | ranges::views::transform([](const auto &entry) {
                        REQUIRE(entry);
                        return entry->path();
                    })
                    | ranges::to<std::list>();
            });
            REQUIRE(files);
            REQUIRE_THAT(*files, Catch::Matchers::SizeIs(1));

            const auto content = zero::filesystem::readString(files->front());
            REQUIRE(content);
            REQUIRE_THAT(*content, Catch::Matchers::ContainsSubstring("hello world"));
        }

        SECTION("rotate") {
            using namespace std::chrono_literals;

            for (int i{0}; i < 10; ++i) {
                std::this_thread::sleep_for(10ms);
                REQUIRE(provider.write(record));
                REQUIRE(provider.rotate());
            }

            const auto count = zero::filesystem::readDirectory(directory).transform([](const auto &it) {
                return ranges::distance(it);
            });
            REQUIRE(count);
            REQUIRE(*count == 4);
        }
    }

    REQUIRE(zero::filesystem::removeAll(directory));
}
