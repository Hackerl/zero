#include <zero/try.h>
#include <zero/async/coroutine.h>
#include <catch2/catch_test_macros.hpp>

tl::expected<std::unique_ptr<int>, std::error_code> func1(int value) {
    return std::make_unique<int>(value * 2);
}

tl::expected<std::unique_ptr<int>, std::error_code> func2(int value) {
    return std::make_unique<int>(value * 4);
}

tl::expected<std::unique_ptr<int>, std::error_code> func3(int) {
    return tl::unexpected(make_error_code(std::errc::operation_canceled));
}

tl::expected<std::unique_ptr<int>, std::error_code> func4(int) {
    return tl::unexpected(make_error_code(std::errc::timed_out));
}

zero::async::coroutine::Task<std::unique_ptr<int>, std::error_code> func5(int value) {
    co_return std::make_unique<int>(value * 2);
}

zero::async::coroutine::Task<std::unique_ptr<int>, std::error_code> func6(int value) {
    co_return std::make_unique<int>(value * 4);
}

zero::async::coroutine::Task<std::unique_ptr<int>, std::error_code> func7(int value) {
    co_return tl::unexpected(make_error_code(std::errc::operation_canceled));
}

zero::async::coroutine::Task<std::unique_ptr<int>, std::error_code> func8(int value) {
    co_return tl::unexpected(make_error_code(std::errc::timed_out));
}

tl::expected<int, std::error_code> test1() {
    auto result = TRY(func1(2));
    result = TRY(func2(**result));
    return **result * 10;
}

tl::expected<int, std::error_code> test2() {
    auto result = TRY(func1(2));
    result = TRY(func2(**result));
    result = TRY(func3(**result));
    return **result * 10;
}

tl::expected<int, std::error_code> test3() {
    auto result = TRY(func1(2));
    result = TRY(func2(**result));
    result = TRY(func4(**result));
    return **result * 10;
}

tl::expected<int, std::error_code> test4() {
    auto result = TRY(func4(2));
    result = TRY(func1(**result));
    result = TRY(func2(**result));
    return **result * 10;
}

zero::async::coroutine::Task<int, std::error_code> test5() {
    auto result = CO_TRY(std::move(co_await func5(2)));
    result = CO_TRY(std::move(co_await func6(**result)));
    co_return **result * 10;
}

zero::async::coroutine::Task<int, std::error_code> test6() {
    auto result = CO_TRY(std::move(co_await func5(2)));
    result = CO_TRY(std::move(co_await func6(**result)));
    result = CO_TRY(std::move(co_await func7(**result)));
    co_return **result * 10;
}

zero::async::coroutine::Task<int, std::error_code> test7() {
    auto result = CO_TRY(std::move(co_await func5(2)));
    result = CO_TRY(std::move(co_await func6(**result)));
    result = CO_TRY(std::move(co_await func8(**result)));
    co_return **result * 10;
}

zero::async::coroutine::Task<int, std::error_code> test8() {
    auto result = CO_TRY(std::move(co_await func8(2)));
    result = CO_TRY(std::move(co_await func5(**result)));
    result = CO_TRY(std::move(co_await func6(**result)));
    co_return **result * 10;
}

TEST_CASE("try helper macro", "[try]") {
    SECTION("normal") {
        auto result = test1();
        REQUIRE(result);
        REQUIRE(*result == 160);

        result = test2();
        REQUIRE(!result);
        REQUIRE(result.error() == std::errc::operation_canceled);

        result = test3();
        REQUIRE(!result);
        REQUIRE(result.error() == std::errc::timed_out);

        result = test4();
        REQUIRE(!result);
        REQUIRE(result.error() == std::errc::timed_out);
    }

    SECTION("coroutine") {
        auto task = test5();
        REQUIRE(task.done());
        REQUIRE(*task.result() == 160);

        task = test6();
        REQUIRE(task.done());
        REQUIRE(task.result().error() == std::errc::operation_canceled);

        task = test7();
        REQUIRE(task.done());
        REQUIRE(task.result().error() == std::errc::timed_out);

        task = test8();
        REQUIRE(task.done());
        REQUIRE(task.result().error() == std::errc::timed_out);
    }
}
