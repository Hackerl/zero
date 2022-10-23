#ifndef ZERO_CMDLINE_H
#define ZERO_CMDLINE_H

#include "strings/strings.h"
#include "filesystem/path.h"
#include <list>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <functional>
#include <any>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace zero {
    template<typename T>
    std::any parseValue(std::string_view str) {
        if constexpr (std::is_arithmetic_v<T>) {
            std::optional<T> value = strings::toNumber<T>(str);

            if (!value)
                return {};

            return *value;
        } else if (std::is_same_v<T, std::string>) {
            return std::string{str};
        } else if (std::is_same_v<T, std::vector<typename T::value_type, typename T::allocator_type>>) {
            T value;

            for (const auto &token : strings::split(str, ",")) {
                std::any v = parseValue<typename T::value_type>(strings::trim(token));

                if (!v.has_value())
                    return {};

                value.push_back(std::any_cast<typename T::value_type>(v));
            }

            return value;
        }

        return {};
    }

    template<typename T>
    std::string demangle() {
        if constexpr (std::is_arithmetic_v<T>) {
#ifdef _MSC_VER
            return typeid(T).name();
#elif __GNUC__
            int status = 0;
            std::unique_ptr<char, decltype(free) *> buffer(abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status), free);

            if (status != 0 || !buffer)
                return "unknown";

            return buffer.get();
#endif
        } else if (std::is_same_v<T, std::string>) {
            return "string";
        } else if (std::is_same_v<T, std::vector<typename T::value_type, typename T::allocator_type>>) {
            return strings::format("%s[]", demangle<typename T::value_type>().c_str());
        }

        return "unknown";
    }

    struct Optional {
        std::string name;
        char shortName;
        std::string desc;
        std::any value;
        bool flag;
        std::string type;
        std::function<std::any(std::string_view)> parse;
    };

    struct Positional {
        std::string name;
        std::string desc;
        std::any value;
        std::string type;
        std::function<std::any(std::string_view)> parse;
    };

    class Cmdline {
    public:
        Cmdline() {
            addOptional("help", '?', "print help message");
        }

    public:
        template<typename T>
        void add(const char *name, const char *desc) {
            mPositionals.push_back({name, desc, T{}, demangle<T>(), parseValue<T>});
        }

        void addOptional(const char *name, char shortName, const char *desc) {
            mOptionals.push_back({name, shortName, desc, false, true});
        }

        template<typename T>
        void addOptional(const char *name, char shortName, const char *desc, T def = {}) {
            mOptionals.push_back({name, shortName, desc, def, false, demangle<T>(), parseValue<T>});
        }

    public:
        template<typename T>
        T get(const char *name) {
            auto it = std::find_if(
                    mPositionals.begin(),
                    mPositionals.end(),
                    [=](const auto &positional) {
                        return positional.name == name;
                    }
            );

            if (it == mPositionals.end())
                error("positional argument not found: %s", name);

            return std::any_cast<T>(it->value);
        }

        template<typename T>
        T getOptional(const char *name) {
            return std::any_cast<T>(findByName(name).value);
        }

    public:
        void footer(const char *message) {
            mFooter = message;
        }

        std::vector<std::string> rest() {
            return mRest;
        }

    public:
        void parse(int argc, char **argv) {
            auto it = mPositionals.begin();

            for (int i = 1; i < argc; i++) {
                if (!strings::startsWith(argv[i], "-")) {
                    if (it == mPositionals.end()) {
                        mRest.emplace_back(argv[i]);
                        continue;
                    }

                    it->value = it->parse(argv[i]);

                    if (!it++->value.has_value())
                        error("invalid positional argument: %s", argv[i]);

                    continue;
                }

                if (argv[i][1] != '-') {
                    Optional &optional = findByShortName(argv[i][1]);

                    if (optional.flag) {
                        optional.value = true;
                        continue;
                    }

                    if (!argv[i + 1])
                        error("invalid optional argument: %s", argv[i]);

                    optional.value = optional.parse(argv[++i]);

                    if (!optional.value.has_value())
                        error("invalid optional argument: %s", argv[i]);

                    continue;
                }

                char *p = strchr(argv[i], '=');
                unsigned long n = p ? p - argv[i] : strlen(argv[i]);

                if (n == 2) {
                    mRest.insert(mRest.end(), argv + i + 1, argv + argc);
                    break;
                }

                Optional &optional = findByName({argv[i] + 2, n - 2});

                if (optional.flag) {
                    optional.value = true;
                    continue;
                }

                if (!p)
                    error("optional argument requires value: %s", argv[i]);

                optional.value = optional.parse(p + 1);

                if (!optional.value.has_value())
                    error("invalid optional argument: %s", argv[i]);
            }

            if (getOptional<bool>("help")) {
                help();
                exit(0);
            }

            if (it != mPositionals.end())
                error("positional arguments not enough");
        }

    private:
        Optional &findByName(const std::string &name) {
            if (name.empty())
                error("empty option name");

            auto it = std::find_if(
                    mOptionals.begin(),
                    mOptionals.end(),
                    [=](const auto &optional) {
                        return optional.name == name;
                    }
            );

            if (it == mOptionals.end())
                error("optional argument not found: --%s", name.c_str());

            return *it;
        }

        Optional &findByShortName(char shortName) {
            if (!shortName)
                error("empty option name");

            auto it = std::find_if(
                    mOptionals.begin(),
                    mOptionals.end(),
                    [=](const auto &optional) {
                        return optional.shortName == shortName;
                    }
            );

            if (it == mOptionals.end())
                error("optional argument not found: -%c", shortName);

            return *it;
        }

    private:
        void help() {
            std::list<std::string> positionals;

            std::transform(
                    mPositionals.begin(),
                    mPositionals.end(),
                    std::back_inserter(positionals),
                    [](const auto &p) {
                        return strings::format("%s(%s)", p.name.c_str(), p.type.c_str());
                    }
            );

            std::cout << strings::format(
                    "usage: %s [options] %s ... %s ...",
                    zero::filesystem::getApplicationPath().filename().string().c_str(),
                    strings::join(positionals, " ").c_str(),
                    mFooter.empty() ? "extra" : mFooter.c_str()
            ) << std::endl;

            std::cout << "positional:" << std::endl;

            for (const auto &positional : mPositionals) {
                std::cout << strings::format(
                        "\t%-20s%s(%s)",
                        positional.name.c_str(),
                        positional.desc.c_str(),
                        positional.type.c_str()
                ) << std::endl;
            }

            std::cout << "optional:" << std::endl;

            for (const auto &optional : mOptionals) {
                std::cout << strings::format(
                        optional.shortName ? "\t-%c, --%-14s%s%s" : "\t%-4c--%-14s%s%s",
                        optional.shortName ? optional.shortName : ' ',
                        optional.name.c_str(),
                        optional.desc.c_str(),
                        optional.flag ? "" : strings::format("(%s)", optional.type.c_str()).c_str()
                ) << std::endl;
            }
        }

        template<typename... Args>
        void error(const char *message, Args... args) {
            if constexpr (sizeof...(args) == 0) {
                std::cout << "error:\n\t" << message << std::endl;
            } else {
                std::cout << "error:\n\t" << zero::strings::format(message, args...) << std::endl;
            }

            help();
            exit(1);
        }

    private:
        std::string mFooter;
        std::vector<std::string> mRest;

    private:
        std::list<Optional> mOptionals;
        std::list<Positional> mPositionals;
    };
}

#endif //ZERO_CMDLINE_H
