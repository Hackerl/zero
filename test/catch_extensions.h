#ifndef CATCH_EXTENSIONS_H
#define CATCH_EXTENSIONS_H

#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS

#include <fmt/std.h>
#include <zero/formatter.h>
#include <catch2/catch_test_macros.hpp>

template<>
struct Catch::StringMaker<std::exception_ptr> {
    static std::string convert(const std::exception_ptr &ptr) {
        return fmt::to_string(ptr);
    }
};

template<typename T, typename E>
struct Catch::StringMaker<tl::expected<T, E>> {
    static std::string convert(const tl::expected<T, E> &expected) {
        if (!expected)
            return fmt::format("unexpected({})", StringMaker<E>::convert(expected.error()));

        if constexpr (std::is_void_v<T>)
            return "expected()";
        else
            return fmt::format("expected({})", StringMaker<T>::convert(*expected));
    }
};

template<>
struct Catch::StringMaker<std::error_code> {
    static std::string convert(const std::error_code &ec) {
        return fmt::format("{} ({})", ec.message(), ec);
    }
};

template<typename T>
struct Catch::StringMaker<T, std::enable_if_t<std::is_error_code_enum_v<T>>> {
    static std::string convert(const T &error) {
        return StringMaker<std::error_code>::convert(error);
    }
};

template<>
struct Catch::StringMaker<std::error_condition> {
    static std::string convert(const std::error_condition &condition) {
        return fmt::format("{} ({}:{})", condition.message(), condition.category().name(), condition.value());
    }
};

template<typename T>
struct Catch::StringMaker<T,  std::enable_if_t<std::is_error_condition_enum_v<T>>> {
    static std::string convert(const T &error) {
        return StringMaker<std::error_condition>::convert(error);
    }
};

#define INTERNAL_REQUIRE_ERROR(var, expr, err)  \
    const auto var = expr;                      \
    REQUIRE_FALSE(var);                         \
    REQUIRE((var).error() == (err))

#define REQUIRE_ERROR(expr, err) INTERNAL_REQUIRE_ERROR(INTERNAL_CATCH_UNIQUE_NAME(_result), expr, err)

#endif //CATCH_EXTENSIONS_H
