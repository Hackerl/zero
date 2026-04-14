#ifndef ZERO_CMDLINE_H
#define ZERO_CMDLINE_H

#include "strings.h"
#include "meta/type_traits.h"
#include <any>
#include <span>
#include <list>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <fmt/format.h>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace zero {
    template<typename T>
    T scan(std::string_view input) = delete;

    template<typename T>
    std::any parseValue(const std::string_view input) {
        if constexpr (std::is_arithmetic_v<T>) {
            const auto value = strings::toNumber<T>(input);

            if (!value)
                throw std::system_error{value.error()};

            return *value;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return std::string{input};
        }
        else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return std::filesystem::path{input};
        }
        else if constexpr (meta::IsSpecialization<T, std::vector>) {
            using ValueType = T::value_type;

            T value;

            for (const auto &token: strings::split(input, ",")) {
                auto element = parseValue<ValueType>(strings::trim(token));
                value.push_back(std::move(std::any_cast<ValueType>(element)));
            }

            return value;
        }
        else {
            return scan<T>(input);
        }
    }

    template<typename T>
    std::string getTypeName() {
        if constexpr (std::is_same_v<T, std::string>) {
            return "string";
        }
        else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return "path";
        }
        else if constexpr (meta::IsSpecialization<T, std::vector>) {
            return fmt::format("{}[]", getTypeName<typename T::value_type>());
        }
        else {
#if defined(_CPPRTTI) || defined(__GXX_RTTI)
#ifdef _MSC_VER
            return typeid(T).name();
#elif defined(__GNUC__)
            int status{};

            const std::unique_ptr<char, decltype(&free)> buffer{
                abi::__cxa_demangle(
                    typeid(T).name(),
                    nullptr,
                    nullptr,
                    &status
                ),
                free
            };

            if (status != 0 || !buffer)
                return "unknown";

            return buffer.get();
#endif
#else
            return "unknown";
#endif
        }
    }

    class Cmdline {
        struct TypeInfo {
            std::string name;
            std::function<std::any(std::string_view)> parse;
        };

        struct Optional {
            std::string name;
            char shortName{};
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

    public:
        Cmdline();

    private:
        Optional &find(char shortName);
        Optional &find(std::string_view name);
        [[nodiscard]] const Optional &find(std::string_view name) const;

        void help() const;

    public:
        template<typename T>
        void add(std::string name, std::string desc) {
            mPositionals.emplace_back(
                std::move(name),
                std::move(desc),
                std::any{},
                TypeInfo{getTypeName<T>(), parseValue<T>}
            );
        }

        template<typename T>
        void addOptional(
            std::string name,
            const char shortName,
            std::string desc,
            std::optional<T> def = std::nullopt
        ) {
            mOptionals.emplace_back(
                std::move(name),
                shortName,
                std::move(desc),
                def ? std::any{*std::move(def)} : std::any{},
                TypeInfo{
                    getTypeName<T>(),
                    parseValue<T>
                }
            );
        }

        void addOptional(std::string name, char shortName, std::string desc);

        template<typename T>
        [[nodiscard]] T get(const std::string_view name) const {
            const auto it = std::ranges::find_if(
                mPositionals,
                [&](const auto &positional) {
                    return positional.name == name;
                }
            );

            if (it == mPositionals.end())
                throw std::runtime_error{fmt::format("Unknown positional argument '{}'", name)};

            return std::any_cast<T>(it->value);
        }

        template<typename T>
        [[nodiscard]] std::optional<T> getOptional(const std::string_view name) const {
            const auto &value = find(name).value;

            if (!value.has_value())
                return std::nullopt;

            return std::any_cast<T>(value);
        }

        [[nodiscard]] bool exist(std::string_view name) const;
        [[nodiscard]] std::vector<std::string> rest() const;

        void footer(std::string message);
        void parse(std::span<const std::string_view> arguments);
        void parse(int argc, const char *const argv[]);

    private:
        std::vector<std::string> mRest;
        std::optional<std::string> mFooter;
        std::list<Optional> mOptionals;
        std::list<Positional> mPositionals;
    };
}

#endif //ZERO_CMDLINE_H
