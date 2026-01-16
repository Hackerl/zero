#include "catch_extensions.h"
#include <zero/error.h>

namespace {
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
}

Z_DEFINE_ERROR_CODE(
    ErrorCode,
    "ErrorCode",
    InvalidArgument, "invalid argument",
    Timeout, "timeout"
)

Z_DECLARE_ERROR_CODE(ErrorCode)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCode)

Z_DEFINE_ERROR_CONDITION(
    ErrorCondition,
    "ErrorCondition",
    InvalidArgument, "invalid argument",
    Timeout, "timeout"
)

Z_DECLARE_ERROR_CONDITION(ErrorCondition)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCondition)

Z_DEFINE_ERROR_CODE_EX(
    ErrorCodeEx,
    "ErrorCodeEx",
    InvalidArgument, "invalid argument", ErrorCondition::InvalidArgument,
    Timeout, "timeout", ErrorCondition::Timeout
)

Z_DECLARE_ERROR_CODE(ErrorCodeEx)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCodeEx)

Z_DEFINE_ERROR_TRANSFORMER(
    ErrorTransformer,
    "ErrorTransformer",
    stringify
)

Z_DECLARE_ERROR_CODE(ErrorTransformer)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorTransformer)

Z_DEFINE_ERROR_TRANSFORMER_EX(
    ErrorTransformerEx,
    "ErrorTransformerEx",
    stringify,
    [](const int value) -> std::optional<std::error_condition> {
        switch (value) {
        case EINVAL:
            return ErrorCondition::InvalidArgument;

        case ETIMEDOUT:
            return ErrorCondition::Timeout;

        default:
            return std::nullopt;
        }
    }
)

Z_DECLARE_ERROR_CODE(ErrorTransformerEx)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorTransformerEx)

Z_DEFINE_ERROR_CONDITION_EX(
    ErrorConditionEx,
    "ErrorConditionEx",
    InvalidArgument,
    "invalid argument",
    [](const std::error_code &ec) {
        return ec == ErrorCode::InvalidArgument ||
            ec == ErrorCodeEx::InvalidArgument ||
            ec == static_cast<ErrorTransformer>(EINVAL) ||
            ec == static_cast<ErrorTransformerEx>(EINVAL);
    },
    Timeout,
    "timeout",
    [](const std::error_code &ec) {
        return ec == ErrorCode::Timeout ||
            ec == ErrorCodeEx::Timeout ||
            ec == static_cast<ErrorTransformer>(ETIMEDOUT) ||
            ec == static_cast<ErrorTransformerEx>(ETIMEDOUT);
    }
)

Z_DECLARE_ERROR_CONDITION(ErrorConditionEx)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorConditionEx)

struct ErrorCodeWrapper {
    Z_DEFINE_ERROR_CODE_INNER(
        ErrorCode,
        "ErrorCode",
        InvalidArgument, "invalid argument",
        Timeout, "timeout"
    )
};

Z_DECLARE_ERROR_CODE(ErrorCodeWrapper::ErrorCode)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCodeWrapper::ErrorCode)

struct ErrorConditionWrapper {
    Z_DEFINE_ERROR_CONDITION_INNER(
        ErrorCondition,
        "ErrorCondition",
        InvalidArgument, "invalid argument",
        Timeout, "timeout"
    )
};

Z_DECLARE_ERROR_CONDITION(ErrorConditionWrapper::ErrorCondition)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorConditionWrapper::ErrorCondition)

struct ErrorCodeExWrapper {
    Z_DEFINE_ERROR_CODE_INNER_EX(
        ErrorCodeEx,
        "ErrorCodeEx",
        InvalidArgument, "invalid argument", ErrorConditionWrapper::ErrorCondition::InvalidArgument,
        Timeout, "timeout", ErrorConditionWrapper::ErrorCondition::Timeout
    )
};

Z_DECLARE_ERROR_CODE(ErrorCodeExWrapper::ErrorCodeEx)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorCodeExWrapper::ErrorCodeEx)

struct ErrorTransformerWrapper {
    Z_DEFINE_ERROR_TRANSFORMER_INNER(
        ErrorTransformer,
        "ErrorTransformer",
        stringify
    )
};

Z_DECLARE_ERROR_CODE(ErrorTransformerWrapper::ErrorTransformer)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorTransformerWrapper::ErrorTransformer)

struct ErrorTransformerExWrapper {
    Z_DEFINE_ERROR_TRANSFORMER_INNER_EX(
        ErrorTransformerEx,
        "ErrorTransformerEx",
        stringify,
        [](const int value) -> std::optional<std::error_condition> {
            switch (value) {
            case EINVAL:
                return ErrorConditionWrapper::ErrorCondition::InvalidArgument;

            case ETIMEDOUT:
                return ErrorConditionWrapper::ErrorCondition::Timeout;

            default:
                return std::nullopt;
            }
        }
    )
};

Z_DECLARE_ERROR_CODE(ErrorTransformerExWrapper::ErrorTransformerEx)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorTransformerExWrapper::ErrorTransformerEx)

struct ErrorConditionExWrapper {
    Z_DEFINE_ERROR_CONDITION_INNER_EX(
        ErrorConditionEx,
        "ErrorConditionEx",
        InvalidArgument,
        "invalid argument",
        [](const std::error_code &ec) {
            return ec == ErrorCodeWrapper::ErrorCode::InvalidArgument ||
                ec == ErrorCodeExWrapper::ErrorCodeEx::InvalidArgument ||
                ec == static_cast<ErrorTransformerWrapper::ErrorTransformer>(EINVAL) ||
                ec == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(EINVAL);
        },
        Timeout,
        "timeout",
        [](const std::error_code &ec) {
            return ec == ErrorCodeWrapper::ErrorCode::Timeout ||
                ec == ErrorCodeExWrapper::ErrorCodeEx::Timeout ||
                ec == static_cast<ErrorTransformerWrapper::ErrorTransformer>(ETIMEDOUT) ||
                ec == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(ETIMEDOUT);
        }
    )
};

Z_DECLARE_ERROR_CONDITION(ErrorConditionExWrapper::ErrorConditionEx)

Z_DEFINE_ERROR_CATEGORY_INSTANCE(ErrorConditionExWrapper::ErrorConditionEx)

TEST_CASE("custom error code", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{ErrorCode::InvalidArgument};
        REQUIRE(ec.category().name() == "ErrorCode"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionEx::InvalidArgument);
    }

    SECTION("timeout") {
        std::error_code ec{ErrorCode::Timeout};
        REQUIRE(ec.category().name() == "ErrorCode"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionEx::Timeout);
    }
}

TEST_CASE("custom extended error code", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{ErrorCodeEx::InvalidArgument};
        REQUIRE(ec.category().name() == "ErrorCodeEx"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorCondition::InvalidArgument);
        REQUIRE(ec == ErrorConditionEx::InvalidArgument);
    }

    SECTION("timeout") {
        std::error_code ec{ErrorCodeEx::Timeout};
        REQUIRE(ec.category().name() == "ErrorCodeEx"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorCondition::Timeout);
        REQUIRE(ec == ErrorConditionEx::Timeout);
    }
}

TEST_CASE("custom error transformer", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{static_cast<ErrorTransformer>(EINVAL)};
        REQUIRE(ec.category().name() == "ErrorTransformer"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionEx::InvalidArgument);
    }

    SECTION("timeout") {
        std::error_code ec{static_cast<ErrorTransformer>(ETIMEDOUT)};
        REQUIRE(ec.category().name() == "ErrorTransformer"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionEx::Timeout);
    }
}

TEST_CASE("custom extended error transformer", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{static_cast<ErrorTransformerEx>(EINVAL)};
        REQUIRE(ec.category().name() == "ErrorTransformerEx"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorCondition::InvalidArgument);
        REQUIRE(ec == ErrorConditionEx::InvalidArgument);
    }

    SECTION("timeout") {
        std::error_code ec{static_cast<ErrorTransformerEx>(ETIMEDOUT)};
        REQUIRE(ec.category().name() == "ErrorTransformerEx"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorCondition::Timeout);
        REQUIRE(ec == ErrorConditionEx::Timeout);
    }
}

TEST_CASE("custom error condition", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_condition condition{ErrorCondition::InvalidArgument};
        REQUIRE(condition.category().name() == "ErrorCondition"sv);
        REQUIRE(condition.message() == "invalid argument");
        REQUIRE(condition == ErrorCodeEx::InvalidArgument);
        REQUIRE(condition == static_cast<ErrorTransformerEx>(EINVAL));
    }

    SECTION("timeout") {
        std::error_condition condition{ErrorCondition::Timeout};
        REQUIRE(condition.category().name() == "ErrorCondition"sv);
        REQUIRE(condition.message() == "timeout");
        REQUIRE(condition == ErrorCodeEx::Timeout);
        REQUIRE(condition == static_cast<ErrorTransformerEx>(ETIMEDOUT));
    }
}

TEST_CASE("custom extended error condition", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_condition condition{ErrorConditionEx::InvalidArgument};
        REQUIRE(condition.category().name() == "ErrorConditionEx"sv);
        REQUIRE(condition.message() == "invalid argument");
        REQUIRE(condition == ErrorCode::InvalidArgument);
        REQUIRE(condition == ErrorCodeEx::InvalidArgument);
        REQUIRE(condition == static_cast<ErrorTransformer>(EINVAL));
        REQUIRE(condition == static_cast<ErrorTransformerEx>(EINVAL));
    }

    SECTION("timeout") {
        std::error_condition condition{ErrorConditionEx::Timeout};
        REQUIRE(condition.category().name() == "ErrorConditionEx"sv);
        REQUIRE(condition.message() == "timeout");
        REQUIRE(condition == ErrorCode::Timeout);
        REQUIRE(condition == ErrorCodeEx::Timeout);
        REQUIRE(condition == static_cast<ErrorTransformer>(ETIMEDOUT));
        REQUIRE(condition == static_cast<ErrorTransformerEx>(ETIMEDOUT));
    }
}

TEST_CASE("custom error code defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{ErrorCodeWrapper::ErrorCode::InvalidArgument};
        REQUIRE(ec.category().name() == "ErrorCode"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::InvalidArgument);
    }

    SECTION("timeout") {
        std::error_code ec{ErrorCodeWrapper::ErrorCode::Timeout};
        REQUIRE(ec.category().name() == "ErrorCode"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::Timeout);
    }
}

TEST_CASE("custom extended error code defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{ErrorCodeExWrapper::ErrorCodeEx::InvalidArgument};
        REQUIRE(ec.category().name() == "ErrorCodeEx"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionWrapper::ErrorCondition::InvalidArgument);
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::InvalidArgument);
    }

    SECTION("timeout") {
        std::error_code ec{ErrorCodeExWrapper::ErrorCodeEx::Timeout};
        REQUIRE(ec.category().name() == "ErrorCodeEx"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionWrapper::ErrorCondition::Timeout);
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::Timeout);
    }
}

TEST_CASE("custom error transformer defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{static_cast<ErrorTransformerWrapper::ErrorTransformer>(EINVAL)};
        REQUIRE(ec.category().name() == "ErrorTransformer"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::InvalidArgument);
    }

    SECTION("timeout") {
        std::error_code ec{static_cast<ErrorTransformerWrapper::ErrorTransformer>(ETIMEDOUT)};
        REQUIRE(ec.category().name() == "ErrorTransformer"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::Timeout);
    }
}

TEST_CASE("custom extended error transformer defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_code ec{static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(EINVAL)};
        REQUIRE(ec.category().name() == "ErrorTransformerEx"sv);
        REQUIRE(ec.message() == "invalid argument");
        REQUIRE(ec == ErrorConditionWrapper::ErrorCondition::InvalidArgument);
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::InvalidArgument);
    }

    SECTION("timeout") {
        std::error_code ec{static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(ETIMEDOUT)};
        REQUIRE(ec.category().name() == "ErrorTransformerEx"sv);
        REQUIRE(ec.message() == "timeout");
        REQUIRE(ec == ErrorConditionWrapper::ErrorCondition::Timeout);
        REQUIRE(ec == ErrorConditionExWrapper::ErrorConditionEx::Timeout);
    }
}

TEST_CASE("custom error condition defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_condition condition{ErrorConditionWrapper::ErrorCondition::InvalidArgument};
        REQUIRE(condition.category().name() == "ErrorCondition"sv);
        REQUIRE(condition.message() == "invalid argument");
        REQUIRE(condition == ErrorCodeExWrapper::ErrorCodeEx::InvalidArgument);
        REQUIRE(condition == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(EINVAL));
    }

    SECTION("timeout") {
        std::error_condition condition{ErrorConditionWrapper::ErrorCondition::Timeout};
        REQUIRE(condition.category().name() == "ErrorCondition"sv);
        REQUIRE(condition.message() == "timeout");
        REQUIRE(condition == ErrorCodeExWrapper::ErrorCodeEx::Timeout);
        REQUIRE(condition == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(ETIMEDOUT));
    }
}

TEST_CASE("custom extended error condition defined in class", "[error]") {
    using namespace std::string_view_literals;

    SECTION("invalid argument") {
        std::error_condition condition{ErrorConditionExWrapper::ErrorConditionEx::InvalidArgument};
        REQUIRE(condition.category().name() == "ErrorConditionEx"sv);
        REQUIRE(condition.message() == "invalid argument");
        REQUIRE(condition == ErrorCodeWrapper::ErrorCode::InvalidArgument);
        REQUIRE(condition == ErrorCodeExWrapper::ErrorCodeEx::InvalidArgument);
        REQUIRE(condition == static_cast<ErrorTransformerWrapper::ErrorTransformer>(EINVAL));
        REQUIRE(condition == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(EINVAL));
    }

    SECTION("timeout") {
        std::error_condition condition{ErrorConditionExWrapper::ErrorConditionEx::Timeout};
        REQUIRE(condition.category().name() == "ErrorConditionEx"sv);
        REQUIRE(condition.message() == "timeout");
        REQUIRE(condition == ErrorCodeWrapper::ErrorCode::Timeout);
        REQUIRE(condition == ErrorCodeExWrapper::ErrorCodeEx::Timeout);
        REQUIRE(condition == static_cast<ErrorTransformerWrapper::ErrorTransformer>(ETIMEDOUT));
        REQUIRE(condition == static_cast<ErrorTransformerExWrapper::ErrorTransformerEx>(ETIMEDOUT));
    }
}
