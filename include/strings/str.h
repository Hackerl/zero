#ifndef ZERO_STR_H
#define ZERO_STR_H

#include <vector>
#include <sstream>
#include <numeric>
#include <memory>

namespace zero {
    namespace strings {
        bool containsIC(const std::string &str, const std::string &substr);
        bool startsWith(const std::string &str, const std::string &prefix);
        bool endsWith(const std::string &str, const std::string &suffix);

        std::string trim(const std::string &str);
        std::string ltrim(std::string str);
        std::string rtrim(std::string str);
        std::string trimExtraSpace(std::string str);

        std::string tolower(std::string str);
        std::string toupper(std::string str);

        std::vector<std::string> split(const std::string &str, char delimiter);

        template<typename T>
        std::string join(const T &containers, const char *delimiter) {
            if (containers.empty())
                return "";

            return std::accumulate(
                    std::next(containers.begin()),
                    containers.end(),
                    containers.front(),
                    [=](auto result, const auto &value) {
                        return result + delimiter + value;
                    }
            );
        }

        template<typename T>
        bool toNumber(const std::string &str, T &number, int base = 10) {
            std::istringstream ss(str);

            switch (base) {
                case 8:
                    ss >> std::oct >> number;
                    break;

                case 10:
                    ss >> std::dec >> number;
                    break;

                case 16:
                    ss >> std::hex >> number;
                    break;

                default:
                    ss >> number;
            }

            return !ss.fail() && !ss.bad();
        }

        template<typename... Args>
        std::string format(const char *fmt, Args... args) {
            int length = snprintf(nullptr, 0, fmt, args...);

            if (length <= 0)
                return "";

            std::unique_ptr<char> buffer(new char[length + 1]);
            snprintf(buffer.get(), length + 1, fmt, args...);

            return {buffer.get(), (std::size_t)length};
        }
    }
}

#endif //ZERO_STR_H
