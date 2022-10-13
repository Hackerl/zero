#include <zero/strings/strings.h>
#include <algorithm>

bool zero::strings::containsIC(std::string_view str, std::string_view substr) {
    return std::search(
            str.begin(), str.end(),
            substr.begin(), substr.end(),
            [](char ch1, char ch2) {
                return ::toupper(ch1) == ::toupper(ch2);
            }
    ) != str.end();
}

bool zero::strings::startsWith(std::string_view str, std::string_view prefix) {
    if (str.length() < prefix.length())
        return false;

    return str.compare(0, prefix.length(), prefix) == 0;
}

bool zero::strings::endsWith(std::string_view str, std::string_view suffix) {
    if (str.length() < suffix.length())
        return false;

    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string zero::strings::trim(const std::string &str) {
    return rtrim(ltrim(str));
}

std::string zero::strings::ltrim(std::string str) {
    str.erase(
            str.begin(),
            std::find_if(
                    str.begin(),
                    str.end(),
                    [](char ch) {
                        return !std::isspace(ch);
                    }
            )
    );

    return str;
}

std::string zero::strings::rtrim(std::string str) {
    str.erase(
            std::find_if(
                    str.rbegin(),
                    str.rend(),
                    [](char ch) {
                        return !std::isspace(ch);
                    }
            ).base(),
            str.end()
    );

    return str;
}

std::string zero::strings::trimExtraSpace(std::string str) {
    str.erase(
            std::unique(
                    str.begin(),
                    str.end(),
                    [](char ch1, char ch2) {
                        return std::isspace(ch1) && std::isspace(ch2);
                    }),
            str.end()
    );

    return str;
}

std::string zero::strings::tolower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::string zero::strings::toupper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

std::vector<std::string> zero::strings::split(std::string_view str, std::string_view delimiter, int limit) {
    std::vector<std::string> tokens;

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
