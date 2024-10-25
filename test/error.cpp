#include <zero/error.h>
#include <catch2/catch_test_macros.hpp>

std::string stringify(const int value) {
    switch (value) {
    case EINVAL:
        return "invalid argument";

    case ETIMEDOUT:
        return "timeout";

    default:
        return "unknown";
    }
}

DEFINE_ERROR_CODE(
    ErrorCode,
    "ErrorCode",
    INVALID_ARGUMENT, "invalid argument",
    TIMEOUT, "timeout"
)

DECLARE_ERROR_CODE(ErrorCode)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCode)

DEFINE_ERROR_CONDITION(
    ErrorCondition,
    "ErrorCondition",
    INVALID_ARGUMENT, "invalid argument",
    TIMEOUT, "timeout"
)

DECLARE_ERROR_CONDITION(ErrorCondition)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCondition)

DEFINE_ERROR_CODE_EX(
    ErrorCodeEx,
    "ErrorCodeEx",
    INVALID_ARGUMENT, "invalid argument", ErrorCondition::INVALID_ARGUMENT,
    TIMEOUT, "timeout", ErrorCondition::TIMEOUT
)

DECLARE_ERROR_CODE(ErrorCodeEx)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCodeEx)

DEFINE_ERROR_TRANSFORMER(
    ErrorTransformer,
    "ErrorTransformer",
    stringify
)

DECLARE_ERROR_CODE(ErrorTransformer)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorTransformer)

DEFINE_ERROR_TRANSFORMER_EX(
    ErrorTransformerEx,
    "ErrorTransformerEx",
    stringify,
    [](const int value) -> std::optional<std::error_condition> {
        switch (value) {
        case EINVAL:
            return ErrorCondition::INVALID_ARGUMENT;

        case ETIMEDOUT:
            return ErrorCondition::TIMEOUT;

        default:
            return std::nullopt;
        }
    }
)

DECLARE_ERROR_CODE(ErrorTransformerEx)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorTransformerEx)

DEFINE_ERROR_CONDITION_EX(
    ErrorConditionEx,
    "ErrorConditionEx",
    INVALID_ARGUMENT,
    "invalid argument",
    [](const std::error_code &ec) {
        return ec == ErrorCode::INVALID_ARGUMENT ||
            ec == ErrorCodeEx::INVALID_ARGUMENT ||
            ec == static_cast<ErrorTransformer>(EINVAL) ||
            ec == static_cast<ErrorTransformerEx>(EINVAL);
    },
    TIMEOUT,
    "timeout",
    [](const std::error_code &ec) {
        return ec == ErrorCode::TIMEOUT ||
            ec == ErrorCodeEx::TIMEOUT ||
            ec == static_cast<ErrorTransformer>(ETIMEDOUT) ||
            ec == static_cast<ErrorTransformerEx>(ETIMEDOUT);
    }
)

DECLARE_ERROR_CONDITION(ErrorConditionEx)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorConditionEx)

struct ErrorCodeWrapper {
    DEFINE_ERROR_CODE_INNER(
        ErrorCode,
        "ErrorCode",
        INVALID_ARGUMENT, "invalid argument",
        TIMEOUT, "timeout"
    )
};

DECLARE_ERROR_CODE(ErrorCodeWrapper::ErrorCode)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCodeWrapper::ErrorCode)

struct ErrorConditionWrapper {
    DEFINE_ERROR_CONDITION_INNER(
        ErrorCondition,
        "ErrorCondition",
        INVALID_ARGUMENT, "invalid argument",
        TIMEOUT, "timeout"
    )
};

DECLARE_ERROR_CONDITION(ErrorConditionWrapper::ErrorCondition)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorConditionWrapper::ErrorCondition)

struct ErrorCodeExWrapper {
    DEFINE_ERROR_CODE_INNER_EX(
        ErrorCodeEx,
        "ErrorCodeEx",
        INVALID_ARGUMENT, "invalid argument", ErrorConditionWrapper::ErrorCondition::INVALID_ARGUMENT,
        TIMEOUT, "timeout", ErrorConditionWrapper::ErrorCondition::TIMEOUT
    )
};

DECLARE_ERROR_CODE(ErrorCodeExWrapper::ErrorCodeEx)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCodeExWrapper::ErrorCodeEx)

struct ErrorTransformerWrapper {
    DEFINE_ERROR_TRANSFORMER_INNER(
        ErrorTransformer,
        "ErrorTransformer",
        stringify
    )
};

DECLARE_ERROR_CODE(ErrorTransformerWrapper::ErrorTransformer)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorTransformerWrapper::ErrorTransformer)

struct ErrorTransformerExWrapper {
    DEFINE_ERROR_TRANSFORMER_INNER_EX(
        ErrorTransformerEx,
        "ErrorTransformerEx",
        stringify,
        [](const int value) -> std::optional<std::error_condition> {
            switch (value) {
            case EINVAL:
                return ErrorConditionWrapper::ErrorCondition::INVALID_ARGUMENT;

            case ETIMEDOUT:
                return ErrorConditionWrapper::ErrorCondition::TIMEOUT;

            default:
                return std::nullopt;
            }
        }
    )
};

DECLARE_ERROR_CODE(ErrorTransformerExWrapper::ErrorTransformerEx)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorTransformerExWrapper::ErrorTransformerEx)

struct ErrorConditionExWrapper {
    DEFINE_ERROR_CONDITION_INNER_EX(
        ErrorConditionEx,
        "ErrorConditionEx",
        INVALID_ARGUMENT,
        "invalid argument",
        [](const std::error_code &ec) {
            return ec == ErrorCodeWrapper::ErrorCode::INVALID_ARGUMENT ||
                ec == ErrorCodeExWrapper::ErrorCodeEx::INVALID_ARGUMENT ||
                ec == static_cast<ErrorTransformerWrapper::ErrorTransformer>(EINVAL) ||
                ec == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(EINVAL);
        },
        TIMEOUT,
        "timeout",
        [](const std::error_code &ec) {
            return ec == ErrorCodeWrapper::ErrorCode::TIMEOUT ||
                ec == ErrorCodeExWrapper::ErrorCodeEx::TIMEOUT ||
                ec == static_cast<ErrorTransformerWrapper::ErrorTransformer>(ETIMEDOUT) ||
                ec == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(ETIMEDOUT);
        }
    )
};

DECLARE_ERROR_CONDITION(ErrorConditionExWrapper::ErrorConditionEx)
DEFINE_ERROR_CATEGORY_INSTANCE(ErrorConditionExWrapper::ErrorConditionEx)

TEST_CASE("custom error code", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{ErrorCode::INVALID_ARGUMENT};
        REQUIRE(ec.category().name() == "ErrorCode"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionEx::INVALID_ARGUMENT);
    }

    SECTION("timeout") {
        std::error_code ec{ErrorCode::TIMEOUT};
        REQUIRE(ec.category().name() == "ErrorCode"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionEx::TIMEOUT);
    }
}

TEST_CASE("custom extended error code", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{ErrorCodeEx::INVALID_ARGUMENT};
        REQUIRE(ec.category().name() == "ErrorCodeEx"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorCondition::INVALID_ARGUMENT);
        REQUIRE(ec == ErrorConditionEx::INVALID_ARGUMENT);
    }

    SECTION("timeout") {
        std::error_code ec{ErrorCodeEx::TIMEOUT};
        REQUIRE(ec.category().name() == "ErrorCodeEx"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorCondition::TIMEOUT);
        REQUIRE(ec == ErrorConditionEx::TIMEOUT);
    }
}

TEST_CASE("custom error transformer", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{static_cast<ErrorTransformer>(EINVAL)};
        REQUIRE(ec.category().name() == "ErrorTransformer"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionEx::INVALID_ARGUMENT);
    }

    SECTION("timeout") {
        std::error_code ec{static_cast<ErrorTransformer>(ETIMEDOUT)};
        REQUIRE(ec.category().name() == "ErrorTransformer"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionEx::TIMEOUT);
    }
}

TEST_CASE("custom extended error transformer", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{static_cast<ErrorTransformerEx>(EINVAL)};
        REQUIRE(ec.category().name() == "ErrorTransformerEx"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorCondition::INVALID_ARGUMENT);
        REQUIRE(ec == ErrorConditionEx::INVALID_ARGUMENT);
    }

    SECTION("timeout") {
        std::error_code ec{static_cast<ErrorTransformerEx>(ETIMEDOUT)};
        REQUIRE(ec.category().name() == "ErrorTransformerEx"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorCondition::TIMEOUT);
        REQUIRE(ec == ErrorConditionEx::TIMEOUT);
    }
}

TEST_CASE("custom error condition", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_condition condition{ErrorCondition::INVALID_ARGUMENT};
        REQUIRE(condition.category().name() == "ErrorCondition"sv);
        REQUIRE(condition.message() == "invalid argument");
        REQUIRE(condition == ErrorCodeEx::INVALID_ARGUMENT);
        REQUIRE(condition == static_cast<ErrorTransformerEx>(EINVAL));
    }

    SECTION("timeout") {
        std::error_condition condition{ErrorCondition::TIMEOUT};
        REQUIRE(condition.category().name() == "ErrorCondition"sv);
        REQUIRE(condition.message() == "timeout");
        REQUIRE(condition == ErrorCodeEx::TIMEOUT);
        REQUIRE(condition == static_cast<ErrorTransformerEx>(ETIMEDOUT));
    }
}

TEST_CASE("custom extended error condition", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_condition condition{ErrorConditionEx::INVALID_ARGUMENT};
        REQUIRE(condition.category().name() == "ErrorConditionEx"sv);
        REQUIRE(condition.message() == "invalid argument");
        REQUIRE(condition == ErrorCode::INVALID_ARGUMENT);
        REQUIRE(condition == ErrorCodeEx::INVALID_ARGUMENT);
        REQUIRE(condition == static_cast<ErrorTransformer>(EINVAL));
        REQUIRE(condition == static_cast<ErrorTransformerEx>(EINVAL));
    }

    SECTION("timeout") {
        std::error_condition condition{ErrorConditionEx::TIMEOUT};
        REQUIRE(condition.category().name() == "ErrorConditionEx"sv);
        REQUIRE(condition.message() == "timeout");
        REQUIRE(condition == ErrorCode::TIMEOUT);
        REQUIRE(condition == ErrorCodeEx::TIMEOUT);
        REQUIRE(condition == static_cast<ErrorTransformer>(ETIMEDOUT));
        REQUIRE(condition == static_cast<ErrorTransformerEx>(ETIMEDOUT));
    }
}

TEST_CASE("custom error code defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{ErrorCodeWrapper::ErrorCode::INVALID_ARGUMENT};
        REQUIRE(ec.category().name() == "ErrorCode"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::INVALID_ARGUMENT);
    }

    SECTION("timeout") {
        std::error_code ec{ErrorCodeWrapper::ErrorCode::TIMEOUT};
        REQUIRE(ec.category().name() == "ErrorCode"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::TIMEOUT);
    }
}

TEST_CASE("custom extended error code defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{ErrorCodeExWrapper::ErrorCodeEx::INVALID_ARGUMENT};
        REQUIRE(ec.category().name() == "ErrorCodeEx"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionWrapper::ErrorCondition::INVALID_ARGUMENT);
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::INVALID_ARGUMENT);
    }

    SECTION("timeout") {
        std::error_code ec{ErrorCodeExWrapper::ErrorCodeEx::TIMEOUT};
        REQUIRE(ec.category().name() == "ErrorCodeEx"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionWrapper::ErrorCondition::TIMEOUT);
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::TIMEOUT);
    }
}

TEST_CASE("custom error transformer defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{static_cast<ErrorTransformerWrapper::ErrorTransformer>(EINVAL)};
        REQUIRE(ec.category().name() == "ErrorTransformer"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::INVALID_ARGUMENT);
    }

    SECTION("timeout") {
        std::error_code ec{static_cast<ErrorTransformerWrapper::ErrorTransformer>(ETIMEDOUT)};
        REQUIRE(ec.category().name() == "ErrorTransformer"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::TIMEOUT);
    }
}

TEST_CASE("custom extended error transformer defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(EINVAL)};
        REQUIRE(ec.category().name() == "ErrorTransformerEx"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionWrapper::ErrorCondition::INVALID_ARGUMENT);
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::INVALID_ARGUMENT);
    }

    SECTION("timeout") {
        std::error_code ec{static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(ETIMEDOUT)};
        REQUIRE(ec.category().name() == "ErrorTransformerEx"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionWrapper::ErrorCondition::TIMEOUT);
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::TIMEOUT);
    }
}

TEST_CASE("custom error condition defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_condition condition{ErrorConditionWrapper::ErrorCondition::INVALID_ARGUMENT};
        REQUIRE(condition.category().name() == "ErrorCondition"sv);
        REQUIRE(condition.message() == "invalid argument");
        REQUIRE(condition == ErrorCodeExWrapper::ErrorCodeEx::INVALID_ARGUMENT);
        REQUIRE(condition == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(EINVAL));
    }

    SECTION("timeout") {
        std::error_condition condition{ErrorConditionWrapper::ErrorCondition::TIMEOUT};
        REQUIRE(condition.category().name() == "ErrorCondition"sv);
        REQUIRE(condition.message() == "timeout");
        REQUIRE(condition == ErrorCodeExWrapper::ErrorCodeEx::TIMEOUT);
        REQUIRE(condition == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(ETIMEDOUT));
    }
}

TEST_CASE("custom extended error condition defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_condition condition{ErrorConditionExWrapper::ErrorConditionEx::INVALID_ARGUMENT};
        REQUIRE(condition.category().name() == "ErrorConditionEx"sv);
        REQUIRE(condition.message() == "invalid argument");
        REQUIRE(condition == ErrorCodeWrapper::ErrorCode::INVALID_ARGUMENT);
        REQUIRE(condition == ErrorCodeExWrapper::ErrorCodeEx::INVALID_ARGUMENT);
        REQUIRE(condition == static_cast<ErrorTransformerWrapper::ErrorTransformer>(EINVAL));
        REQUIRE(condition == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(EINVAL));
    }

    SECTION("timeout") {
        std::error_condition condition{ErrorConditionExWrapper::ErrorConditionEx::TIMEOUT};
        REQUIRE(condition.category().name() == "ErrorConditionEx"sv);
        REQUIRE(condition.message() == "timeout");
        REQUIRE(condition == ErrorCodeWrapper::ErrorCode::TIMEOUT);
        REQUIRE(condition == ErrorCodeExWrapper::ErrorCodeEx::TIMEOUT);
        REQUIRE(condition == static_cast<ErrorTransformerWrapper::ErrorTransformer>(ETIMEDOUT));
        REQUIRE(condition == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(ETIMEDOUT));
    }
}
