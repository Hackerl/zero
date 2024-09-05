#include <zero/expect.h>
#include <expected>
#include <catch2/catch_test_macros.hpp>

std::expected<std::unique_ptr<int>, std::error_code> func1(const int value) {
    return std::make_unique<int>(value * 2);
}

std::expected<std::unique_ptr<int>, std::error_code> func2(const int value) {
    return std::make_unique<int>(value * 4);
}

std::expected<std::unique_ptr<int>, std::error_code> func3(const int) {
    return std::unexpected(make_error_code(std::errc::operation_canceled));
}

std::expected<std::unique_ptr<int>, std::error_code> func4(const int) {
    return std::unexpected(make_error_code(std::errc::timed_out));
}

std::expected<int, std::error_code> test1() {
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

std::expected<int, std::error_code> test2() {
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

std::expected<int, std::error_code> test3() {
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

std::expected<int, std::error_code> test4() {
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

TEST_CASE("macro for error handling", "[expect]") {
    auto result = test1();
    REQUIRE(result);
    REQUIRE(*result == 160);

    result = test2();
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == std::errc::operation_canceled);

    result = test3();
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == std::errc::timed_out);

    result = test4();
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == std::errc::timed_out);
}
