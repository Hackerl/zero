#include <zero/async/promise.h>
#include <zero/concurrent/channel.h>
#include <zero/defer.h>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <thread>
#include <range/v3/algorithm.hpp>

using namespace std::chrono_literals;

constexpr auto THREAD_NUMBER = 16;
constexpr auto CHANNEL_CAPACITY = 32;

class ThreadPool {
public:
    ThreadPool(const std::size_t number, const std::size_t capacity): mChannel(capacity) {
        for (int i = 0; i < number; i++)
            mThreads.emplace_back(&ThreadPool::dispatch, this);
    }

    ~ThreadPool() {
        mChannel.close();

        for (auto &thread: mThreads)
            thread.join();
    }

    void dispatch() {
        while (true) {
            const auto task = mChannel.receive();

            if (!task)
                break;

            (*task)();
        }
    }

    template<typename F>
    void post(F &&f) {
        mChannel.send(std::forward<F>(f));
    }

private:
    zero::concurrent::Channel<std::function<void()>> mChannel;
    std::list<std::thread> mThreads;
};

static std::unique_ptr<ThreadPool> pool;

template<typename T, typename E, typename U>
zero::async::promise::Future<T, E> reject(U &&error) {
    auto promise = std::make_shared<zero::async::promise::Promise<T, E>>();
    auto future = promise->getFuture();

    pool->post([promise = std::move(promise), error = std::forward<U>(error)] {
        std::this_thread::yield();
        promise->reject(error);
    });

    return future;
}

template<typename T, typename E, std::enable_if_t<std::is_void_v<T>, void *>  = nullptr>
zero::async::promise::Future<T, E> resolve() {
    auto promise = std::make_shared<zero::async::promise::Promise<T, E>>();
    auto future = promise->getFuture();

    pool->post([promise = std::move(promise)] {
        std::this_thread::yield();
        promise->resolve();
    });

    return future;
}

template<typename T, typename E, typename U, std::enable_if_t<!std::is_void_v<T>, void *>  = nullptr>
zero::async::promise::Future<T, E> resolve(U &&result) {
    auto promise = std::make_shared<zero::async::promise::Promise<T, E>>();
    auto future = promise->getFuture();

    pool->post(
        [promise = std::move(promise), result = std::make_shared<std::decay_t<U>>(std::forward<U>(result))] {
            std::this_thread::yield();
            promise->resolve(std::move(*result));
        }
    );

    return future;
}

TEST_CASE("promise", "[async]") {
    pool = std::make_unique<ThreadPool>(THREAD_NUMBER, CHANNEL_CAPACITY);
    DEFER(pool.reset());

    SECTION("basic") {
        zero::async::promise::Promise<int, int> promise;
        REQUIRE(promise.valid());
        REQUIRE(!promise.isFulfilled());
        auto future = promise.getFuture();
        REQUIRE(future.valid());
        REQUIRE(!future.isReady());

        const auto result = future.wait(10ms);
        REQUIRE(!result);
        REQUIRE(result.error() == std::errc::timed_out);

        SECTION("resolve") {
            promise.resolve(1024);
            REQUIRE(promise.valid());
            REQUIRE(promise.isFulfilled());

            REQUIRE(future.valid());
            REQUIRE(future.isReady());
            REQUIRE(future.wait());

            const auto &res = future.result();
            REQUIRE(res);
            REQUIRE(*res == 1024);
        }

        SECTION("reject") {
            promise.reject(-1);
            REQUIRE(promise.valid());
            REQUIRE(promise.isFulfilled());

            REQUIRE(future.valid());
            REQUIRE(future.isReady());
            REQUIRE(future.wait());

            const auto &res = future.result();
            REQUIRE(!res);
            REQUIRE(res.error() == -1);
        }
    }

    SECTION("callback chain") {
        zero::async::promise::chain<int, int>([](auto p) {
            p.resolve(1);
        }).then([](const int value) {
            REQUIRE(value == 1);
        });

        zero::async::promise::resolve<int, int>(1)
            .then([](const int value) {
                REQUIRE(value == 1);
            });

        zero::async::promise::reject<void, int>(-1)
            .fail([](const int error) {
                REQUIRE(error == -1);
            });

        zero::async::promise::resolve<std::array<int, 2>, int>(std::array{1, 2})
            .then([](const int r1, const int r2) {
                REQUIRE(r1 == 1);
                REQUIRE(r2 == 2);
            });

        zero::async::promise::resolve<std::pair<int, long>, int>(1, 2L)
            .then(
                [](const int r1, const long r2) {
                    REQUIRE(r1 == 1);
                    REQUIRE(r2 == 2L);
                }
            );

        zero::async::promise::resolve<std::tuple<int, long>, int>(1, 2L)
            .then([](const int r1, const long r2) {
                REQUIRE(r1 == 1);
                REQUIRE(r2 == 2L);
            });

        zero::async::promise::resolve<int, int>(1)
            .then([](const int value) {
                return zero::async::promise::resolve<int, int>(value * 10);
            }).then([](const int value) {
                REQUIRE(value == 10);
            });

        zero::async::promise::resolve<int, int>(1)
            .then([](const int value) -> tl::expected<int, int> {
                if (value == 2)
                    return tl::unexpected(2);

                return 2;
            }).then([](const int value) {
                REQUIRE(value == 2);
            });

        zero::async::promise::resolve<int, int>(1)
            .then([](const int value) -> tl::expected<int, int> {
                if (value == 1)
                    return tl::unexpected(-1);

                return 2;
            }).fail([](const int error) {
                REQUIRE(error == -1);
                return tl::unexpected(error);
            });

        const auto i = std::make_shared<int>(0);

        zero::async::promise::resolve<int, int>(1)
            .finally([=] {
                *i = 1;
            }).then([=](const int value) {
                REQUIRE(*i == 1);
                REQUIRE(value == 1);
            });

        zero::async::promise::chain<std::unique_ptr<char[]>, int>([](auto p) {
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

    SECTION("concurrent") {
        {
            const auto result = resolve<int, int>(1).get();
            REQUIRE(result);
            REQUIRE(*result == 1);
        }

        {
            const auto result = reject<void, int>(-1).get();
            REQUIRE(!result);
            REQUIRE(result.error() == -1);
        }

        {
            const auto result = resolve<int, int>(1)
                                .then([](const int value) {
                                    return resolve<int, int>(value * 10);
                                }).get();
            REQUIRE(result);
            REQUIRE(*result == 10);
        }

        {
            const auto result = resolve<int, int>(1)
                                .then([](const int value) -> tl::expected<int, int> {
                                    if (value == 2)
                                        return tl::unexpected(2);

                                    return 2;
                                }).get();
            REQUIRE(result);
            REQUIRE(*result == 2);
        }

        {
            const auto result = resolve<int, int>(1)
                                .then([](const int value) -> tl::expected<int, int> {
                                    if (value == 1)
                                        return tl::unexpected(-1);

                                    return 2;
                                }).get();
            REQUIRE(!result);
            REQUIRE(result.error() == -1);
        }

        {
            const auto i = std::make_shared<int>(0);
            const auto result = resolve<int, int>(1)
                                .finally([=] {
                                    *i = 1;
                                }).get();
            REQUIRE(result);
            REQUIRE(*result == 1);
            REQUIRE(*i == 1);
        }

        {
            auto buffer = std::make_unique<char[]>(1024);

            buffer[0] = 'h';
            buffer[1] = 'e';
            buffer[2] = 'l';
            buffer[3] = 'l';
            buffer[4] = 'o';

            const auto result = resolve<std::unique_ptr<char[]>, int>(std::move(buffer)).get();
            REQUIRE(result);
            REQUIRE(strcmp(result->get(), "hello") == 0);
        }
    }

    SECTION("promise::all") {
        SECTION("same types") {
            const auto result = all(
                resolve<int, int>(1),
                resolve<int, int>(2),
                resolve<int, int>(3),
                resolve<int, int>(4),
                resolve<int, int>(5)
            ).get();
            REQUIRE(result);

            const auto [r1, r2, r3, r4, r5] = *result;
            REQUIRE(r1 == 1);
            REQUIRE(r2 == 2);
            REQUIRE(r3 == 3);
            REQUIRE(r4 == 4);
            REQUIRE(r5 == 5);
        }

        SECTION("different types") {
            const auto result = all(
                resolve<int, int>(1),
                resolve<void, int>(),
                resolve<long, int>(2),
                resolve<long, int>(3),
                resolve<long, int>(4)
            ).get();
            REQUIRE(result);
            const auto [r1, r2, r3, r4] = *result;

            REQUIRE(r1 == 1);
            REQUIRE(r2 == 2);
            REQUIRE(r3 == 3);
            REQUIRE(r4 == 4);
        }

        SECTION("reject") {
            const auto result = all(
                resolve<int, int>(1),
                reject<void, int>(-1),
                reject<void, int>(-2),
                reject<void, int>(-3),
                resolve<int, int>(2),
                resolve<int, int>(3)
            ).get();
            REQUIRE(!result);
            const int error = result.error();
            REQUIRE((error == -1 || error == -2 || error == -3));
        }
    }

    SECTION("promise::allSettled") {
        const auto result = allSettled(
            resolve<int, int>(1),
            reject<void, int>(-1),
            resolve<long, int>(2L),
            resolve<long, long>(3),
            resolve<int, int>(4),
            resolve<void, int>()
        ).get();
        REQUIRE(result);

        const auto [r1, r2, r3, r4, r5, r6] = *result;
        REQUIRE(r1.value() == 1);
        REQUIRE(r2.error() == -1);
        REQUIRE(r3.value() == 2);
        REQUIRE(r4.value() == 3);
        REQUIRE(r5.value() == 4);
        REQUIRE(r6);
    }

    SECTION("promise::any") {
        SECTION("same types") {
            const auto result = any(
                reject<int, int>(-1),
                reject<int, int>(-2),
                reject<int, int>(-3),
                reject<int, int>(-4),
                reject<int, int>(-5),
                reject<int, int>(-6),
                resolve<int, int>(1)
            ).get();
            REQUIRE(result);
            REQUIRE(*result == 1);
        }

        SECTION("different types") {
            {
                const auto result = any(
                    reject<void, int>(-1),
                    reject<long, int>(-2),
                    reject<void, int>(-3),
                    reject<long, int>(-4),
                    reject<void, int>(-5),
                    reject<long, int>(-6),
                    resolve<int, int>(1)
                ).get();

                REQUIRE(result);
                const auto &any = *result;
#if _CPPRTTI || __GXX_RTTI
                REQUIRE(any.type() == typeid(int));
#endif
                REQUIRE(std::any_cast<int>(any) == 1);
            }

            {
                const auto result = any(
                    reject<void, int>(-1),
                    reject<long, int>(-2),
                    reject<void, int>(-3),
                    reject<long, int>(-4),
                    reject<void, int>(-5),
                    reject<long, int>(-6),
                    resolve<void, int>()
                ).get();
                REQUIRE(result);
                REQUIRE(!result->has_value());
            }
        }

        SECTION("reject") {
            const auto result = any(
                reject<void, int>(-1),
                reject<long, int>(-2),
                reject<void, int>(-3),
                reject<long, int>(-4),
                reject<void, int>(-5),
                reject<long, int>(-6)
            ).get();
            REQUIRE(!result);

            const auto &errors = result.error();
            REQUIRE(errors.size() == 6);
            REQUIRE(ranges::find(errors, -1) != errors.end());
            REQUIRE(ranges::find(errors, -2) != errors.end());
            REQUIRE(ranges::find(errors, -3) != errors.end());
            REQUIRE(ranges::find(errors, -4) != errors.end());
            REQUIRE(ranges::find(errors, -5) != errors.end());
            REQUIRE(ranges::find(errors, -6) != errors.end());
        }
    }

    SECTION("promise::race") {
        SECTION("same types") {
            if (const auto result = race(
                resolve<int, int>(1),
                reject<int, int>(-1),
                resolve<int, int>(2),
                reject<int, int>(-2),
                resolve<int, int>(3),
                reject<int, int>(-3)
            ).get(); result) {
                const int value = *result;
                REQUIRE((value == 1 || value == 2 || value == 3));
            }
            else {
                const int error = result.error();
                REQUIRE((error == -1 || error == -2 || error == -3));
            }
        }

        SECTION("different types") {
            if (const auto result = race(
                resolve<int, int>(1),
                reject<long, int>(-1),
                resolve<int, int>(2),
                reject<long, int>(-2),
                resolve<int, int>(3),
                reject<long, int>(-3)
            ).get(); result) {
                const auto &any = *result;
#if _CPPRTTI || __GXX_RTTI
                REQUIRE(any.type() == typeid(int));
#endif
                const int value = std::any_cast<int>(any);
                REQUIRE((value == 1 || value == 2 || value == 3));
            }
            else {
                const int error = result.error();
                REQUIRE((error == -1 || error == -2 || error == -3));
            }
        }
    }
}
