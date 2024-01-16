#include <zero/log.h>
#include <catch2/catch_test_macros.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std::chrono_literals;

TEST_CASE("logging module", "[log]") {
    zero::Logger logger;

    SECTION("enable") {
        class Provider : public zero::ILogProvider {
        public:
            bool init() override {
                return true;
            }

            bool rotate() override {
                FAIL();
                return true;
            }

            bool flush() override {
                FAIL();
                return true;
            }

            zero::LogResult write(const zero::LogMessage &message) override {
                REQUIRE(message.content == "hello world");
                return zero::SUCCEEDED;
            }
        };

        REQUIRE(!logger.enabled(zero::INFO_LEVEL));
        logger.addProvider(zero::INFO_LEVEL, std::make_unique<Provider>());

        REQUIRE(logger.enabled(zero::INFO_LEVEL));
        logger.log(zero::INFO_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");
    }

    SECTION("disable") {
        class Provider : public zero::ILogProvider {
        public:
            bool init() override {
                return true;
            }

            bool rotate() override {
                FAIL();
                return true;
            }

            bool flush() override {
                FAIL();
                return true;
            }

            zero::LogResult write(const zero::LogMessage &message) override {
                FAIL();
                return zero::SUCCEEDED;
            }
        };

        REQUIRE(!logger.enabled(zero::INFO_LEVEL));
        logger.addProvider(zero::ERROR_LEVEL, std::make_unique<Provider>());

        REQUIRE(!logger.enabled(zero::INFO_LEVEL));
        logger.log(zero::INFO_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");
    }

    SECTION("override") {
        SECTION("enable") {
#ifdef _WIN32
            SetEnvironmentVariableA("ZERO_LOG_LEVEL", "3");
#else
            setenv("ZERO_LOG_LEVEL", "3", 0);
#endif

            class Provider : public zero::ILogProvider {
            public:
                bool init() override {
                    return true;
                }

                bool rotate() override {
                    FAIL();
                    return true;
                }

                bool flush() override {
                    FAIL();
                    return true;
                }

                zero::LogResult write(const zero::LogMessage &message) override {
                    REQUIRE(message.content == "hello world");
                    return zero::SUCCEEDED;
                }
            };

            REQUIRE(!logger.enabled(zero::DEBUG_LEVEL));
            logger.addProvider(zero::ERROR_LEVEL, std::make_unique<Provider>());

            REQUIRE(logger.enabled(zero::DEBUG_LEVEL));
            logger.log(zero::DEBUG_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");

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

            class Provider : public zero::ILogProvider {
            public:
                bool init() override {
                    return true;
                }

                bool rotate() override {
                    FAIL();
                    return true;
                }

                bool flush() override {
                    FAIL();
                    return true;
                }

                zero::LogResult write(const zero::LogMessage &message) override {
                    FAIL();
                    return zero::SUCCEEDED;
                }
            };

            REQUIRE(!logger.enabled(zero::INFO_LEVEL));
            logger.addProvider(zero::ERROR_LEVEL, std::make_unique<Provider>());

            REQUIRE(logger.enabled(zero::INFO_LEVEL));
            REQUIRE(!logger.enabled(zero::DEBUG_LEVEL));
            logger.log(zero::DEBUG_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");

#ifdef _WIN32
            SetEnvironmentVariableA("ZERO_LOG_LEVEL", nullptr);
#else
            unsetenv("ZERO_LOG_LEVEL");
#endif
        }
    }

    SECTION("rotate") {
        class Provider : public zero::ILogProvider {
        public:
            bool init() override {
                return true;
            }

            bool rotate() override {
                SUCCEED();
                return true;
            }

            bool flush() override {
                FAIL();
                return true;
            }

            zero::LogResult write(const zero::LogMessage &message) override {
                REQUIRE(message.content == "hello world");
                return zero::ROTATED;
            }
        };

        logger.addProvider(zero::INFO_LEVEL, std::make_unique<Provider>());
        logger.log(zero::INFO_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");
    }

    SECTION("flush") {
        class Provider : public zero::ILogProvider {
        public:
            bool init() override {
                return true;
            }

            bool rotate() override {
                FAIL();
                return true;
            }

            bool flush() override {
                SUCCEED();
                return true;
            }

            zero::LogResult write(const zero::LogMessage &message) override {
                REQUIRE(message.content == "hello world");
                return zero::SUCCEEDED;
            }
        };

        logger.addProvider(zero::INFO_LEVEL, std::make_unique<Provider>(), 10ms);
        logger.log(zero::INFO_LEVEL, zero::sourceFilename(__FILE__), __LINE__, "hello world");

        std::this_thread::sleep_for(20ms);
    }
}
