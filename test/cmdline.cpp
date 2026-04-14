#include "catch_extensions.h"
#include <zero/cmdline.h>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <fmt/ranges.h>

namespace {
    struct Config {
        std::string username;
        std::string password;

        auto operator<=>(const Config &) const = default;
    };
}

template<>
Config zero::scan(const std::string_view input) {
    const auto tokens = strings::split(input, ":", 1);

    if (tokens.size() != 2)
        throw std::invalid_argument{"Expected format 'username:password'"};

    return Config{strings::trim(tokens[0]), strings::trim(tokens[1])};
}

TEST_CASE("positional command line arguments", "[cmdline]") {
    using namespace std::string_view_literals;

    zero::Cmdline cmdline;
    cmdline.add<int>("counts", "Retry counts");

    SECTION("missing") {
        REQUIRE_THROWS_MATCHES(
            cmdline.parse({}),
            std::runtime_error,
            Catch::Matchers::Message("Missing required positional arguments: 'counts'")
        );
    }

    SECTION("invalid") {
        REQUIRE_THROWS_MATCHES(
            cmdline.parse(std::array{"/tmp"sv}),
            std::runtime_error,
            Catch::Matchers::MessageMatches(
                Catch::Matchers::StartsWith("Invalid value '/tmp' for positional argument 'counts'")
            )
        );
    }

    SECTION("valid") {
        cmdline.parse(std::array{"3"sv});
        REQUIRE(cmdline.get<int>("counts") == 3);
    }
}

TEST_CASE("optional command line arguments", "[cmdline]") {
    using namespace std::string_view_literals;

    const auto def = GENERATE(as<std::optional<int>>{}, std::nullopt, 0, 1, 2);

    zero::Cmdline cmdline;
    cmdline.addOptional<int>("counts", 'c', "Retry counts", def);

    SECTION("missing") {
        cmdline.parse({});
        REQUIRE(cmdline.getOptional<int>("counts") == def);
    }

    SECTION("missing value") {
        SECTION("long") {
            SECTION("with equal sign") {
                REQUIRE_THROWS_MATCHES(
                    cmdline.parse(std::array{"--counts="sv}),
                    std::runtime_error,
                    Catch::Matchers::Message("Optional argument '--counts' requires a value")
                );
            }

            SECTION("without equal sign") {
                REQUIRE_THROWS_MATCHES(
                    cmdline.parse(std::array{"--counts"sv}),
                    std::runtime_error,
                    Catch::Matchers::Message("Optional argument '--counts' requires a value")
                );
            }
        }

        SECTION("short") {
            REQUIRE_THROWS_MATCHES(
                cmdline.parse(std::array{"-c"sv}),
                std::runtime_error,
                Catch::Matchers::Message("Optional argument '-c' requires a value")
            );
        }
    }

    SECTION("invalid") {
        SECTION("long") {
            REQUIRE_THROWS_MATCHES(
                cmdline.parse(std::array{"--counts=/tmp"sv}),
                std::runtime_error,
                Catch::Matchers::MessageMatches(
                    Catch::Matchers::StartsWith("Invalid value '/tmp' for optional argument '--counts'")
                )
            );
        }

        SECTION("short") {
            REQUIRE_THROWS_MATCHES(
                cmdline.parse(std::array{"-c"sv, "/tmp"sv}),
                std::runtime_error,
                Catch::Matchers::MessageMatches(
                    Catch::Matchers::StartsWith("Invalid value '/tmp' for optional argument '-c'")
                )
            );
        }
    }

    SECTION("valid") {
        SECTION("long") {
            cmdline.parse(std::array{"--counts=3"sv});
        }

        SECTION("short") {
            cmdline.parse(std::array{"-c"sv, "3"sv});
        }

        REQUIRE(cmdline.getOptional<int>("counts") == 3);
    }
}

TEST_CASE("optional command line arguments without short name", "[cmdline]") {
    using namespace std::string_view_literals;

    zero::Cmdline cmdline;
    cmdline.addOptional<int>("counts", '\0', "Retry counts");

    SECTION("long") {
        cmdline.parse(std::array{"--counts=3"sv});
        REQUIRE(cmdline.getOptional<int>("counts") == 3);
    }

    SECTION("short") {
        REQUIRE_THROWS_MATCHES(
            cmdline.parse(std::array{"-c"sv, "3"sv}),
            std::runtime_error,
            Catch::Matchers::Message("Unknown optional argument '-c'")
        );
    }
}

TEST_CASE("command line flags", "[cmdline]") {
    using namespace std::string_view_literals;

    zero::Cmdline cmdline;
    cmdline.addOptional("debug", 'd', "Debug mode");

    SECTION("unexpected value") {
        REQUIRE_THROWS_MATCHES(
            cmdline.parse(std::array{"--debug="sv}),
            std::runtime_error,
            Catch::Matchers::Message("Unexpected value '' for flag 'debug'")
        );
    }

    SECTION("not set") {
        cmdline.parse({});
        REQUIRE_FALSE(cmdline.exist("debug"));
    }

    SECTION("set") {
        SECTION("long") {
            cmdline.parse(std::array{"--debug"sv});
        }

        SECTION("short") {
            cmdline.parse(std::array{"-d"sv});
        }

        REQUIRE(cmdline.exist("debug"));
    }
}

TEST_CASE("command line additional arguments", "[cmdline]") {
    using namespace std::string_view_literals;

    zero::Cmdline cmdline;
    cmdline.add<int>("counts", "Retry counts");

    SECTION("without delimiter") {
        cmdline.parse(std::array{"3"sv, "rest"sv, "arguments"sv});
        REQUIRE(cmdline.get<int>("counts") == 3);
        REQUIRE(cmdline.rest() == std::vector<std::string>{"rest", "arguments"});
    }

    SECTION("with delimiter") {
        cmdline.parse(std::array{"3"sv, "--"sv, "--counts=3"sv, "-c=3"sv});
        REQUIRE(cmdline.get<int>("counts") == 3);
        REQUIRE(cmdline.rest() == std::vector<std::string>{"--counts=3", "-c=3"});
    }
}

TEMPLATE_TEST_CASE(
    "parsing numbers from command line arguments",
    "[cmdline]",
    std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t
) {
    const auto name = GENERATE(take(1, randomAlphanumericString(1, 16)));
    const auto value = GENERATE(
        take(10, random(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::max()))
    );

    SECTION("positional") {
        const std::vector arguments{std::to_string(value)};

        zero::Cmdline cmdline;
        cmdline.add<TestType>(name, "Test value");
        cmdline.parse(std::vector<std::string_view>{arguments.begin(), arguments.end()});

        REQUIRE(cmdline.get<TestType>(name) == value);
    }

    SECTION("optional") {
        const std::vector arguments{fmt::format("--{}={}", name, value)};

        zero::Cmdline cmdline;
        cmdline.addOptional<TestType>(name, '\0', "Test value");
        cmdline.parse(std::vector<std::string_view>{arguments.begin(), arguments.end()});

        REQUIRE(cmdline.getOptional<TestType>(name) == value);
    }
}

TEMPLATE_TEST_CASE(
    "parsing number list from command line arguments",
    "[cmdline]",
    std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t
) {
    const auto name = GENERATE(take(1, randomAlphanumericString(1, 16)));
    const auto values = GENERATE(
        take(10, chunk(10, random(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::max())))
    );

    SECTION("positional") {
        const std::vector arguments{to_string(fmt::join(values, ","))};

        zero::Cmdline cmdline;
        cmdline.add<std::vector<TestType>>(name, "Test values");
        cmdline.parse(std::vector<std::string_view>{arguments.begin(), arguments.end()});

        REQUIRE(cmdline.get<std::vector<TestType>>(name) == values);
    }

    SECTION("optional") {
        const std::vector arguments{fmt::format("--{}={}", name, fmt::join(values, ","))};

        zero::Cmdline cmdline;
        cmdline.addOptional<std::vector<TestType>>(name, '\0', "Test values");
        cmdline.parse(std::vector<std::string_view>{arguments.begin(), arguments.end()});

        REQUIRE(cmdline.getOptional<std::vector<TestType>>(name) == values);
    }
}

TEST_CASE("parsing custom type from command line arguments", "[cmdline]") {
    using namespace std::string_view_literals;

    zero::Cmdline cmdline;

    SECTION("positional") {
        cmdline.add<Config>("config", "User configuration");

        SECTION("invalid") {
            REQUIRE_THROWS_MATCHES(
                cmdline.parse(std::array{"root"sv}),
                std::runtime_error,
                Catch::Matchers::MessageMatches(
                    Catch::Matchers::StartsWith("Invalid value 'root' for positional argument 'config'")
                )
            );
        }

        SECTION("valid") {
            cmdline.parse(std::array{"root:123456"sv});
            REQUIRE(cmdline.get<Config>("config") == Config{"root", "123456"});
        }
    }

    SECTION("optional") {
        cmdline.addOptional<Config>("config", '\0', "User configuration");

        SECTION("invalid") {
            REQUIRE_THROWS_MATCHES(
                cmdline.parse(std::array{"--config=root"sv}),
                std::runtime_error,
                Catch::Matchers::MessageMatches(
                    Catch::Matchers::StartsWith("Invalid value 'root' for optional argument '--config'")
                )
            );
        }

        SECTION("valid") {
            cmdline.parse(std::array{"--config=root:123456"sv});
            REQUIRE(cmdline.getOptional<Config>("config") == Config{"root", "123456"});
        }
    }
}
