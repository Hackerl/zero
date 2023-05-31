#ifndef ZERO_CMDLINE_H
#define ZERO_CMDLINE_H

#include "strings/strings.h"
#include "filesystem/path.h"
#include <list>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <functional>
#include <iomanip>
#include <any>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace zero {
    template<typename>
    struct is_vector : std::false_type {

    };

    template<typename T, typename A>
    struct is_vector<std::vector<T, A>> : std::true_type {

    };

    template<typename T>
    inline constexpr bool is_vector_v = is_vector<T>::value;

    template<typename T>
    std::optional<T> convert(std::string_view str);

    template<typename T>
    std::any parseValue(std::string_view str) {
        if constexpr (std::is_arithmetic_v<T>) {
            std::optional<T> value = strings::toNumber<T>(str);

            if (!value)
                return {};

            return *value;
        } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::filesystem::path>) {
            return T{str};
        } else if constexpr (is_vector_v<T>) {
            T value;

            for (const auto &token: strings::split(str, ",")) {
                std::any v = parseValue<typename T::value_type>(strings::trim(token));

                if (!v.has_value())
                    return {};

                value.push_back(std::any_cast<typename T::value_type>(std::move(v)));
            }

            return value;
        } else {
            std::optional<T> value = convert<T>(str);

            if (!value)
                return {};

            return *value;
        }
    }

    template<typename T>
    std::string getType() {
        if constexpr (std::is_same_v<T, std::string>) {
            return "string";
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return "path";
        } else if constexpr (is_vector_v<T>) {
            return strings::format("%s[]", getType<typename T::value_type>().c_str());
        } else {
#if _CPPRTTI || __GXX_RTTI
#ifdef _MSC_VER
            return typeid(T).name();
#elif __GNUC__
            int status = 0;

            std::unique_ptr<char, decltype(free) *> buffer(
                    abi::__cxa_demangle(
                            typeid(T).name(),
                            nullptr,
                            nullptr,
                            &status
                    ),
                    free
            );

            if (status != 0 || !buffer)
                return "unknown";

            return buffer.get();
#endif
#else
            return "unknown";
#endif
        }
    }

    struct TypeInfo {
        std::string type;
        std::function<std::any(std::string_view)> parse;
    };

    struct Optional {
        std::string name;
        char shortName;
        std::string desc;
        std::any value;
        std::optional<TypeInfo> typeInfo;
    };

    struct Positional {
        std::string name;
        std::string desc;
        std::any value;
        TypeInfo typeInfo;
    };

    class Cmdline {
    public:
        Cmdline() {
            addOptional("help", '?', "print help message");
        }

    public:
        template<typename T>
        void add(const char *name, const char *desc) {
            mPositionals.push_back({name, desc, std::any{}, {getType<T>(), parseValue<T>}});
        }

        void addOptional(const char *name, char shortName, const char *desc) {
            mOptionals.push_back({name, shortName, desc, false});
        }

        template<typename T>
        void addOptional(const char *name, char shortName, const char *desc, std::optional<T> def = std::nullopt) {
            mOptionals.push_back(
                    {
                            name,
                            shortName,
                            desc,
                            def ? std::any{*def} : std::any{},
                            TypeInfo{
                                    getType<T>(),
                                    parseValue<T>
                            }
                    }
            );
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

        bool exist(const char *name) {
            std::optional<std::reference_wrapper<Optional>> optional = find(name);

            if (!optional)
                return false;

            return std::any_cast<bool>(optional->get().value);
        }

        template<typename T>
        std::optional<T> getOptional(const char *name) {
            std::optional<std::reference_wrapper<Optional>> optional = find(name);

            if (!optional || !optional->get().value.has_value())
                return std::nullopt;

            return std::any_cast<T>(optional->get().value);
        }

    public:
        std::string &footer() {
            return mFooter;
        }

        [[nodiscard]] std::vector<std::string> rest() const {
            return mRest;
        }

    public:
        void parse(int argc, const char *const argv[]) {
            auto it = mPositionals.begin();

            for (int i = 1; i < argc; i++) {
                if (!strings::startsWith(argv[i], "-")) {
                    if (it == mPositionals.end()) {
                        mRest.emplace_back(argv[i]);
                        continue;
                    }

                    it->value = it->typeInfo.parse(argv[i]);

                    if (!it++->value.has_value())
                        error("invalid positional argument: %s", argv[i]);

                    continue;
                }

                if (argv[i][1] != '-') {
                    std::optional<std::reference_wrapper<Optional>> optional = find(argv[i][1]);

                    if (!optional)
                        error("optional argument not found: -%c", argv[i][1]);

                    if (!optional->get().typeInfo) {
                        optional->get().value = true;
                        continue;
                    }

                    if (!argv[i + 1])
                        error("invalid optional argument: %s", argv[i]);

                    optional->get().value = optional->get().typeInfo->parse(argv[++i]);

                    if (!optional->get().value.has_value())
                        error("invalid optional argument: %s", argv[i]);

                    continue;
                }

                const char *p = strchr(argv[i], '=');
                size_t n = p ? p - argv[i] : strlen(argv[i]);

                if (n == 2) {
                    mRest.insert(mRest.end(), argv + i + 1, argv + argc);
                    break;
                }

                std::optional<std::reference_wrapper<Optional>> optional = find({argv[i] + 2, n - 2});

                if (!optional)
                    error("optional argument not found: --%.*s", n - 2, argv[i] + 2);

                if (!optional->get().typeInfo) {
                    optional->get().value = true;
                    continue;
                }

                if (!p)
                    error("optional argument requires value: %s", argv[i]);

                optional->get().value = optional->get().typeInfo->parse(p + 1);

                if (!optional->get().value.has_value())
                    error("invalid optional argument: %s", argv[i]);
            }

            if (exist("help")) {
                help();
                exit(0);
            }

            if (it != mPositionals.end())
                error("positional arguments not enough");
        }

    private:
        std::optional<std::reference_wrapper<Optional>> find(char shortName) {
            auto it = std::find_if(
                    mOptionals.begin(),
                    mOptionals.end(),
                    [=](const auto &optional) {
                        return optional.shortName == shortName;
                    }
            );

            if (it == mOptionals.end())
                return std::nullopt;

            return *it;
        }

        std::optional<std::reference_wrapper<Optional>> find(std::string_view name) {
            auto it = std::find_if(
                    mOptionals.begin(),
                    mOptionals.end(),
                    [=](const auto &optional) {
                        return optional.name == name;
                    }
            );

            if (it == mOptionals.end())
                return std::nullopt;

            return *it;
        }

    private:
        void help() const {
            std::list<std::string> positionals;

            std::transform(
                    mPositionals.begin(),
                    mPositionals.end(),
                    std::back_inserter(positionals),
                    [](const auto &p) {
                        return strings::format("%s(%s)", p.name.c_str(), p.typeInfo.type.c_str());
                    }
            );

            std::cout << "usage: "
                      << filesystem::getApplicationPath()->filename().u8string()
                      << " [options] " << strings::join(positionals, " ")
                      << " ... "
                      << (mFooter.empty() ? "extra" : mFooter)
                      << " ..."
                      << std::endl;

            std::cout << "positional:" << std::endl;

            for (const auto &positional: mPositionals) {
                std::cout << '\t'
                          << std::left << std::setw(20) << positional.name
                          << positional.desc
                          << '('
                          << positional.typeInfo.type
                          << ')'
                          << std::endl;
            }

            std::cout << "optional:" << std::endl;

            for (const auto &optional: mOptionals) {
                std::cout << '\t';

                if (optional.shortName) {
                    std::cout << '-' << optional.shortName << ", --";
                } else {
                    std::cout << "    --";
                }

                std::cout << std::left << std::setw(14) << optional.name << optional.desc;

                if (optional.typeInfo)
                    std::cout << '(' << optional.typeInfo->type << ')';

                std::cout << std::endl;
            }
        }

        template<typename... Args>
        [[noreturn]] void error(const char *message, Args... args) const {
            std::cout << "error:" << std::endl;
            std::cout << '\t';

            if constexpr (sizeof...(args) == 0) {
                std::cout << message;
            } else {
                std::cout << strings::format(message, args...);
            }

            std::cout << std::endl;

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
