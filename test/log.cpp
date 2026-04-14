#include "catch_extensions.h"
#include <zero/log.h>
#include <zero/env.h>
#include <zero/defer.h>
#include <zero/filesystem.h>
#include <zero/atomic/event.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <fakeit.hpp>
#include <ranges>

TEST_CASE("logger", "[log]") {
    constexpr std::array levels{
        zero::log::Level::Debug,
        zero::log::Level::Info,
        zero::log::Level::Warning,
        zero::log::Level::Error
    };

    const auto level = GENERATE_REF(from_range(levels));
    const auto line = GENERATE(take(1, random(0, 102400)));
    const auto filename = GENERATE("a", "b", "c", "d");
    const auto content = GENERATE(take(1, randomString(1, 102400)));

    zero::atomic::Event event;
    fakeit::Mock<zero::log::IProvider> mock;

    fakeit::Fake(Dtor(mock));
    fakeit::When(Method(mock, init)).Return();
    fakeit::When(Method(mock, rotate)).AlwaysReturn();
    fakeit::When(
        Method(mock, write)
        .Matching([&](const auto &record) {
            return record.level <= level &&
                record.line == line &&
                record.filename == filename &&
                record.content == content;
        })
    ).AlwaysReturn();

    fakeit::When(Method(mock, flush))
        .Do([&]() -> std::expected<void, std::error_code> {
            event.set();
            return {};
        })
        .AlwaysReturn();

    zero::log::Logger logger;

    SECTION("add provider") {
        using namespace std::chrono_literals;

        const auto interval = GENERATE(50ms, 100ms, 150ms, 200ms, 250ms);
        const auto tp = std::chrono::system_clock::now();

        logger.addProvider(level, std::unique_ptr<zero::log::IProvider>{&mock.get()}, interval);
        logger.log(level, filename, line, content);

        zero::error::guard(event.wait());
        REQUIRE(std::chrono::system_clock::now() - tp > interval - 5ms);

        fakeit::Verify(Method(mock, init)).Once();
        fakeit::Verify(Method(mock, rotate)).Once();
        fakeit::Verify(Method(mock, write)).Once();
        fakeit::Verify(Method(mock, flush)).AtLeastOnce();
    }

    SECTION("enabled") {
        for (const auto &lv: levels) {
            REQUIRE_FALSE(logger.enabled(lv));
        }

        logger.addProvider(level, std::unique_ptr<zero::log::IProvider>{&mock.get()});

        for (const auto &lv: levels) {
            if (lv > level) {
                REQUIRE_FALSE(logger.enabled(lv));
            }
            else {
                REQUIRE(logger.enabled(lv));
            }
        }

        fakeit::Verify(Method(mock, init)).Once();
        fakeit::Verify(Method(mock, rotate)).Never();
        fakeit::Verify(Method(mock, write)).Never();
        fakeit::Verify(Method(mock, flush)).Any();
    }

    SECTION("log") {
        logger.addProvider(level, std::unique_ptr<zero::log::IProvider>{&mock.get()});

        for (const auto &lv: levels)
            logger.log(lv, filename, line, content);

        zero::error::guard(event.wait());

        const auto times = std::to_underlying(level) - std::to_underlying(zero::log::Level::Error) + 1;

        fakeit::Verify(Method(mock, init)).Once();
        fakeit::Verify(Method(mock, rotate)).Exactly(times);
        fakeit::Verify(Method(mock, write)).Exactly(times);
        fakeit::Verify(Method(mock, flush)).AtLeastOnce();
    }

    SECTION("sync") {
        const auto times = GENERATE(take(1, random(1, 1024)));

        logger.addProvider(level, std::unique_ptr<zero::log::IProvider>{&mock.get()});

        for (int i{0}; i < times; ++i)
            logger.log(level, filename, line, content);

        logger.sync();
        REQUIRE(event.isSet());

        fakeit::Verify(Method(mock, init)).Once();
        fakeit::Verify(Method(mock, rotate)).Exactly(times);
        fakeit::Verify(Method(mock, write)).Exactly(times);
        fakeit::Verify(Method(mock, flush)).AtLeastOnce();
    }
}

TEST_CASE("override log level from environment variable", "[log]") {
    constexpr std::array levels{
        zero::log::Level::Debug,
        zero::log::Level::Info,
        zero::log::Level::Warning,
        zero::log::Level::Error
    };

    const auto level = GENERATE_REF(from_range(levels));

    zero::env::set("ZERO_LOG_LEVEL", std::to_string(std::to_underlying(level)));
    Z_DEFER(zero::env::unset("ZERO_LOG_LEVEL"));

    zero::atomic::Event event;
    fakeit::Mock<zero::log::IProvider> mock;

    fakeit::Fake(Dtor(mock));
    fakeit::When(Method(mock, init)).Return();
    fakeit::When(Method(mock, rotate)).AlwaysReturn();
    fakeit::When(Method(mock, write)).AlwaysReturn();
    fakeit::When(Method(mock, flush))
        .Do([&]() -> std::expected<void, std::error_code> {
            event.set();
            return {};
        })
        .AlwaysReturn();

    zero::log::Logger logger;

    logger.addProvider(zero::log::Level::Error, std::unique_ptr<zero::log::IProvider>{&mock.get()});

    for (const auto &lv: levels)
        logger.log(lv, "", 0, "");

    zero::error::guard(event.wait());

    const auto times = std::to_underlying(level) - std::to_underlying(zero::log::Level::Error) + 1;

    fakeit::Verify(Method(mock, init)).Once();
    fakeit::Verify(Method(mock, rotate)).Exactly(times);
    fakeit::Verify(Method(mock, write)).Exactly(times);
    fakeit::Verify(Method(mock, flush)).AtLeastOnce();
}

TEST_CASE("file log provider", "[log]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto directory = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto name = GENERATE(take(1, randomAlphanumericString(1, 64)));

    SECTION("init") {
        zero::error::guard(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));

        zero::log::FileProvider provider{name, directory};
        REQUIRE(provider.init());

        std::size_t count{0};
        auto iterator = zero::error::guard(zero::filesystem::readDirectory(directory));

        while (zero::error::guard(iterator.next()))
            ++count;

        REQUIRE(count == 1);
    }

    SECTION("write and flush") {
        zero::error::guard(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));

        zero::log::FileProvider provider{name, directory};
        zero::error::guard(provider.init());

        zero::log::Record record;

        REQUIRE(provider.write(record));
        REQUIRE(provider.flush());

        std::list<std::filesystem::path> files;

        auto iterator = zero::error::guard(zero::filesystem::readDirectory(directory));

        while (const auto entry = zero::error::guard(iterator.next()))
            files.push_back(entry->path());

        REQUIRE_THAT(files, Catch::Matchers::SizeIs(1));

        // On Windows, the file content ends with `/r/n`.
        REQUIRE_THAT(
            zero::error::guard(zero::filesystem::readString(files.front())),
            Catch::Matchers::StartsWith(fmt::to_string(record))
        );
    }

    SECTION("rotate") {
        using namespace std::chrono_literals;

        zero::error::guard(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));

        const auto limit = GENERATE(take(1, random<std::size_t>(64, 1024)));
        const auto maxFiles = GENERATE(take(1uz, random(5uz, 10uz)));

        zero::log::FileProvider provider{name, directory, limit, maxFiles};
        zero::error::guard(provider.init());

        zero::log::Record record{
            .content = GENERATE_REF(take(1, randomAlphanumericString(limit, limit)))
        };

        for (int i{0}; i < maxFiles * 2; ++i) {
            // The log file name is generated based on the timestamp.
            std::this_thread::sleep_for(10ms);
            zero::error::guard(provider.write(record));
            REQUIRE(provider.rotate());
        }

        std::size_t count{0};
        auto iterator = zero::error::guard(zero::filesystem::readDirectory(directory));

        while (zero::error::guard(iterator.next()))
            ++count;

        REQUIRE(count == maxFiles + 1);
    }
}
