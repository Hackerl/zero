#include <zero/async/promise.h>
#include <catch2/catch_test_macros.hpp>
#include <cstring>

TEST_CASE("asynchronous callback chain", "[promise]") {
    SECTION("single promise") {
        zero::async::promise::chain<int>([](const auto &p) {
            p->resolve(1);
        })->then([](int result) {
            REQUIRE(result == 1);
        });

        zero::async::promise::resolve<int>(1)->then([](int result) {
            REQUIRE(result == 1);
        });

        zero::async::promise::reject<int>({-1})->fail([](const zero::async::promise::Reason &reason) {
            REQUIRE(reason.code == -1);
        });

        zero::async::promise::resolve<int>(1)->then(
                [](int result) -> nonstd::expected<int, zero::async::promise::Reason> {
                    if (result == 2)
                        return nonstd::make_unexpected(zero::async::promise::Reason{-1});

                    return 2;
                }
        )->then([](int result) {
            REQUIRE(result == 2);
        });

        zero::async::promise::resolve<int>(1)->then(
                [](int result) -> nonstd::expected<int, zero::async::promise::Reason> {
                    if (result == 1)
                        return nonstd::make_unexpected(zero::async::promise::Reason{-1});

                    return 2;
                }
        )->fail([](const zero::async::promise::Reason &reason) {
            REQUIRE(reason.code == -1);
        });

        std::shared_ptr<int> i = std::make_shared<int>(0);

        zero::async::promise::resolve<int>(1)->finally([=]() {
            *i = 1;
        })->then([=](int result) {
            REQUIRE(*i == 1);
            REQUIRE(result == 1);
        });

        zero::async::promise::chain<std::unique_ptr<char[]>>([](const auto &p) {
            std::unique_ptr<char[]> buffer = std::make_unique<char[]>(1024);

            buffer[0] = 'h';
            buffer[1] = 'e';
            buffer[2] = 'l';
            buffer[3] = 'l';
            buffer[4] = 'o';

            p->resolve(std::move(buffer));
        })->then([](const std::unique_ptr<char[]> &buffer) {
            REQUIRE(strcmp(buffer.get(), "hello") == 0);
        });
    }

    SECTION("promise::all") {
        SECTION("same types") {
            zero::async::promise::all(
                    zero::async::promise::resolve<int>(1),
                    zero::async::promise::resolve<int>(2)
            )->then([](std::array<int, 2> results) {
                REQUIRE(results[0] == 1);
                REQUIRE(results[1] == 2);
            });
        }

        SECTION("different types") {
            zero::async::promise::all(
                    zero::async::promise::resolve<int>(1),
                    zero::async::promise::resolve<void>(),
                    zero::async::promise::resolve<long>(2)
            )->then([](int r1, long r2) {
                REQUIRE(r1 == 1);
                REQUIRE(r2 == 2);
            });
        }

        SECTION("reject") {
            zero::async::promise::all(
                    zero::async::promise::resolve<int>(1),
                    zero::async::promise::reject<void>({-1}),
                    zero::async::promise::resolve<long>(2)
            )->fail([](const zero::async::promise::Reason &reason) {
                REQUIRE(reason.code == -1);
            });
        }
    }

    SECTION("promise::allSettled") {
        zero::async::promise::allSettled(
                zero::async::promise::resolve<int>(1),
                zero::async::promise::reject<void>({-1}),
                zero::async::promise::resolve<long>(2)
        )->then([](const std::shared_ptr<zero::async::promise::Promise<int>> &p1,
                   const std::shared_ptr<zero::async::promise::Promise<void>> &p2,
                   const std::shared_ptr<zero::async::promise::Promise<long>> &p3) {
            REQUIRE(p1->status() == zero::async::promise::FULFILLED);
            REQUIRE(p1->value() == 1);
            REQUIRE(p2->status() == zero::async::promise::REJECTED);
            REQUIRE(p2->reason().code == -1);
            REQUIRE(p3->status() == zero::async::promise::FULFILLED);
            REQUIRE(p3->value() == 2);
        });
    }

    SECTION("promise::any") {
        SECTION("same types") {
            zero::async::promise::any(
                    zero::async::promise::resolve<int>(1),
                    zero::async::promise::reject<int>({-1})
            )->then([](int result) {
                REQUIRE(result == 1);
            });
        }

        SECTION("different types") {
            zero::async::promise::any(
                    zero::async::promise::resolve<int>(1),
                    zero::async::promise::reject<void>({-1}),
                    zero::async::promise::reject<long>({-1})
            )->then([](const std::any &result) {
#if _CPPRTTI || __GXX_RTTI
                REQUIRE(result.type() == typeid(int));
#endif
                REQUIRE(std::any_cast<int>(result) == 1);
            });

            zero::async::promise::any(
                    zero::async::promise::reject<int>({-1}),
                    zero::async::promise::resolve<void>(),
                    zero::async::promise::reject<long>({-1})
            )->then([](const std::any &result) {
                REQUIRE(!result.has_value());
            });
        }

        SECTION("reject") {
            zero::async::promise::any(
                    zero::async::promise::reject<int>({-1}),
                    zero::async::promise::reject<void>({-2}),
                    zero::async::promise::reject<long>({-3})
            )->fail([](const zero::async::promise::Reason &reason) {
                REQUIRE(reason.code == -3);
                REQUIRE(reason.previous->code == -2);
                REQUIRE(reason.previous->previous->code == -1);
            });
        }
    }

    SECTION("promise::race") {
        SECTION("same types") {
            zero::async::promise::race(
                    zero::async::promise::resolve<int>(1),
                    zero::async::promise::reject<int>({-1})
            )->then([](int result) {
                REQUIRE(result == 1);
            });

            zero::async::promise::race(
                    zero::async::promise::reject<int>({-1}),
                    zero::async::promise::resolve<int>(1)
            )->fail([](const zero::async::promise::Reason &reason) {
                REQUIRE(reason.code == -1);
            });
        }

        SECTION("different types") {
            zero::async::promise::race(
                    zero::async::promise::resolve<int>(1),
                    zero::async::promise::reject<int>({-1}),
                    zero::async::promise::resolve<long>(2)
            )->then([](const std::any &result) {
#if _CPPRTTI || __GXX_RTTI
                REQUIRE(result.type() == typeid(int));
#endif
                REQUIRE(std::any_cast<int>(result) == 1);
            });

            zero::async::promise::race(
                    zero::async::promise::reject<int>({-1}),
                    zero::async::promise::resolve<int>(1),
                    zero::async::promise::resolve<long>(2)
            )->fail([](const zero::async::promise::Reason &reason) {
                REQUIRE(reason.code == -1);
            });
        }
    }

    SECTION("promise::loop") {
        std::shared_ptr<int> i = std::make_shared<int>(0);

        zero::async::promise::loop<int>([=](const auto &loop) {
            *i += 1;

            if (*i < 10) {
                P_CONTINUE(loop);
                return;
            }

            P_BREAK_V(loop, *i);
        })->then([](int result) {
            REQUIRE(result == 10);
        });
    }
}