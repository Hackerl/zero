#include <zero/error.h>
#include <catch2/catch_test_macros.hpp>

const char *stringify(const int value) {
    if (value == EINVAL)
        return "invalid argument";

    if (value == ETIMEDOUT)
        return "timeout";

    return "unknown";
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

TRANSFORM_ERROR_CODE(
    C,
    "C",
    stringify
)

DECLARE_ERROR_CODE(C)

TRANSFORM_ERROR_CODE_EX(
    D,
    "D",
    stringify,
    EINVAL, std::errc::invalid_argument,
    ETIMEDOUT, std::errc::timed_out
)

DECLARE_ERROR_CODE(D)

DEFINE_ERROR_CONDITION(
    E,
    "E",
    INVALID_ARGUMENT,
    "invalid argument",
    code == A::INVALID_ARGUMENT ||
    code == B::INVALID_ARGUMENT ||
    code == static_cast<C>(EINVAL) ||
    code == static_cast<D>(EINVAL),
    TIMEOUT,
    "timeout",
    code == A::TIMEOUT ||
    code == B::TIMEOUT ||
    code == static_cast<C>(ETIMEDOUT) ||
    code == static_cast<D>(ETIMEDOUT)
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
