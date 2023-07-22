#include <zero/strings/strings.h>
#include <ranges>
#include <algorithm>
#include <iconv.h>

std::string zero::strings::trim(std::string_view str) {
    auto v = str
             | std::views::drop_while(isspace)
             | std::views::reverse
             | std::views::drop_while(isspace)
             | std::views::reverse;

    return {v.begin(), v.end()};
}

std::string zero::strings::ltrim(std::string_view str) {
    auto v = str
             | std::views::drop_while(isspace);

    return {v.begin(), v.end()};
}

std::string zero::strings::rtrim(std::string_view str) {
    auto v = str
             | std::views::reverse
             | std::views::drop_while(isspace)
             | std::views::reverse;

    return {v.begin(), v.end()};
}

std::string zero::strings::tolower(std::string_view str) {
    auto v = str
             | std::views::transform(::tolower);

    return {v.begin(), v.end()};
}

std::string zero::strings::toupper(std::string_view str) {
    auto v = str
             | std::views::transform(::toupper);

    return {v.begin(), v.end()};
}

std::vector<std::string> zero::strings::split(std::string_view str, int limit) {
    std::vector<std::string> tokens;

    auto prev = std::find_if(str.begin(), str.end(), isspace);

    while (true) {
        if (prev == str.end())
            break;

        auto it = std::find_if(prev, str.end(), isspace);

        if (it == str.end()) {
            tokens.emplace_back(prev, str.end());
            break;
        }

        tokens.emplace_back(prev, it);
        prev = std::find_if(it, str.end(), isspace);

        if (!--limit) {
            if (prev == str.end())
                break;

            tokens.emplace_back(prev, str.end());
            break;
        }
    }

    return tokens;
}

std::vector<std::string> zero::strings::split(std::string_view str, std::string_view delimiter, int limit) {
    std::vector<std::string> tokens;

    if (delimiter.empty()) {
        tokens.emplace_back(str);
        return tokens;
    }

    size_t prev = 0;

    while (true) {
        size_t pos = str.find(delimiter, prev);

        if (pos == std::string::npos) {
            tokens.emplace_back(str.substr(prev));
            break;
        }

        tokens.emplace_back(str.substr(prev, pos - prev));
        prev = pos + delimiter.length();

        if (!--limit) {
            tokens.emplace_back(str.substr(prev));
            break;
        }
    }

    return tokens;
}

std::optional<std::string> zero::strings::encode(const std::wstring &str, const char *encoding) {
    iconv_t cd = iconv_open(encoding, "WCHAR_T");

    if (cd == (iconv_t) -1)
        return std::nullopt;

    std::optional<std::string> output = std::make_optional<std::string>();

    char *input = (char *) str.c_str();
    size_t inBytesLeft = str.length() * sizeof(wchar_t);

    while (inBytesLeft > 0) {
        char buffer[1024] = {};

        char *ptr = buffer;
        size_t outBytesLeft = sizeof(buffer);

        if (iconv(cd, &input, &inBytesLeft, &ptr, &outBytesLeft) == -1 && errno != E2BIG) {
            output.reset();
            break;
        }

        output->append(buffer, sizeof(buffer) - outBytesLeft);
    }

    iconv_close(cd);

    return output;
}

std::optional<std::wstring> zero::strings::decode(const std::string &str, const char *encoding) {
    iconv_t cd = iconv_open("WCHAR_T", encoding);

    if (cd == (iconv_t) -1)
        return std::nullopt;

    std::optional<std::wstring> output = std::make_optional<std::wstring>();

    char *input = (char *) str.c_str();
    size_t inBytesLeft = str.length();

    while (inBytesLeft > 0) {
        char buffer[1024] = {};

        char *ptr = buffer;
        size_t outBytesLeft = sizeof(buffer);

        if (iconv(cd, &input, &inBytesLeft, &ptr, &outBytesLeft) == -1 && errno != E2BIG) {
            output.reset();
            break;
        }

        output->append((wchar_t *) buffer, (sizeof(buffer) - outBytesLeft) / sizeof(wchar_t));
    }

    iconv_close(cd);

    return output;
}
