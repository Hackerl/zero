#ifndef ZERO_STRINGS_H
#define ZERO_STRINGS_H

#include <string>
#include <vector>
#include <charconv>
#include <system_error>
#include <tl/expected.hpp>

namespace zero::strings {
    std::string trim(std::string_view str);
    std::string ltrim(std::string_view str);
    std::string rtrim(std::string_view str);

    std::string tolower(std::string_view str);
    std::string toupper(std::string_view str);

    std::vector<std::string> split(std::string_view str, int limit = 0);
    std::vector<std::string> split(std::string_view str, std::string_view delimiter, int limit = 0);

    tl::expected<std::string, std::error_code> encode(std::wstring_view str, const char *encoding = "UTF-8");
    tl::expected<std::wstring, std::error_code> decode(std::string_view str, const char *encoding = "UTF-8");

    template<typename T>
    tl::expected<T, std::error_code> toNumber(std::string_view str, int base = 10) {
        T value;

        if (const auto result = std::from_chars(str.data(), str.data() + str.length(), value, base);
            result.ec != std::errc())
            return tl::unexpected(make_error_code(result.ec));

        return value;
    }
}

#endif //ZERO_STRINGS_H
