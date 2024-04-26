#ifndef ZERO_CMDLINE_H
#define ZERO_CMDLINE_H

#include "expect.h"
#include "strings/strings.h"
#include "detail/type_traits.h"
#include <any>
#include <optional>
#include <filesystem>
#include <fmt/format.h>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace zero {
    template<typename T>
    tl::expected<T, std::error_code> scan(std::string_view input) = delete;

    template<typename T>
    tl::expected<std::any, std::error_code> parseValue(const std::string_view input) {
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
        else if constexpr (detail::Vector<T>) {
            T v;

            for (const auto &token: strings::split(input, ",")) {
                auto value = parseValue<typename T::value_type>(strings::trim(token));
                EXPECT(value);
                v.emplace_back(std::any_cast<typename T::value_type>(*std::move(value)));
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
    std::string getType() {
        if constexpr (std::is_same_v<T, std::string>) {
            return "string";
        }
        else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return "path";
        }
        else if constexpr (detail::Vector<T>) {
            return fmt::format("{}[]", getType<typename T::value_type>());
        }
        else {
#if _CPPRTTI || __GXX_RTTI
#ifdef _MSC_VER
            return typeid(T).name();
#elif __GNUC__
            int status = 0;

            const std::unique_ptr<char, decltype(free) *> buffer(
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
        std::function<tl::expected<std::any, std::error_code>(std::string_view)> parse;
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
        Cmdline();

    private:
        Optional &find(char shortName);
        Optional &find(std::string_view name);
        [[nodiscard]] const Optional &find(std::string_view name) const;

        void help() const;

    public:
        template<typename T>
        void add(const char *name, const char *desc) {
            mPositionals.emplace_back(name, desc, std::any{}, TypeInfo{getType<T>(), parseValue<T>});
        }

        template<typename T>
        void addOptional(const char *name, char shortName, const char *desc, std::optional<T> def = std::nullopt) {
            mOptionals.emplace_back(
                name,
                shortName,
                desc,
                def ? std::any{*std::move(def)} : std::any{},
                TypeInfo{
                    getType<T>(),
                    parseValue<T>
                }
            );
        }

        void addOptional(const char *name, char shortName, const char *desc);

        template<typename T>
        T get(const char *name) const {
            const auto it = std::ranges::find_if(
                mPositionals,
                [=](const auto &positional) {
                    return positional.name == name;
                }
            );

            if (it == mPositionals.end())
                throw std::runtime_error(fmt::format("positional argument not found[{}]", name));

            return std::any_cast<T>(it->value);
        }

        template<typename T>
        std::optional<T> getOptional(const char *name) const {
            const auto &value = find(name).value;

            if (!value.has_value())
                return std::nullopt;

            return std::any_cast<T>(value);
        }

        [[nodiscard]] bool exist(const char *name) const;
        [[nodiscard]] std::vector<std::string> rest() const;

        void footer(const char *message);
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
