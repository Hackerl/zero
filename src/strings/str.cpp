#include <strings/str.h>
#include <algorithm>

bool zero::strings::containsIC(const std::string &str, const std::string &substr) {
    return std::search(
            str.begin(), str.end(),
            substr.begin(), substr.end(),
            [](char ch1, char ch2) {
                return ::toupper(ch1) == ::toupper(ch2);
            }
    ) != str.end();
}

bool zero::strings::startsWith(const std::string &str, const std::string &prefix) {
    if (str.length() < prefix.length())
        return false;

    return str.compare(0, prefix.length(), prefix) == 0;
}

bool zero::strings::endsWith(const std::string &str, const std::string &suffix) {
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
                    [](int ch) {
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
                    [](int ch) {
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
                    std::begin(str),
                    std::end(str),
                    [](unsigned char a, unsigned char b) {
                        return std::isspace(a) && std::isspace(b);
                    }),
            std::end(str)
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

std::vector<std::string> zero::strings::split(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;

    std::stringstream ss(str);
    std::string s;

    while (getline(ss, s, delimiter)) {
        tokens.push_back(s);
    }

    return tokens;
}
