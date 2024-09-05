#include <zero/strings/strings.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("remove any leading and trailing spaces of string", "[strings]") {
    REQUIRE(zero::strings::trim("").empty());
    REQUIRE(zero::strings::trim(" \t\n \n\t").empty());
    REQUIRE(zero::strings::trim(" AbCd ") == "AbCd");
    REQUIRE(zero::strings::trim("\n \t AbCd \n\t") == "AbCd");
    REQUIRE(zero::strings::trim(" \n \t Ab Cd \n\t ") == "Ab Cd");
    REQUIRE(zero::strings::trim(" A b C d ") == "A b C d");

    BENCHMARK("zero::strings::trim") {
        return zero::strings::trim(" \n \t Ab Cd \n\t ");
    };
}

TEST_CASE("remove any leading spaces of string", "[strings]") {
    REQUIRE(zero::strings::ltrim("").empty());
    REQUIRE(zero::strings::ltrim(" \t\n \n\t").empty());
    REQUIRE(zero::strings::ltrim(" AbCd ") == "AbCd ");
    REQUIRE(zero::strings::ltrim("\n \t AbCd \n\t") == "AbCd \n\t");
    REQUIRE(zero::strings::ltrim(" \n \t Ab Cd \n\t ") == "Ab Cd \n\t ");
    REQUIRE(zero::strings::ltrim(" A b C d ") == "A b C d ");

    BENCHMARK("zero::strings::ltrim") {
        return zero::strings::ltrim(" \n \t Ab Cd \n\t ");
    };
}

TEST_CASE("remove any trailing spaces of string", "[strings]") {
    REQUIRE(zero::strings::rtrim("").empty());
    REQUIRE(zero::strings::rtrim(" \t\n \n\t").empty());
    REQUIRE(zero::strings::rtrim(" AbCd ") == " AbCd");
    REQUIRE(zero::strings::rtrim("\n \t AbCd \n\t") == "\n \t AbCd");
    REQUIRE(zero::strings::rtrim(" \n \t Ab Cd \n\t ") == " \n \t Ab Cd");
    REQUIRE(zero::strings::rtrim(" A b C d ") == " A b C d");

    BENCHMARK("zero::strings::rtrim") {
        return zero::strings::rtrim(" \n \t Ab Cd \n\t ");
    };
}

TEST_CASE("convert string to lower case letters", "[strings]") {
    REQUIRE(zero::strings::tolower("").empty());
    REQUIRE(zero::strings::tolower("AbcD") == "abcd");
    REQUIRE(zero::strings::tolower("A b c D") == "a b c d");

    BENCHMARK("zero::strings::tolower") {
        return zero::strings::tolower("A b c D");
    };
}

TEST_CASE("convert string to upper case letters", "[strings]") {
    REQUIRE(zero::strings::toupper("").empty());
    REQUIRE(zero::strings::toupper("AbcD") == "ABCD");
    REQUIRE(zero::strings::toupper("A b c D") == "A B C D");

    BENCHMARK("zero::strings::toupper") {
        return zero::strings::toupper("A b c D");
    };
}

TEST_CASE("split string to vector", "[strings]") {
    REQUIRE(zero::strings::split("", "") == std::vector<std::string>{""});
    REQUIRE(zero::strings::split("", " ") == std::vector<std::string>{""});
    REQUIRE(zero::strings::split("aBcd", "") == std::vector<std::string>{"aBcd"});
    REQUIRE(zero::strings::split("aBc d", " ") == std::vector<std::string>{"aBc", "d"});
    REQUIRE(zero::strings::split("aBc  d", " ") == std::vector<std::string>{"aBc", "", "d"});
    REQUIRE(zero::strings::split("a  B c  d", " ") == std::vector<std::string>{"a", "", "B", "c", "", "d"});
    REQUIRE(zero::strings::split("a  B c  d", " ", 0) == std::vector<std::string>{"a", "", "B", "c", "", "d"});
    REQUIRE(zero::strings::split("a  B c  d", " ", -1) == std::vector<std::string>{"a", "", "B", "c", "", "d"});
    REQUIRE(zero::strings::split("a  B c  d", " ", 2) == std::vector<std::string>{"a", "", "B c  d"});

    BENCHMARK("zero::strings::split") {
        return zero::strings::split("a  B c  d", " ");
    };
}

TEST_CASE("split string to vector by whitespace", "[strings]") {
    REQUIRE(zero::strings::split("").empty());
    REQUIRE(zero::strings::split(" ").empty());
    REQUIRE(zero::strings::split(" \n\t ").empty());
    REQUIRE(zero::strings::split(" A \n B    C ") == std::vector<std::string>{"A", "B", "C"});
    REQUIRE(zero::strings::split("aBcd") == std::vector<std::string>{"aBcd"});
    REQUIRE(zero::strings::split("aBc d") == std::vector<std::string>{"aBc", "d"});
    REQUIRE(zero::strings::split("aBc  d") == std::vector<std::string>{"aBc", "d"});
    REQUIRE(zero::strings::split("a  B c  d") == std::vector<std::string>{"a", "B", "c", "d"});
    REQUIRE(zero::strings::split("a  B c  d", 0) == std::vector<std::string>{"a", "B", "c", "d"});
    REQUIRE(zero::strings::split("a  B c  d", -1) == std::vector<std::string>{"a", "B", "c", "d"});
    REQUIRE(zero::strings::split("a  B c  d", 2) == std::vector<std::string>{"a", "B", "c  d"});
    REQUIRE(zero::strings::split(" a  B c  d ", 2) == std::vector<std::string>{"a", "B", "c  d "});
    REQUIRE(zero::strings::split("\na \t\n B    c  d ", 2) == std::vector<std::string>{"a", "B", "c  d "});

    BENCHMARK("zero::strings::split") {
        return zero::strings::split("a  B c  d");
    };
}

TEST_CASE("convert string to integer", "[strings]") {
    REQUIRE_FALSE(zero::strings::toNumber<int>(""));
    REQUIRE_FALSE(zero::strings::toNumber<int>("", 16));
    REQUIRE_FALSE(zero::strings::toNumber<int>("ABC"));
    REQUIRE(*zero::strings::toNumber<int>("3") == 3);
    REQUIRE_FALSE(zero::strings::toNumber<int>("QQQ3"));
    REQUIRE_FALSE(zero::strings::toNumber<int>("ABC3", 2));
    REQUIRE(*zero::strings::toNumber<int>("ABC3QQQ", 16) == 0xabc3);
    REQUIRE(*zero::strings::toNumber<int>("ABC", 16) == 0xabc);
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
