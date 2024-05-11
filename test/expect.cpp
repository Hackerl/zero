#include <zero/expect.h>
#include <zero/async/coroutine.h>
#include <catch2/catch_test_macros.hpp>

tl::expected<std::unique_ptr<int>, std::error_code> func1(const int value) {
    return std::make_unique<int>(value * 2);
}

tl::expected<std::unique_ptr<int>, std::error_code> func2(const int value) {
    return std::make_unique<int>(value * 4);
}

tl::expected<std::unique_ptr<int>, std::error_code> func3(const int) {
    return tl::unexpected(make_error_code(std::errc::operation_canceled));
}

tl::expected<std::unique_ptr<int>, std::error_code> func4(const int) {
    return tl::unexpected(make_error_code(std::errc::timed_out));
}

zero::async::coroutine::Task<std::unique_ptr<int>, std::error_code> func5(const int value) {
    co_return std::make_unique<int>(value * 2);
}

zero::async::coroutine::Task<std::unique_ptr<int>, std::error_code> func6(const int value) {
    co_return std::make_unique<int>(value * 4);
}

zero::async::coroutine::Task<std::unique_ptr<int>, std::error_code> func7(const int) {
    co_return tl::unexpected(make_error_code(std::errc::operation_canceled));
}

zero::async::coroutine::Task<std::unique_ptr<int>, std::error_code> func8(const int) {
    co_return tl::unexpected(make_error_code(std::errc::timed_out));
}

tl::expected<int, std::error_code> test1() {
#ifdef __GNUC__
    auto result = TRY(func1(2));
    result = TRY(func2(*result));
    return *result * 10;
#else
    auto result = func1(2);
    EXPECT(result);
    result = func2(**result);
    EXPECT(result);
    return **result * 10;
#endif
}

tl::expected<int, std::error_code> test2() {
#ifdef __GNUC__
    auto result = TRY(func1(2));
    result = TRY(func2(*result));
    result = TRY(func3(*result));
    return *result * 10;
#else
    auto result = func1(2);
    EXPECT(result);
    result = func2(**result);
    EXPECT(result);
    result = func3(**result);
    EXPECT(result);
    return **result * 10;
#endif
}

tl::expected<int, std::error_code> test3() {
#ifdef __GNUC__
    auto result = TRY(func1(2));
    result = TRY(func2(*result));
    result = TRY(func4(*result));
    return *result * 10;
#else
    auto result = func1(2);
    EXPECT(result);
    result = func2(**result);
    EXPECT(result);
    result = func4(**result);
    EXPECT(result);
    return **result * 10;
#endif
}

tl::expected<int, std::error_code> test4() {
#ifdef __GNUC__
    auto result = TRY(func4(2));
    result = TRY(func1(*result));
    result = TRY(func2(*result));
    return *result * 10;
#else
    auto result = func4(2);
    EXPECT(result);
    result = func1(**result);
    EXPECT(result);
    result = func2(**result);
    EXPECT(result);
    return **result * 10;
#endif
}

zero::async::coroutine::Task<int, std::error_code> test5() {
#ifdef __clang__
    auto result = CO_TRY(co_await func5(2));
    result = CO_TRY(co_await func6(*result));
    co_return *result * 10;
#else
    auto result = co_await func5(2);
    CO_EXPECT(result);
    result = co_await func6(**result);
    CO_EXPECT(result);
    co_return **result * 10;
#endif
}

zero::async::coroutine::Task<int, std::error_code> test6() {
#ifdef __clang__
    auto result = CO_TRY(co_await func5(2));
    result = CO_TRY(co_await func6(*result));
    result = CO_TRY(co_await func7(*result));
    co_return *result * 10;
#else
    auto result = co_await func5(2);
    CO_EXPECT(result);
    result = co_await func6(**result);
    CO_EXPECT(result);
    result = co_await func7(**result);
    CO_EXPECT(result);
    co_return **result * 10;
#endif
}

zero::async::coroutine::Task<int, std::error_code> test7() {
#ifdef __clang__
    auto result = CO_TRY(co_await func5(2));
    result = CO_TRY(co_await func6(*result));
    result = CO_TRY(co_await func8(*result));
    co_return *result * 10;
#else
    auto result = co_await func5(2);
    CO_EXPECT(result);
    result = co_await func6(**result);
    CO_EXPECT(result);
    result = co_await func8(**result);
    CO_EXPECT(result);
    co_return **result * 10;
#endif
}

zero::async::coroutine::Task<int, std::error_code> test8() {
#ifdef __clang__
    auto result = CO_TRY(co_await func8(2));
    result = CO_TRY(co_await func5(*result));
    result = CO_TRY(co_await func6(*result));
    co_return *result * 10;
#else
    auto result = co_await func8(2);
    CO_EXPECT(result);
    result = co_await func5(**result);
    CO_EXPECT(result);
    result = co_await func6(**result);
    CO_EXPECT(result);
    co_return **result * 10;
#endif
}

TEST_CASE("macro for error handling", "[expect]") {
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
        REQUIRE(task.future().result() == 160);

        task = test6();
        REQUIRE(task.done());
        REQUIRE(task.future().result().error() == std::errc::operation_canceled);

        task = test7();
        REQUIRE(task.done());
        REQUIRE(task.future().result().error() == std::errc::timed_out);

        task = test8();
        REQUIRE(task.done());
        REQUIRE(task.future().result().error() == std::errc::timed_out);
    }
}
