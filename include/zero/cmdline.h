#ifndef ZERO_CMDLINE_H
#define ZERO_CMDLINE_H

#include "expect.h"
#include "strings/strings.h"
#include "detail/type_traits.h"
#include <any>
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
    std::expected<T, std::error_code> scan(std::string_view input) = delete;

    template<typename T>
    std::expected<std::any, std::error_code> parseValue(const std::string_view input) {
        if constexpr (std::is_arithmetic_v<T>) {
            const auto value = strings::toNumber<T>(input);
            EXPECT(value);
            return *value;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return std::string{input};
        }
        else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return std::filesystem::path{input};
        }
        else if constexpr (detail::is_specialization_v<T, std::vector>) {
            T v;

            for (const auto &token: strings::split(input, ",")) {
                auto value = parseValue<typename T::value_type>(strings::trim(token));
                EXPECT(value);
                v.push_back(std::move(std::any_cast<typename T::value_type>(*value)));
            }

            return v;
        }
        else {
            auto value = scan<T>(input);
            EXPECT(value);
            return *std::move(value);
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
        else if constexpr (detail::is_specialization_v<T, std::vector>) {
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
            std::function<std::expected<std::any, std::error_code>(std::string_view)> parse;
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
        T get(const std::string_view name) const {
            const auto it = std::ranges::find_if(
                mPositionals,
                [&](const auto &positional) {
                    return positional.name == name;
                }
            );

            if (it == mPositionals.end())
                throw std::runtime_error{fmt::format("positional argument not found[{}]", name)};

            return std::any_cast<T>(it->value);
        }

        template<typename T>
        std::optional<T> getOptional(const std::string_view name) const {
            const auto &value = find(name).value;

            if (!value.has_value())
                return std::nullopt;

            return std::any_cast<T>(value);
        }

        [[nodiscard]] bool exist(std::string_view name) const;
        [[nodiscard]] std::vector<std::string> rest() const;

        void footer(std::string message);
        void from(int argc, const char *const argv[]);
        void parse(int argc, const char *const argv[]);

    private:
        std::vector<std::string> mRest;
        std::optional<std::string> mFooter;
        std::list<Optional> mOptionals;
        std::list<Positional> mPositionals;
    };
}

#endif //ZERO_CMDLINE_H
