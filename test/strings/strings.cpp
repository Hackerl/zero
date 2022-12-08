#include <zero/strings/strings.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("string contains the specified substring in a case insensitive way", "[strings]") {
    REQUIRE(zero::strings::containsIgnoreCase("", "") == true);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "") == true);
    REQUIRE(zero::strings::containsIgnoreCase("", "AbCd") == false);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "abc") == true);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "abd") == false);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "a") == true);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "aBcD") == true);
    REQUIRE(zero::strings::containsIgnoreCase("AbCd", "aBcDE") == false);
}

TEST_CASE("string starts with the specified value", "[strings]") {
    REQUIRE(zero::strings::startsWith("", "") == true);
    REQUIRE(zero::strings::startsWith("AbCd", "") == true);
    REQUIRE(zero::strings::startsWith("", "AbCd") == false);
    REQUIRE(zero::strings::startsWith("AbCd", "abc") == false);
    REQUIRE(zero::strings::startsWith("AbCd", "Ab") == true);
    REQUIRE(zero::strings::startsWith("AbCd", "AbCd") == true);
}

TEST_CASE("string ends with the specified value", "[strings]") {
    REQUIRE(zero::strings::endsWith("", "") == true);
    REQUIRE(zero::strings::endsWith("AbCd", "") == true);
    REQUIRE(zero::strings::endsWith("", "AbCd") == false);
    REQUIRE(zero::strings::endsWith("AbCd", "BcD") == false);
    REQUIRE(zero::strings::endsWith("AbCd", "bCd") == true);
    REQUIRE(zero::strings::endsWith("AbCd", "Cd") == true);
}

TEST_CASE("remove any leading and trailing spaces of string", "[strings]") {
    REQUIRE(zero::strings::trim("").empty());
    REQUIRE(zero::strings::trim(" \t\n \n\t").empty());
    REQUIRE(zero::strings::trim(" AbCd ") == "AbCd");
    REQUIRE(zero::strings::trim("\n \t AbCd \n\t") == "AbCd");
    REQUIRE(zero::strings::trim(" \n \t Ab Cd \n\t ") == "Ab Cd");
    REQUIRE(zero::strings::trim(" A b C d ") == "A b C d");
}

TEST_CASE("remove any leading spaces of string", "[strings]") {
    REQUIRE(zero::strings::ltrim("").empty());
    REQUIRE(zero::strings::ltrim(" \t\n \n\t").empty());
    REQUIRE(zero::strings::ltrim(" AbCd ") == "AbCd ");
    REQUIRE(zero::strings::ltrim("\n \t AbCd \n\t") == "AbCd \n\t");
    REQUIRE(zero::strings::ltrim(" \n \t Ab Cd \n\t ") == "Ab Cd \n\t ");
    REQUIRE(zero::strings::ltrim(" A b C d ") == "A b C d ");
}

TEST_CASE("remove any trailing spaces of string", "[strings]") {
    REQUIRE(zero::strings::rtrim("").empty());
    REQUIRE(zero::strings::rtrim(" \t\n \n\t").empty());
    REQUIRE(zero::strings::rtrim(" AbCd ") == " AbCd");
    REQUIRE(zero::strings::rtrim("\n \t AbCd \n\t") == "\n \t AbCd");
    REQUIRE(zero::strings::rtrim(" \n \t Ab Cd \n\t ") == " \n \t Ab Cd");
    REQUIRE(zero::strings::rtrim(" A b C d ") == " A b C d");
}

TEST_CASE("trim extra spaces between words", "[strings]") {
    REQUIRE(zero::strings::trimExtraSpace("").empty());
    REQUIRE(zero::strings::trimExtraSpace(" \t\n \n\t") == " ");
    REQUIRE(zero::strings::trimExtraSpace(" Ab \t\nCd ") == " Ab Cd ");
    REQUIRE(zero::strings::trimExtraSpace("\n \t A\n bCd \n\t") == "\nA\nbCd ");
    REQUIRE(zero::strings::trimExtraSpace(" \n \t Ab Cd \n\t ") == " Ab Cd ");
    REQUIRE(zero::strings::trimExtraSpace(" A b C d ") == " A b C d ");
}

TEST_CASE("convert string to lower case letters", "[strings]") {
    REQUIRE(zero::strings::tolower("").empty());
    REQUIRE(zero::strings::tolower("AbcD") == "abcd");
    REQUIRE(zero::strings::tolower("A b c D") == "a b c d");
}

TEST_CASE("convert string to upper case letters", "[strings]") {
    REQUIRE(zero::strings::toupper("").empty());
    REQUIRE(zero::strings::toupper("AbcD") == "ABCD");
    REQUIRE(zero::strings::toupper("A b c D") == "A B C D");
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
}