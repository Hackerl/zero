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
    fakeit::Mock<zero::log::ISink> mock;

    fakeit::Fake(Dtor(mock));
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
        .Do([&] {
            event.set();
        })
        .AlwaysReturn();

    zero::log::Logger logger;

    SECTION("add") {
        SECTION("normal") {
            using namespace std::chrono_literals;

            const auto name = GENERATE(take(2, optional(randomAlphanumericString(8, 64))));
            const auto tag = GENERATE(take(2, optional(randomAlphanumericString(8, 64))));
            const auto interval = GENERATE(50ms, 100ms, 150ms, 200ms, 250ms);
            const auto tp = std::chrono::system_clock::now();

            std::vector<std::string> tags;

            if (tag)
                tags.push_back(*tag);

            REQUIRE_NOTHROW(logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, name, tags, interval));
            logger.log(level, filename, line, content, tag);

            zero::error::guard(event.wait());
            REQUIRE(std::chrono::system_clock::now() - tp > interval - 5ms);

            fakeit::Verify(Method(mock, write)).Once();
            fakeit::Verify(Method(mock, flush)).AtLeastOnce();
        }

        SECTION("duplicate name") {
            const auto name = GENERATE(take(1, randomAlphanumericString(8, 64)));
            logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, name);

            REQUIRE_THROWS_AS(
                logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, name),
                std::runtime_error
            );
        }
    }

    SECTION("remove") {
        const auto name = GENERATE(take(1, randomAlphanumericString(8, 64)));

        SECTION("normal") {
            logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, name);
            REQUIRE_NOTHROW(logger.remove(name));
            REQUIRE_FALSE(logger.enabled(level));

            for (const auto &lv: levels)
                logger.log(lv, filename, line, content);

            logger.sync();

            fakeit::Verify(Method(mock, write)).Never();
        }

        SECTION("non-existent") {
            REQUIRE_THROWS_AS(logger.remove(name), std::runtime_error);
        }
    }

    SECTION("set level") {
        const auto name = GENERATE(take(1, randomAlphanumericString(8, 64)));

        SECTION("normal") {
            logger.add(zero::log::Level::Error, std::unique_ptr<zero::log::ISink>{&mock.get()}, name);
            REQUIRE_NOTHROW(logger.setLevel(name, level));

            for (const auto &lv: levels) {
                if (lv > level) {
                    REQUIRE_FALSE(logger.enabled(lv));
                }
                else {
                    REQUIRE(logger.enabled(lv));
                }
            }

            for (const auto &lv: levels)
                logger.log(lv, filename, line, content);

            logger.sync();

            const auto times = std::to_underlying(level) - std::to_underlying(zero::log::Level::Error) + 1;
            fakeit::Verify(Method(mock, write)).Exactly(times);
        }

        SECTION("non-existent") {
            REQUIRE_THROWS_AS(logger.setLevel(name, zero::log::Level::Debug), std::runtime_error);
        }
    }

    SECTION("set flush interval") {
        using namespace std::chrono_literals;

        const auto name = GENERATE(take(1, randomAlphanumericString(8, 64)));

        SECTION("normal") {
            const auto interval = GENERATE(50ms, 100ms, 150ms, 200ms, 250ms);
            const auto tp = std::chrono::system_clock::now();

            logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, name);
            REQUIRE_NOTHROW(logger.setFlushInterval(name, interval));

            logger.log(level, filename, line, content);

            zero::error::guard(event.wait());
            REQUIRE(std::chrono::system_clock::now() - tp > interval - 5ms);

            fakeit::Verify(Method(mock, write)).Once();
            fakeit::Verify(Method(mock, flush)).AtLeastOnce();
        }

        SECTION("non-existent") {
            REQUIRE_THROWS_AS(logger.setFlushInterval(name, 50ms), std::runtime_error);
        }
    }

    SECTION("enabled") {
        SECTION("by level") {
            for (const auto &lv: levels) {
                REQUIRE_FALSE(logger.enabled(lv));
            }

            logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()});

            for (const auto &lv: levels) {
                if (lv > level) {
                    REQUIRE_FALSE(logger.enabled(lv));
                }
                else {
                    REQUIRE(logger.enabled(lv));
                }
            }
        }

        SECTION("by level and tag") {
            const auto tag = GENERATE(take(1, randomAlphanumericString(8, 64)));

            SECTION("without any sink") {
                for (const auto &lv: levels) {
                    REQUIRE_FALSE(logger.enabled(lv, tag));
                }
            }

            SECTION("with unmatched tag") {
                const auto tags = GENERATE(take(1, chunk(5, randomAlphanumericString(8, 64))));
                logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, std::nullopt, tags);

                for (const auto &lv: levels) {
                    REQUIRE_FALSE(logger.enabled(lv, tag));
                }
            }

            SECTION("with matched tag") {
                logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, std::nullopt, {tag});

                for (const auto &lv: levels) {
                    if (lv > level) {
                        REQUIRE_FALSE(logger.enabled(lv, tag));
                    }
                    else {
                        REQUIRE(logger.enabled(lv, tag));
                    }
                }
            }
        }
    }

    SECTION("log") {
        SECTION("untagged message") {
            SECTION("with untagged sink") {
                logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()});

                for (const auto &lv: levels)
                    logger.log(lv, filename, line, content);

                logger.sync();

                const auto times = std::to_underlying(level) - std::to_underlying(zero::log::Level::Error) + 1;
                fakeit::Verify(Method(mock, write)).Exactly(times);
            }

            SECTION("with tagged sink") {
                const auto tag = GENERATE(take(1, randomAlphanumericString(8, 64)));

                logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, std::nullopt, {tag});

                for (const auto &lv: levels)
                    logger.log(lv, filename, line, content);

                logger.sync();
                fakeit::Verify(Method(mock, write)).Never();
            }
        }

        SECTION("tagged message") {
            const auto tag = GENERATE(take(1, randomAlphanumericString(8, 64)));

            SECTION("with matched tag") {
                logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, std::nullopt, {tag});

                for (const auto &lv: levels)
                    logger.log(lv, filename, line, content, tag);

                logger.sync();

                const auto times = std::to_underlying(level) - std::to_underlying(zero::log::Level::Error) + 1;
                fakeit::Verify(Method(mock, write)).Exactly(times);
            }

            SECTION("without matched tag") {
                SECTION("with untagged sink") {
                    logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()});
                }

                SECTION("with unmatched sink") {
                    const auto tags = GENERATE(take(1, chunk(5, randomAlphanumericString(8, 64))));
                    logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()}, std::nullopt, tags);
                }

                for (const auto &lv: levels)
                    logger.log(lv, filename, line, content, tag);

                logger.sync();
                fakeit::Verify(Method(mock, write)).Never();
            }
        }
    }

    SECTION("sync") {
        const auto times = GENERATE(take(1, random(1, 1024)));

        logger.add(level, std::unique_ptr<zero::log::ISink>{&mock.get()});

        for (int i{0}; i < times; ++i)
            logger.log(level, filename, line, content);

        logger.sync();

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
    const auto tag = GENERATE(take(2, optional(randomAlphanumericString(8, 64))));

    std::vector<std::string> tags;

    if (tag)
        tags.push_back(*tag);

    zero::env::set("ZERO_LOG_LEVEL", std::to_string(std::to_underlying(level)));
    Z_DEFER(zero::env::unset("ZERO_LOG_LEVEL"));

    fakeit::Mock<zero::log::ISink> mock;

    fakeit::Fake(Dtor(mock));
    fakeit::When(Method(mock, write)).AlwaysReturn();
    fakeit::When(Method(mock, flush)).AlwaysReturn();

    zero::log::Logger logger;

    logger.add(zero::log::Level::Error, std::unique_ptr<zero::log::ISink>{&mock.get()}, std::nullopt, tags);

    for (const auto &lv: levels) {
        if (lv > level) {
            REQUIRE_FALSE(logger.enabled(lv));
        }
        else {
            REQUIRE(logger.enabled(lv));
        }
    }

    for (const auto &lv: levels)
        logger.log(lv, "", 0, "", tag);

    logger.sync();

    const auto times = std::to_underlying(level) - std::to_underlying(zero::log::Level::Error) + 1;
    fakeit::Verify(Method(mock, write)).Exactly(times);
}

TEST_CASE("file log sink", "[log]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto directory = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto name = GENERATE(take(1, randomAlphanumericString(1, 64)));

    SECTION("construct") {
        zero::error::guard(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));

        zero::log::FileSink sink{name, directory};

        std::size_t count{0};
        auto iterator = zero::error::guard(zero::filesystem::readDirectory(directory));

        while (zero::error::guard(iterator.next()))
            ++count;

        REQUIRE(count == 1);
    }

    SECTION("write and flush") {
        zero::error::guard(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));

        zero::log::FileSink sink{name, directory};
        zero::log::Record record;

        REQUIRE_NOTHROW(sink.write(record));
        REQUIRE_NOTHROW(sink.flush());

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

        const auto maxFileSize = GENERATE(take(1, random(64uz, 1024uz)));
        const auto maxFiles = GENERATE(take(1, random(5uz, 10uz)));

        zero::log::FileSink sink{name, directory, maxFileSize, maxFiles};

        zero::log::Record record{
            .content = GENERATE_REF(take(1, randomAlphanumericString(maxFileSize, maxFileSize)))
        };

        for (int i{0}; i < maxFiles * 2; ++i) {
            // The log file name is generated based on the timestamp.
            std::this_thread::sleep_for(10ms);
            sink.write(record);
        }

        std::size_t count{0};
        auto iterator = zero::error::guard(zero::filesystem::readDirectory(directory));

        while (zero::error::guard(iterator.next()))
            ++count;

        REQUIRE(count == maxFiles + 1);
    }
}
