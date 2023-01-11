#include <zero/strings/strings.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("string contains the specified substring in a case insensitive way", "[strings]") {
    REQUIRE(zero::strings::containsIgnoreCase("", "") == true);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "") == true);
    REQUIRE(zero::strings::containsIgnoreCase("", "AbCd") == false);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "abc") == true);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "abd") == false);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "a") == true);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "aBcD") == true);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "aBcDE") == false);

    BENCHMARK("zero::strings::containsIgnoreCase") {
        return zero::strings::containsIgnoreCase("AbCd", "aBcD");
    };
}

TEST_CASE("string starts with the specified value", "[strings]") {
    REQUIRE(zero::strings::startsWith("", "") == true);
    REQUIRE(zero::strings::startsWith("AbCd", "") == true);
    REQUIRE(zero::strings::startsWith("", "AbCd") == false);
    REQUIRE(zero::strings::startsWith("AbCd", "abc") == false);
    REQUIRE(zero::strings::startsWith("AbCd", "Ab") == true);
    REQUIRE(zero::strings::startsWith("AbCd", "AbCd") == true);

    BENCHMARK("zero::strings::startsWith") {
        return zero::strings::startsWith("AbCd", "AbCd");
    };
}

TEST_CASE("string ends with the specified value", "[strings]") {
    REQUIRE(zero::strings::endsWith("", "") == true);
    REQUIRE(zero::strings::endsWith("AbCd", "") == true);
    REQUIRE(zero::strings::endsWith("", "AbCd") == false);
    REQUIRE(zero::strings::endsWith("AbCd", "BcD") == false);
    REQUIRE(zero::strings::endsWith("AbCd", "bCd") == true);
    REQUIRE(zero::strings::endsWith("AbCd", "Cd") == true);

    BENCHMARK("zero::strings::endsWith") {
        return zero::strings::endsWith("AbCd", "Cd");
    };
}

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
        return zero::strings::rtrim(" \n \t Ab Cd \n\t ") ;
    };
}

TEST_CASE("trim extra spaces between words", "[strings]") {
    REQUIRE(zero::strings::trimExtraSpace("").empty());
    REQUIRE(zero::strings::trimExtraSpace(" \t\n \n\t") == " ");
    REQUIRE(zero::strings::trimExtraSpace(" Ab \t\nCd ") == " Ab Cd ");
    REQUIRE(zero::strings::trimExtraSpace("\n \t A\n bCd \n\t") == "\nA\nbCd ");
    REQUIRE(zero::strings::trimExtraSpace(" \n \t Ab Cd \n\t ") == " Ab Cd ");
    REQUIRE(zero::strings::trimExtraSpace(" A b C d ") == " A b C d ");

    BENCHMARK("zero::strings::trimExtraSpace") {
        return zero::strings::trimExtraSpace(" \n \t Ab Cd \n\t ");
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

TEST_CASE("concatenates the given elements with the delimiter", "[strings]") {
    REQUIRE(zero::strings::join(std::vector<std::string>{}, "").empty());
    REQUIRE(zero::strings::join(std::vector<std::string>{}, " ").empty());
    REQUIRE(zero::strings::join(std::vector<std::string>{"a", "b", "c", "d", "e", "f", "g"}, "") == "abcdefg");
    REQUIRE(zero::strings::join(std::vector<std::string>{"a", "b", "c", "d", "e", "f", "g"}, " ") == "a b c d e f g");

    BENCHMARK("zero::strings::join") {
        return zero::strings::join(std::vector<std::string>{"a", "b", "c", "d", "e", "f", "g"}, " ");
    };
}

TEST_CASE("convert string to integer", "[strings]") {
    REQUIRE(!zero::strings::toNumber<int>(""));
    REQUIRE(!zero::strings::toNumber<int>("", 16));
    REQUIRE(!zero::strings::toNumber<int>("ABC"));
    REQUIRE(*zero::strings::toNumber<int>("3") == 3);
    REQUIRE(!zero::strings::toNumber<int>("QQQ3"));
    REQUIRE(!zero::strings::toNumber<int>("ABC3", 2));
    REQUIRE(*zero::strings::toNumber<int>("ABC3QQQ", 16) == 0xabc3);
    REQUIRE(*zero::strings::toNumber<int>("ABC", 16) == 0xabc);
    REQUIRE(!zero::strings::toNumber<int>("QQQ0ABC", 16));

    BENCHMARK("zero::strings::toNumber") {
        return zero::strings::toNumber<int>("aBcDeF19", 16);
    };
}

TEST_CASE("format args according to the format string", "[strings]") {
    REQUIRE(zero::strings::format("%s", "").empty());
    REQUIRE(zero::strings::format("%d", 1) == "1");
    REQUIRE(zero::strings::format("%d %s", 1, "abc") == "1 abc");

    BENCHMARK("zero::strings::format") {
        return zero::strings::format("%d %s", 1, "abc");
    };
}