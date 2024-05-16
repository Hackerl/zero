#include <zero/error.h>
#include <catch2/catch_test_macros.hpp>

std::string stringify(const int value) {
    std::string msg;

    switch (value) {
    case EINVAL:
        msg = "invalid argument";
        break;

    case ETIMEDOUT:
        msg = "timeout";
        break;

    default:
        msg = "unknown";
        break;
    }

    return msg;
}

DEFINE_ERROR_CODE(
    A,
    "A",
    INVALID_ARGUMENT, "invalid argument",
    TIMEOUT, "timeout"
)

DECLARE_ERROR_CODE(A)

DEFINE_ERROR_CODE_EX(
    B,
    "B",
    INVALID_ARGUMENT, "invalid argument", std::errc::invalid_argument,
    TIMEOUT, "timeout", std::errc::timed_out
)

DECLARE_ERROR_CODE(B)

DEFINE_ERROR_TRANSFORMER(
    C,
    "C",
    stringify
)

DECLARE_ERROR_CODE(C)

DEFINE_ERROR_TRANSFORMER_EX(
    D,
    "D",
    stringify,
    [](const int value) -> std::optional<std::error_condition> {
        std::optional<std::error_condition> condition;

        switch (value) {
        case EINVAL:
            condition = std::errc::invalid_argument;
            break;

        case ETIMEDOUT:
            condition = std::errc::timed_out;
            break;

        default:
            break;
        }

        return condition;
    }
)

DECLARE_ERROR_CODE(D)

DEFINE_ERROR_CONDITION(
    E,
    "E",
    INVALID_ARGUMENT,
    "invalid argument",
    [](const std::error_code &ec) {
        return ec == A::INVALID_ARGUMENT ||
            ec == B::INVALID_ARGUMENT ||
            ec == static_cast<C>(EINVAL) ||
            ec == static_cast<D>(EINVAL);
    },
    TIMEOUT,
    "timeout",
    [](const std::error_code &ec) {
        return ec == A::TIMEOUT ||
            ec == B::TIMEOUT ||
            ec == static_cast<C>(ETIMEDOUT) ||
            ec == static_cast<D>(ETIMEDOUT);
    }
)

DECLARE_ERROR_CONDITION(E)

TEST_CASE("macro for define error code", "[error]") {
    std::error_code ec = A::INVALID_ARGUMENT;
    REQUIRE(strcmp(ec.category().name(), "A") == 0);
    REQUIRE(ec.message() == "invalid argument");

    ec = A::TIMEOUT;
    REQUIRE(strcmp(ec.category().name(), "A") == 0);
    REQUIRE(ec.message() == "timeout");

    ec = B::INVALID_ARGUMENT;
    REQUIRE(strcmp(ec.category().name(), "B") == 0);
    REQUIRE(ec.message() == "invalid argument");
    REQUIRE(ec == std::errc::invalid_argument);

    ec = B::TIMEOUT;
    REQUIRE(strcmp(ec.category().name(), "B") == 0);
    REQUIRE(ec.message() == "timeout");
    REQUIRE(ec == std::errc::timed_out);

    ec = static_cast<C>(EINVAL);
    REQUIRE(strcmp(ec.category().name(), "C") == 0);
    REQUIRE(ec.message() == "invalid argument");

    ec = static_cast<C>(ETIMEDOUT);
    REQUIRE(strcmp(ec.category().name(), "C") == 0);
    REQUIRE(ec.message() == "timeout");

    ec = static_cast<D>(EINVAL);
    REQUIRE(strcmp(ec.category().name(), "D") == 0);
    REQUIRE(ec.message() == "invalid argument");
    REQUIRE(ec == std::errc::invalid_argument);

    ec = static_cast<D>(ETIMEDOUT);
    REQUIRE(strcmp(ec.category().name(), "D") == 0);
    REQUIRE(ec.message() == "timeout");
    REQUIRE(ec == std::errc::timed_out);

    std::error_condition condition = E::INVALID_ARGUMENT;
    REQUIRE(strcmp(condition.category().name(), "E") == 0);
    REQUIRE(condition.message() == "invalid argument");
    REQUIRE(condition == A::INVALID_ARGUMENT);
    REQUIRE(condition == B::INVALID_ARGUMENT);
    REQUIRE(condition == static_cast<C>(EINVAL));
    REQUIRE(condition == static_cast<D>(EINVAL));

    condition = E::TIMEOUT;
    REQUIRE(strcmp(condition.category().name(), "E") == 0);
    REQUIRE(condition.message() == "timeout");
    REQUIRE(condition == A::TIMEOUT);
    REQUIRE(condition == B::TIMEOUT);
    REQUIRE(condition == static_cast<C>(ETIMEDOUT));
    REQUIRE(condition == static_cast<D>(ETIMEDOUT));
}
