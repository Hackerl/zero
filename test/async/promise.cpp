#include <zero/async/promise.h>
#include <catch2/catch_test_macros.hpp>
#include <cstring>

TEST_CASE("asynchronous callback chain", "[promise]") {
    SECTION("single promise") {
        zero::async::promise::chain<int, int>([](auto &p) {
            p.resolve(1);
        }).then([](int result) {
            REQUIRE(result == 1);
        });

        zero::async::promise::resolve<int, int>(1).then([](int result) {
            REQUIRE(result == 1);
        });

        zero::async::promise::reject<void, int>(-1).fail([](int reason) {
            REQUIRE(reason == -1);
        });

        zero::async::promise::resolve<std::array<int, 2>, int>({1, 2}).then([](int r1, int r2) {
            REQUIRE(r1 == 1);
            REQUIRE(r2 == 2);
        });

        zero::async::promise::resolve<std::pair<int, long>, int>({1, 2L}).then([](int r1, long r2) {
            REQUIRE(r1 == 1);
            REQUIRE(r2 == 2L);
        });

        zero::async::promise::resolve<std::tuple<int, long>, int>({1, 2L}).then([](int r1, long r2) {
            REQUIRE(r1 == 1);
            REQUIRE(r2 == 2L);
        });

        zero::async::promise::resolve<int, int>(1).then([](int result) {
            return zero::async::promise::resolve<int, int>(result * 10);
        }).then([](int result) {
            REQUIRE(result == 10);
        });

        zero::async::promise::resolve<int, int>(1).then([](int result) -> tl::expected<int, int> {
            if (result == 2)
                return tl::unexpected(2);

            return 2;
        }).then([](int result) {
            REQUIRE(result == 2);
        });

        zero::async::promise::resolve<int, int>(1).then([](int result) -> tl::expected<int, int> {
            if (result == 1)
                return tl::unexpected(-1);

            return 2;
        }).fail([](int reason) {
            REQUIRE(reason == -1);
            return tl::unexpected(reason);
        });

        auto i = std::make_shared<int>(0);

        zero::async::promise::resolve<int, int>(1).finally([=]() {
            *i = 1;
        }).then([=](int result) {
            REQUIRE(*i == 1);
            REQUIRE(result == 1);
        });

        zero::async::promise::chain<std::unique_ptr<char[]>, int>([](auto &p) {
            auto buffer = std::make_unique<char[]>(1024);

            buffer[0] = 'h';
            buffer[1] = 'e';
            buffer[2] = 'l';
            buffer[3] = 'l';
            buffer[4] = 'o';

            p.resolve(std::move(buffer));
        }).then([](const std::unique_ptr<char[]> &buffer) {
            REQUIRE(strcmp(buffer.get(), "hello") == 0);
        });
    }

    SECTION("promise::all") {
        SECTION("same types") {
            zero::async::promise::all(
                    zero::async::promise::resolve<int, int>(1),
                    zero::async::promise::resolve<int, int>(2)
            ).then([](int r1, long r2) {
                REQUIRE(r1 == 1);
                REQUIRE(r2 == 2);
            });
        }

        SECTION("different types") {
            zero::async::promise::all(
                    zero::async::promise::resolve<int, int>(1),
                    zero::async::promise::resolve<void, int>(),
                    zero::async::promise::resolve<long, int>(2)
            ).then([](int r1, long r2) {
                REQUIRE(r1 == 1);
                REQUIRE(r2 == 2);
            }).fail([](int reason) {
                FAIL();
            });
        }

        SECTION("reject") {
            zero::async::promise::all(
                    zero::async::promise::resolve<int, int>(1),
                    zero::async::promise::reject<void, int>(-1),
                    zero::async::promise::resolve<int, int>(2)
            ).fail([](int reason) {
                REQUIRE(reason == -1);
                return tl::unexpected(reason);
            });
        }
    }

    SECTION("promise::allSettled") {
        zero::async::promise::allSettled(
                zero::async::promise::resolve<int, int>(1),
                zero::async::promise::reject<void, int>(-1),
                zero::async::promise::resolve<long, int>(2L)
        ).then([](const tl::expected<int, int> &r1,
                  const tl::expected<void, int> &r2,
                  const tl::expected<long, int> &r3) {
            REQUIRE(r1.value() == 1);
            REQUIRE(r2.error() == -1);
            REQUIRE(r3.value() == 2);
        });
    }

    SECTION("promise::any") {
        SECTION("same types") {
            zero::async::promise::any(
                    zero::async::promise::resolve<int, int>(1),
                    zero::async::promise::reject<int, int>(-1)
            ).then([](int result) {
                REQUIRE(result == 1);
            });
        }

        SECTION("different types") {
            zero::async::promise::any(
                    zero::async::promise::resolve<int, int>(1),
                    zero::async::promise::reject<void, int>(-1),
                    zero::async::promise::reject<long, int>(-1)
            ).then([](const std::any &result) {
#if _CPPRTTI || __GXX_RTTI
                REQUIRE(result.type() == typeid(int));
#endif
                REQUIRE(std::any_cast<int>(result) == 1);
            });

            zero::async::promise::any(
                    zero::async::promise::reject<int, int>(-1),
                    zero::async::promise::resolve<void, int>(),
                    zero::async::promise::reject<long, int>(-1)
            ).then([](const std::any &result) {
                REQUIRE(!result.has_value());
            });
        }

        SECTION("reject") {
            zero::async::promise::any(
                    zero::async::promise::reject<int, int>(-1),
                    zero::async::promise::reject<void, int>(-2),
                    zero::async::promise::reject<long, int>(-3)
            ).fail([](const std::list<int> &reasons) {
                REQUIRE(reasons.front() == -3);
                REQUIRE(reasons.back() == -1);

                return tl::unexpected(reasons);
            });
        }
    }

    SECTION("promise::race") {
        SECTION("same types") {
            zero::async::promise::race(
                    zero::async::promise::resolve<int, int>(1),
                    zero::async::promise::reject<int, int>(-1)
            ).then([](int result) {
                REQUIRE(result == 1);
            });

            zero::async::promise::race(
                    zero::async::promise::reject<int, int>(-1),
                    zero::async::promise::resolve<int, int>(1)
            ).fail([](int reason) {
                REQUIRE(reason == -1);
                return tl::unexpected(reason);
            });
        }

        SECTION("different types") {
            zero::async::promise::race(
                    zero::async::promise::resolve<int, int>(1),
                    zero::async::promise::reject<int, int>(-1),
                    zero::async::promise::resolve<long, int>(2L)
            ).then([](const std::any &result) {
#if _CPPRTTI || __GXX_RTTI
                REQUIRE(result.type() == typeid(int));
#endif
                REQUIRE(std::any_cast<int>(result) == 1);
            });

            zero::async::promise::race(
                    zero::async::promise::reject<int, int>(-1),
                    zero::async::promise::resolve<int, int>(1),
                    zero::async::promise::resolve<long, int>(2L)
            ).fail([](int reason) {
                REQUIRE(reason == -1);
                return tl::unexpected(reason);
            });
        }
    }
}