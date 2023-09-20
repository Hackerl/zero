#include <zero/try.h>
#include <memory>
#include <tl/expected.hpp>
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

TEST_CASE("try helper macro", "[try]") {
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
