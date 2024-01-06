#include <zero/async/coroutine.h>
#include <catch2/catch_test_macros.hpp>

zero::async::coroutine::Task<int> half(zero::async::coroutine::Task<int> task) {
    const auto value = co_await task;

    if (value % 2)
        throw std::system_error(make_error_code(std::errc::invalid_argument));

    co_return value / 2;
}

zero::async::coroutine::Task<long> half(zero::async::coroutine::Task<long> task) {
    const auto value = co_await task;

    if (value % 2)
        throw std::system_error(make_error_code(std::errc::invalid_argument));

    co_return value / 2;
}

zero::async::coroutine::Task<void> requireEven(zero::async::coroutine::Task<int> task) {
    if (const auto value = co_await task; value % 2)
        throw std::system_error(make_error_code(std::errc::invalid_argument));

    co_return;
}

TEST_CASE("C++20 coroutines with exception", "[coroutine]") {
    SECTION("success") {
        const auto promise = zero::async::promise::make<int, std::exception_ptr>();
        const auto task = half(zero::async::coroutine::from(promise));
        REQUIRE(!task.done());

        promise->resolve(10);
        REQUIRE(task.done());

        const auto &result = task.result();
        REQUIRE(result);
        REQUIRE(*result == 5);
    }

    SECTION("failure") {
        const auto promise = zero::async::promise::make<int, std::exception_ptr>();
        const auto task = half(zero::async::coroutine::from(promise));
        REQUIRE(!task.done());

        promise->resolve(9);
        REQUIRE(task.done());

        const auto &result = task.result();
        REQUIRE(!result);

        try {
            std::rethrow_exception(result.error());
        }
        catch (const std::system_error &error) {
            REQUIRE(error.code() == std::errc::invalid_argument);
        }
    }

    SECTION("throw") {
        const auto promise = zero::async::promise::make<int, std::exception_ptr>();
        const auto task = half(zero::async::coroutine::from(promise));
        REQUIRE(!task.done());

        promise->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
        REQUIRE(task.done());

        const auto &result = task.result();
        REQUIRE(!result);

        try {
            std::rethrow_exception(result.error());
        }
        catch (const std::system_error &error) {
            REQUIRE(error.code() == std::errc::owner_dead);
        }
    }

    SECTION("cancel") {
        const auto promise = zero::async::promise::make<int, std::exception_ptr>();
        auto task = half(from(zero::async::coroutine::Cancellable{
            promise,
            [=]() mutable -> tl::expected<void, std::error_code> {
                promise->reject(std::make_exception_ptr(
                    std::system_error(make_error_code(std::errc::operation_canceled))
                ));
                return {};
            }
        }));
        REQUIRE(!task.done());

        task.cancel();
        REQUIRE(task.done());

        const auto &result = task.result();
        REQUIRE(!result);

        try {
            std::rethrow_exception(result.error());
        }
        catch (const std::system_error &error) {
            REQUIRE(error.code() == std::errc::operation_canceled);
        }
    }

    SECTION("traceback") {
        const auto promise = zero::async::promise::make<int, std::exception_ptr>();
        const auto task = half(zero::async::coroutine::from(promise));
        REQUIRE(!task.done());

        auto callstack = task.traceback();
        REQUIRE(!callstack.empty());
        REQUIRE(strstr(callstack[0].function_name(), "half"));

        promise->resolve(10);
        REQUIRE(task.done());
        REQUIRE(task.traceback().empty());

        const auto &result = task.result();
        REQUIRE(result);
        REQUIRE(*result == 5);
    }

    SECTION("coroutine::all") {
        SECTION("same types") {
            SECTION("success") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = all(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->resolve(10);
                promise2->resolve(100);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(result);
                REQUIRE(*result == std::array{5, 50});
            }

            SECTION("failure") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = all(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->resolve(10);
                promise2->resolve(99);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                try {
                    std::rethrow_exception(result.error());
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }
            }

            SECTION("throw") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = all(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->resolve(10);
                promise2->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                try {
                    std::rethrow_exception(result.error());
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }
            }

            SECTION("cancel") {
                SECTION("started") {
                    SECTION("supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = all(
                            half(from(zero::async::coroutine::Cancellable{
                                promise1,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise1->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            half(from(zero::async::coroutine::Cancellable{
                                promise2,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise2->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            }))
                        );
                        REQUIRE(!task.done());

                        promise1->resolve(10);
                        const auto &res = task.cancel();
                        REQUIRE(res);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }
                    }

                    SECTION("not supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = all(
                            half(zero::async::coroutine::from(promise1)),
                            half(zero::async::coroutine::from(promise2))
                        );
                        REQUIRE(!task.done());

                        promise1->resolve(10);
                        const auto &res = task.cancel();
                        REQUIRE(!res);
                        REQUIRE(res.error() == std::errc::operation_not_supported);

                        promise2->resolve(100);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(result);
                        REQUIRE(*result == std::array{5, 50});
                    }
                }

                SECTION("not started") {
                    SECTION("supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = all(
                            half(from(zero::async::coroutine::Cancellable{
                                promise1,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise1->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            half(from(zero::async::coroutine::Cancellable{
                                promise2,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise2->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            }))
                        );
                        REQUIRE(!task.done());

                        const auto &res = task.cancel();
                        REQUIRE(res);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }
                    }

                    SECTION("not supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = all(
                            half(zero::async::coroutine::from(promise1)),
                            half(zero::async::coroutine::from(promise2))
                        );
                        REQUIRE(!task.done());

                        const auto &res = task.cancel();
                        REQUIRE(!res);
                        REQUIRE(res.error() == std::errc::operation_not_supported);

                        promise1->resolve(10);
                        promise2->resolve(100);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(result);
                        REQUIRE(*result == std::array{5, 50});
                    }
                }
            }
        }

        SECTION("different types") {
            SECTION("success") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = all(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2)),
                    requireEven(zero::async::coroutine::from(promise3))
                );
                REQUIRE(!task.done());

                promise1->resolve(10);
                promise2->resolve(100);
                promise3->resolve(200);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(result);
                REQUIRE(*result == std::tuple{5, 50});
            }

            SECTION("failure") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = all(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2)),
                    requireEven(zero::async::coroutine::from(promise3))
                );
                REQUIRE(!task.done());

                promise1->resolve(10);
                promise2->resolve(200);
                promise3->resolve(99);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                try {
                    std::rethrow_exception(result.error());
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }
            }

            SECTION("throw") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = all(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2)),
                    requireEven(zero::async::coroutine::from(promise3))
                );
                REQUIRE(!task.done());

                promise1->resolve(10);
                promise2->resolve(100);
                promise3->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                try {
                    std::rethrow_exception(result.error());
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }
            }

            SECTION("cancel") {
                SECTION("started") {
                    SECTION("supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                        const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = all(
                            half(from(zero::async::coroutine::Cancellable{
                                promise1,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise1->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            half(from(zero::async::coroutine::Cancellable{
                                promise2,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise2->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            requireEven(from(zero::async::coroutine::Cancellable{
                                promise3,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise3->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            }))
                        );
                        REQUIRE(!task.done());

                        promise1->resolve(10);
                        promise2->resolve(100);
                        const auto &res = task.cancel();
                        REQUIRE(res);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }
                    }

                    SECTION("not supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                        const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = all(
                            half(zero::async::coroutine::from(promise1)),
                            half(zero::async::coroutine::from(promise2)),
                            requireEven(zero::async::coroutine::from(promise3))
                        );
                        REQUIRE(!task.done());

                        promise1->resolve(10);
                        const auto &res = task.cancel();
                        REQUIRE(!res);
                        REQUIRE(res.error() == std::errc::operation_not_supported);

                        promise2->resolve(100);
                        promise3->resolve(200);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(result);
                        REQUIRE(*result == std::tuple{5, 50});
                    }
                }

                SECTION("not started") {
                    SECTION("supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                        const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = all(
                            half(from(zero::async::coroutine::Cancellable{
                                promise1,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise1->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            half(from(zero::async::coroutine::Cancellable{
                                promise2,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise2->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            requireEven(from(zero::async::coroutine::Cancellable{
                                promise3,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise3->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            }))
                        );
                        REQUIRE(!task.done());

                        const auto &res = task.cancel();
                        REQUIRE(res);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }
                    }

                    SECTION("not supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                        const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = all(
                            half(zero::async::coroutine::from(promise1)),
                            half(zero::async::coroutine::from(promise2)),
                            requireEven(zero::async::coroutine::from(promise3))
                        );
                        REQUIRE(!task.done());

                        const auto &res = task.cancel();
                        REQUIRE(!res);
                        REQUIRE(res.error() == std::errc::operation_not_supported);

                        promise1->resolve(10);
                        promise2->resolve(100);
                        promise3->resolve(200);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(result);
                        REQUIRE(*result == std::tuple{5, 50});
                    }
                }
            }
        }
    }

    SECTION("coroutine::allSettled") {
        SECTION("success") {
            const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
            const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
            const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

            const auto task = allSettled(
                half(zero::async::coroutine::from(promise1)),
                half(zero::async::coroutine::from(promise2)),
                requireEven(zero::async::coroutine::from(promise3))
            );
            REQUIRE(!task.done());

            promise1->resolve(10);
            promise2->resolve(100);
            promise3->resolve(200);
            REQUIRE(task.done());

            const auto &result = task.result();
            REQUIRE(result);
            REQUIRE(std::get<0>(*result));
            REQUIRE(*std::get<0>(*result) == 5);
            REQUIRE(std::get<1>(*result));
            REQUIRE(*std::get<1>(*result) == 50);
            REQUIRE(std::get<2>(*result));
        }

        SECTION("failure") {
            const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
            const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
            const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

            const auto task = allSettled(
                half(zero::async::coroutine::from(promise1)),
                half(zero::async::coroutine::from(promise2)),
                requireEven(zero::async::coroutine::from(promise3))
            );
            REQUIRE(!task.done());

            promise1->resolve(10);
            promise2->resolve(100);
            promise3->resolve(99);
            REQUIRE(task.done());

            const auto &result = task.result();
            REQUIRE(result);
            REQUIRE(std::get<0>(*result));
            REQUIRE(*std::get<0>(*result) == 5);
            REQUIRE(std::get<1>(*result));
            REQUIRE(*std::get<1>(*result) == 50);
            REQUIRE(!std::get<2>(*result));

            try {
                std::rethrow_exception(std::get<2>(*result).error());
            }
            catch (const std::system_error &error) {
                REQUIRE(error.code() == std::errc::invalid_argument);
            }
        }

        SECTION("throw") {
            const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
            const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
            const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

            const auto task = allSettled(
                half(zero::async::coroutine::from(promise1)),
                half(zero::async::coroutine::from(promise2)),
                requireEven(zero::async::coroutine::from(promise3))
            );
            REQUIRE(!task.done());

            promise1->resolve(10);
            promise2->resolve(100);
            promise3->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
            REQUIRE(task.done());

            const auto &result = task.result();
            REQUIRE(result);
            REQUIRE(std::get<0>(*result));
            REQUIRE(*std::get<0>(*result) == 5);
            REQUIRE(std::get<1>(*result));
            REQUIRE(*std::get<1>(*result) == 50);
            REQUIRE(!std::get<2>(*result));

            try {
                std::rethrow_exception(std::get<2>(*result).error());
            }
            catch (const std::system_error &error) {
                REQUIRE(error.code() == std::errc::owner_dead);
            }
        }

        SECTION("cancel") {
            SECTION("started") {
                SECTION("supported") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    auto task = allSettled(
                        half(from(zero::async::coroutine::Cancellable{
                            promise1,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise1->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        })),
                        half(from(zero::async::coroutine::Cancellable{
                            promise2,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise2->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        })),
                        requireEven(from(zero::async::coroutine::Cancellable{
                            promise3,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise3->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        }))
                    );
                    REQUIRE(!task.done());

                    promise1->resolve(10);
                    promise2->resolve(100);
                    const auto &res = task.cancel();
                    REQUIRE(res);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(std::get<0>(*result));
                    REQUIRE(*std::get<0>(*result) == 5);
                    REQUIRE(std::get<1>(*result));
                    REQUIRE(*std::get<1>(*result) == 50);
                    REQUIRE(!std::get<2>(*result));

                    try {
                        std::rethrow_exception(std::get<2>(*result).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    }
                }

                SECTION("not supported") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    auto task = allSettled(
                        half(zero::async::coroutine::from(promise1)),
                        half(zero::async::coroutine::from(promise2)),
                        requireEven(zero::async::coroutine::from(promise3))
                    );
                    REQUIRE(!task.done());

                    promise1->resolve(10);
                    const auto &res = task.cancel();
                    REQUIRE(!res);
                    REQUIRE(res.error() == std::errc::operation_not_supported);

                    promise2->resolve(100);
                    promise3->resolve(200);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(std::get<0>(*result));
                    REQUIRE(*std::get<0>(*result) == 5);
                    REQUIRE(std::get<1>(*result));
                    REQUIRE(*std::get<1>(*result) == 50);
                    REQUIRE(std::get<2>(*result));
                }
            }

            SECTION("not started") {
                SECTION("supported") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    auto task = allSettled(
                        half(from(zero::async::coroutine::Cancellable{
                            promise1,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise1->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        })),
                        half(from(zero::async::coroutine::Cancellable{
                            promise2,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise2->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        })),
                        requireEven(from(zero::async::coroutine::Cancellable{
                            promise3,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise3->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        }))
                    );
                    REQUIRE(!task.done());

                    const auto &res = task.cancel();
                    REQUIRE(res);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(!std::get<0>(*result));
                    REQUIRE(!std::get<1>(*result));
                    REQUIRE(!std::get<2>(*result));

                    try {
                        std::rethrow_exception(std::get<0>(*result).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    }

                    try {
                        std::rethrow_exception(std::get<1>(*result).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    }

                    try {
                        std::rethrow_exception(std::get<2>(*result).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    }
                }

                SECTION("not supported") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    auto task = allSettled(
                        half(zero::async::coroutine::from(promise1)),
                        half(zero::async::coroutine::from(promise2)),
                        requireEven(zero::async::coroutine::from(promise3))
                    );
                    REQUIRE(!task.done());

                    const auto &res = task.cancel();
                    REQUIRE(!res);
                    REQUIRE(res.error() == std::errc::operation_not_supported);

                    promise1->resolve(10);
                    promise2->resolve(100);
                    promise3->resolve(200);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(std::get<0>(*result));
                    REQUIRE(*std::get<0>(*result) == 5);
                    REQUIRE(std::get<1>(*result));
                    REQUIRE(*std::get<1>(*result) == 50);
                    REQUIRE(std::get<2>(*result));
                }
            }
        }

        SECTION("different types") {
            const auto promise1 = zero::async::promise::make<int, long>();
            const auto promise2 = zero::async::promise::make<long, long>();
            const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

            const auto task = allSettled(
                zero::async::coroutine::from(promise1),
                zero::async::coroutine::from(promise2),
                zero::async::coroutine::from(promise3)
            );
            REQUIRE(!task.done());

            promise1->resolve(10);
            promise2->resolve(100);
            promise3->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
            REQUIRE(task.done());

            const auto &result = task.result();
            REQUIRE(result);
            REQUIRE(std::get<0>(*result));
            REQUIRE(*std::get<0>(*result) == 10);
            REQUIRE(std::get<1>(*result));
            REQUIRE(*std::get<1>(*result) == 100);
            REQUIRE(!std::get<2>(*result));

            try {
                std::rethrow_exception(std::get<2>(*result).error());
            }
            catch (const std::system_error &error) {
                REQUIRE(error.code() == std::errc::io_error);
            }
        }
    }

    SECTION("coroutine::any") {
        SECTION("same types") {
            SECTION("success") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = any(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->resolve(9);
                promise2->resolve(100);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(result);
                REQUIRE(*result == 50);
            }

            SECTION("failure") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = any(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->resolve(9);
                promise2->resolve(99);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                auto it = result.error().begin();

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }
            }

            SECTION("throw") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = any(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                promise2->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                auto it = result.error().begin();

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }
            }

            SECTION("cancel") {
                SECTION("started") {
                    SECTION("supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = any(
                            half(from(zero::async::coroutine::Cancellable{
                                promise1,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise1->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            half(from(zero::async::coroutine::Cancellable{
                                promise2,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise2->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            }))
                        );
                        REQUIRE(!task.done());

                        promise1->resolve(9);
                        const auto &res = task.cancel();
                        REQUIRE(res);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(!result);

                        auto it = result.error().begin();

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        }
                    }

                    SECTION("not supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = any(
                            half(zero::async::coroutine::from(promise1)),
                            half(zero::async::coroutine::from(promise2))
                        );
                        REQUIRE(!task.done());

                        promise1->resolve(9);
                        const auto &res = task.cancel();
                        REQUIRE(!res);
                        REQUIRE(res.error() == std::errc::operation_not_supported);

                        promise2->resolve(100);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(result);
                        REQUIRE(*result == 50);
                    }
                }

                SECTION("not started") {
                    SECTION("supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = any(
                            half(from(zero::async::coroutine::Cancellable{
                                promise1,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise1->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            half(from(zero::async::coroutine::Cancellable{
                                promise2,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise2->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            }))
                        );
                        REQUIRE(!task.done());

                        const auto &res = task.cancel();
                        REQUIRE(res);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(!result);

                        auto it = result.error().begin();

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }
                    }

                    SECTION("not supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = any(
                            half(zero::async::coroutine::from(promise1)),
                            half(zero::async::coroutine::from(promise2))
                        );
                        REQUIRE(!task.done());

                        const auto &res = task.cancel();
                        REQUIRE(!res);
                        REQUIRE(res.error() == std::errc::operation_not_supported);

                        promise1->resolve(9);
                        promise2->resolve(100);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(result);
                        REQUIRE(*result == 50);
                    }
                }
            }
        }

        SECTION("different types") {
            SECTION("success") {
                SECTION("has value") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    const auto task = any(
                        half(zero::async::coroutine::from(promise1)),
                        half(zero::async::coroutine::from(promise2)),
                        requireEven(zero::async::coroutine::from(promise3))
                    );
                    REQUIRE(!task.done());

                    promise1->resolve(9);
                    promise2->resolve(100);
                    promise3->resolve(99);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(result->has_value());

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result->type() == typeid(long));
#endif
                    REQUIRE(std::any_cast<long>(*result) == 50L);
                }

                SECTION("no value") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    const auto task = any(
                        half(zero::async::coroutine::from(promise1)),
                        half(zero::async::coroutine::from(promise2)),
                        requireEven(zero::async::coroutine::from(promise3))
                    );
                    REQUIRE(!task.done());

                    promise1->resolve(9);
                    promise2->resolve(99);
                    promise3->resolve(100);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(!result->has_value());

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result->type() == typeid(void));
#endif
                }
            }

            SECTION("failure") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = any(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2)),
                    requireEven(zero::async::coroutine::from(promise3))
                );
                REQUIRE(!task.done());

                promise1->resolve(9);
                promise2->resolve(99);
                promise3->resolve(199);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                auto it = result.error().begin();

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }
            }

            SECTION("throw") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = any(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2)),
                    requireEven(zero::async::coroutine::from(promise3))
                );
                REQUIRE(!task.done());

                promise1->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                promise2->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                promise3->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                auto it = result.error().begin();

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }

                try {
                    std::rethrow_exception(*it++);
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }
            }

            SECTION("cancel") {
                SECTION("started") {
                    SECTION("supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                        const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = any(
                            half(from(zero::async::coroutine::Cancellable{
                                promise1,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise1->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            half(from(zero::async::coroutine::Cancellable{
                                promise2,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise2->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            requireEven(from(zero::async::coroutine::Cancellable{
                                promise3,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise3->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            }))
                        );
                        REQUIRE(!task.done());

                        promise1->resolve(9);
                        const auto &res = task.cancel();
                        REQUIRE(res);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(!result);

                        auto it = result.error().begin();

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        }
                    }

                    SECTION("not supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                        const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = any(
                            half(zero::async::coroutine::from(promise1)),
                            half(zero::async::coroutine::from(promise2)),
                            requireEven(zero::async::coroutine::from(promise3))
                        );
                        REQUIRE(!task.done());

                        promise1->resolve(9);
                        const auto &res = task.cancel();
                        REQUIRE(!res);
                        REQUIRE(res.error() == std::errc::operation_not_supported);

                        promise2->resolve(99);
                        promise3->resolve(100);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(result);
                        REQUIRE(!result->has_value());

#if _CPPRTTI || __GXX_RTTI
                        REQUIRE(result->type() == typeid(void));
#endif
                    }
                }

                SECTION("not started") {
                    SECTION("supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                        const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = any(
                            half(from(zero::async::coroutine::Cancellable{
                                promise1,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise1->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            half(from(zero::async::coroutine::Cancellable{
                                promise2,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise2->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            })),
                            requireEven(from(zero::async::coroutine::Cancellable{
                                promise3,
                                [=]() mutable -> tl::expected<void, std::error_code> {
                                    promise3->reject(std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))
                                    ));
                                    return {};
                                }
                            }))
                        );
                        REQUIRE(!task.done());

                        const auto &res = task.cancel();
                        REQUIRE(res);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(!result);

                        auto it = result.error().begin();

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }

                        try {
                            std::rethrow_exception(*it++);
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        }
                    }

                    SECTION("not supported") {
                        const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                        const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                        const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                        auto task = any(
                            half(zero::async::coroutine::from(promise1)),
                            half(zero::async::coroutine::from(promise2)),
                            requireEven(zero::async::coroutine::from(promise3))
                        );
                        REQUIRE(!task.done());

                        const auto &res = task.cancel();
                        REQUIRE(!res);
                        REQUIRE(res.error() == std::errc::operation_not_supported);

                        promise1->resolve(9);
                        promise2->resolve(100);
                        promise3->resolve(90);
                        REQUIRE(task.done());

                        const auto &result = task.result();
                        REQUIRE(result);
                        REQUIRE(result->has_value());

#if _CPPRTTI || __GXX_RTTI
                        REQUIRE(result->type() == typeid(long));
#endif
                        REQUIRE(std::any_cast<long>(*result) == 50L);
                    }
                }
            }
        }
    }

    SECTION("coroutine::race") {
        SECTION("same types") {
            SECTION("success") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = race(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->resolve(10);
                promise2->resolve(99);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(result);
                REQUIRE(*result == 5);
            }

            SECTION("failure") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = race(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->resolve(9);
                promise2->resolve(100);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                try {
                    std::rethrow_exception(result.error());
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }
            }

            SECTION("throw") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = race(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2))
                );
                REQUIRE(!task.done());

                promise1->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                promise2->resolve(100);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                try {
                    std::rethrow_exception(result.error());
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }
            }

            SECTION("cancel") {
                SECTION("supported") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                    auto task = race(
                        half(from(zero::async::coroutine::Cancellable{
                            promise1,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise1->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        })),
                        half(from(zero::async::coroutine::Cancellable{
                            promise2,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise2->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        }))
                    );
                    REQUIRE(!task.done());

                    const auto &res = task.cancel();
                    REQUIRE(res);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    }
                }

                SECTION("not supported") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<int, std::exception_ptr>();

                    auto task = race(
                        half(zero::async::coroutine::from(promise1)),
                        half(zero::async::coroutine::from(promise2))
                    );
                    REQUIRE(!task.done());

                    const auto &res = task.cancel();
                    REQUIRE(!res);
                    REQUIRE(res.error() == std::errc::operation_not_supported);

                    promise1->resolve(10);
                    promise2->resolve(100);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(*result == 5);
                }
            }
        }

        SECTION("different types") {
            SECTION("success") {
                SECTION("has value") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    const auto task = race(
                        half(zero::async::coroutine::from(promise1)),
                        half(zero::async::coroutine::from(promise2)),
                        requireEven(zero::async::coroutine::from(promise3))
                    );
                    REQUIRE(!task.done());

                    promise1->resolve(10);
                    promise2->resolve(99);
                    promise3->resolve(199);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(result->has_value());

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result->type() == typeid(int));
#endif
                    REQUIRE(std::any_cast<int>(*result) == 5);
                }

                SECTION("no value") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    const auto task = race(
                        half(zero::async::coroutine::from(promise1)),
                        half(zero::async::coroutine::from(promise2)),
                        requireEven(zero::async::coroutine::from(promise3))
                    );
                    REQUIRE(!task.done());

                    promise3->resolve(100);
                    promise1->resolve(9);
                    promise2->resolve(99);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(result);
                    REQUIRE(!result->has_value());

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result->type() == typeid(void));
#endif
                }
            }

            SECTION("failure") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = race(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2)),
                    requireEven(zero::async::coroutine::from(promise3))
                );
                REQUIRE(!task.done());

                promise1->resolve(9);
                promise2->resolve(99);
                promise3->resolve(199);
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                try {
                    std::rethrow_exception(result.error());
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::invalid_argument);
                }
            }

            SECTION("throw") {
                const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                const auto task = race(
                    half(zero::async::coroutine::from(promise1)),
                    half(zero::async::coroutine::from(promise2)),
                    requireEven(zero::async::coroutine::from(promise3))
                );
                REQUIRE(!task.done());

                promise1->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::owner_dead))));
                promise2->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
                promise3->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
                REQUIRE(task.done());

                const auto &result = task.result();
                REQUIRE(!result);

                try {
                    std::rethrow_exception(result.error());
                }
                catch (const std::system_error &error) {
                    REQUIRE(error.code() == std::errc::owner_dead);
                }
            }

            SECTION("cancel") {
                SECTION("supported") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    auto task = race(
                        half(from(zero::async::coroutine::Cancellable{
                            promise1,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise1->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        })),
                        half(from(zero::async::coroutine::Cancellable{
                            promise2,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise2->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        })),
                        requireEven(from(zero::async::coroutine::Cancellable{
                            promise3,
                            [=]() mutable -> tl::expected<void, std::error_code> {
                                promise3->reject(std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))
                                ));
                                return {};
                            }
                        }))
                    );
                    REQUIRE(!task.done());

                    const auto &res = task.cancel();
                    REQUIRE(res);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    }
                }

                SECTION("not supported") {
                    const auto promise1 = zero::async::promise::make<int, std::exception_ptr>();
                    const auto promise2 = zero::async::promise::make<long, std::exception_ptr>();
                    const auto promise3 = zero::async::promise::make<int, std::exception_ptr>();

                    auto task = race(
                        half(zero::async::coroutine::from(promise1)),
                        half(zero::async::coroutine::from(promise2)),
                        requireEven(zero::async::coroutine::from(promise3))
                    );
                    REQUIRE(!task.done());

                    const auto &res = task.cancel();
                    REQUIRE(!res);
                    REQUIRE(res.error() == std::errc::operation_not_supported);

                    promise1->resolve(9);
                    promise2->resolve(100);
                    promise3->resolve(90);
                    REQUIRE(task.done());

                    const auto &result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    }
                }
            }
        }
    }
}
