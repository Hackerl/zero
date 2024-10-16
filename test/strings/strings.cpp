#include <zero/strings/strings.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("remove any leading and trailing spaces of string", "[strings]") {
    REQUIRE_THAT(zero::strings::trim(""), Catch::Matchers::IsEmpty());
    REQUIRE_THAT(zero::strings::trim("\t\n \n\t"), Catch::Matchers::IsEmpty());
    REQUIRE(zero::strings::trim(" AbCd ") == "AbCd");
    REQUIRE(zero::strings::trim("\n \t AbCd \n\t") == "AbCd");
    REQUIRE(zero::strings::trim(" \n \t Ab Cd \n\t ") == "Ab Cd");
    REQUIRE(zero::strings::trim(" A b C d ") == "A b C d");

    BENCHMARK("zero::strings::trim") {
        return zero::strings::trim(" \n \t Ab Cd \n\t ");
    };
}

TEST_CASE("remove any leading spaces of string", "[strings]") {
    REQUIRE_THAT(zero::strings::ltrim(""), Catch::Matchers::IsEmpty());
    REQUIRE_THAT(zero::strings::ltrim(" \t\n \n\t"), Catch::Matchers::IsEmpty());
    REQUIRE(zero::strings::ltrim(" AbCd ") == "AbCd ");
    REQUIRE(zero::strings::ltrim("\n \t AbCd \n\t") == "AbCd \n\t");
    REQUIRE(zero::strings::ltrim(" \n \t Ab Cd \n\t ") == "Ab Cd \n\t ");
    REQUIRE(zero::strings::ltrim(" A b C d ") == "A b C d ");

    BENCHMARK("zero::strings::ltrim") {
        return zero::strings::ltrim(" \n \t Ab Cd \n\t ");
    };
}

TEST_CASE("remove any trailing spaces of string", "[strings]") {
    REQUIRE_THAT(zero::strings::rtrim(""), Catch::Matchers::IsEmpty());
    REQUIRE_THAT(zero::strings::rtrim(" \t\n \n\t"), Catch::Matchers::IsEmpty());
    REQUIRE(zero::strings::rtrim(" AbCd ") == " AbCd");
    REQUIRE(zero::strings::rtrim("\n \t AbCd \n\t") == "\n \t AbCd");
    REQUIRE(zero::strings::rtrim(" \n \t Ab Cd \n\t ") == " \n \t Ab Cd");
    REQUIRE(zero::strings::rtrim(" A b C d ") == " A b C d");

    BENCHMARK("zero::strings::rtrim") {
        return zero::strings::rtrim(" \n \t Ab Cd \n\t ");
    };
}

TEST_CASE("convert string to lower case letters", "[strings]") {
    REQUIRE_THAT(zero::strings::tolower(""), Catch::Matchers::IsEmpty());
    REQUIRE(zero::strings::tolower("AbcD") == "abcd");
    REQUIRE(zero::strings::tolower("A b c D") == "a b c d");

    BENCHMARK("zero::strings::tolower") {
        return zero::strings::tolower("A b c D");
    };
}

TEST_CASE("convert string to upper case letters", "[strings]") {
    REQUIRE_THAT(zero::strings::toupper(""), Catch::Matchers::IsEmpty());
    REQUIRE(zero::strings::toupper("AbcD") == "ABCD");
    REQUIRE(zero::strings::toupper("A b c D") == "A B C D");

    BENCHMARK("zero::strings::toupper") {
        return zero::strings::toupper("A b c D");
    };
}

TEST_CASE("split string to vector", "[strings]") {
    REQUIRE_THAT(zero::strings::split("", ""), Catch::Matchers::RangeEquals(std::vector{""}));
    REQUIRE_THAT(zero::strings::split("", " "), Catch::Matchers::RangeEquals(std::vector{""}));
    REQUIRE_THAT(zero::strings::split("aBcd", ""), Catch::Matchers::RangeEquals(std::vector{"aBcd"}));
    REQUIRE_THAT(zero::strings::split("aBc d", " "), Catch::Matchers::RangeEquals(std::vector{"aBc", "d"}));
    REQUIRE_THAT(zero::strings::split("aBc  d", " "), Catch::Matchers::RangeEquals(std::vector{"aBc", "", "d"}));
    REQUIRE_THAT(
        zero::strings::split("a  B c  d", " "),
        Catch::Matchers::RangeEquals(std::vector{"a", "", "B", "c", "", "d"})
    );

    REQUIRE_THAT(
        zero::strings::split("a  B c  d", " ", 0),
        Catch::Matchers::RangeEquals(std::vector{"a", "", "B", "c", "", "d"})
    );

    REQUIRE_THAT(
        zero::strings::split("a  B c  d", " ", -1),
        Catch::Matchers::RangeEquals(std::vector{"a", "", "B", "c", "", "d"})
    );
    REQUIRE_THAT(
        zero::strings::split("a  B c  d", " ", 2),
        Catch::Matchers::RangeEquals(std::vector{"a", "", "B c  d"})
    );

    BENCHMARK("zero::strings::split") {
        return zero::strings::split("a  B c  d", " ");
    };
}

TEST_CASE("split string to vector by whitespace", "[strings]") {
    REQUIRE_THAT(zero::strings::split(""), Catch::Matchers::IsEmpty());
    REQUIRE_THAT(zero::strings::split(" "), Catch::Matchers::IsEmpty());
    REQUIRE_THAT(zero::strings::split(" \n\t "), Catch::Matchers::IsEmpty());
    REQUIRE_THAT(zero::strings::split(" A \n B    C "), Catch::Matchers::RangeEquals(std::vector{"A", "B", "C"}));
    REQUIRE_THAT(zero::strings::split("aBcd"), Catch::Matchers::RangeEquals(std::vector{"aBcd"}));
    REQUIRE_THAT(zero::strings::split("aBc d"), Catch::Matchers::RangeEquals(std::vector{"aBc", "d"}));
    REQUIRE_THAT(zero::strings::split("aBc  d"), Catch::Matchers::RangeEquals(std::vector{"aBc", "d"}));
    REQUIRE_THAT(zero::strings::split("a  B c  d"), Catch::Matchers::RangeEquals(std::vector{"a", "B", "c", "d"}));
    REQUIRE_THAT(zero::strings::split("a  B c  d", 0), Catch::Matchers::RangeEquals(std::vector{"a", "B", "c", "d"}));
    REQUIRE_THAT(zero::strings::split("a  B c  d", -1), Catch::Matchers::RangeEquals(std::vector{"a", "B", "c", "d"}));
    REQUIRE_THAT(zero::strings::split("a  B c  d", 2), Catch::Matchers::RangeEquals(std::vector{"a", "B", "c  d"}));
    REQUIRE_THAT(zero::strings::split(" a  B c  d ", 2), Catch::Matchers::RangeEquals(std::vector{"a", "B", "c  d "}));
    REQUIRE_THAT(
        zero::strings::split("\na \t\n B    c  d ", 2),
        Catch::Matchers::RangeEquals(std::vector{"a", "B", "c  d "})
    );

    BENCHMARK("zero::strings::split") {
        return zero::strings::split("a  B c  d");
    };
}

TEST_CASE("convert string to integer", "[strings]") {
    REQUIRE_FALSE(zero::strings::toNumber<int>(""));
    REQUIRE_FALSE(zero::strings::toNumber<int>("", 16));
    REQUIRE_FALSE(zero::strings::toNumber<int>("ABC"));

    auto result = zero::strings::toNumber<int>("3");
    REQUIRE(result);
    REQUIRE(*result == 3);

    REQUIRE_FALSE(zero::strings::toNumber<int>("QQQ3"));
    REQUIRE_FALSE(zero::strings::toNumber<int>("ABC3", 2));

    result = zero::strings::toNumber<int>("ABC", 16);
    REQUIRE(result);
    REQUIRE(*result == 0xabc);

    result = zero::strings::toNumber<int>("ABC3QQQ", 16);
    REQUIRE(result);
    REQUIRE(*result == 0xabc3);

    REQUIRE_FALSE(zero::strings::toNumber<int>("QQQ0ABC", 16));

    BENCHMARK("zero::strings::toNumber") {
        return zero::strings::toNumber<int>("aBcDeF19", 16);
    };
}

TEST_CASE("convert a wide-character string to a multibyte string", "[strings]") {
    std::setlocale(LC_ALL, "en_US.UTF-8");

    auto result = zero::strings::encode(L"");
    REQUIRE(result);
    REQUIRE(result->empty());

    result = zero::strings::encode(L"1234567890");
    REQUIRE(result);
    REQUIRE(*result == "1234567890");

    result = zero::strings::encode(L"你好");
    REQUIRE(result);
    REQUIRE(*result == "你好");

    BENCHMARK("zero::strings::encode") {
        return zero::strings::encode(L"1234567890");
    };
}

TEST_CASE("convert a multibyte string to a wide-character string", "[strings]") {
    std::setlocale(LC_ALL, "en_US.UTF-8");

    auto result = zero::strings::decode("");
    REQUIRE(result);
    REQUIRE(result->empty());

    result = zero::strings::decode("1234567890");
    REQUIRE(result);
    REQUIRE(*result == L"1234567890");

    result = zero::strings::decode("你好");
    REQUIRE(result);
    REQUIRE(*result == L"你好");

    BENCHMARK("zero::strings::decode") {
        return zero::strings::decode("1234567890");
    };
}
