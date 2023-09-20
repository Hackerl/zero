#ifndef ZERO_CMDLINE_H
#define ZERO_CMDLINE_H

#include "strings/strings.h"
#include "filesystem/path.h"
#include "detail/type_traits.h"
#include <optional>
#include <any>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace zero {
    template<typename T>
    tl::expected<T, std::error_code> fromCommandLine(const std::string &str);

    template<typename T>
    tl::expected<std::any, std::error_code> parseValue(const std::string &str) {
        if constexpr (std::is_arithmetic_v<T>) {
            auto value = strings::toNumber<T>(str);

            if (!value)
                return tl::unexpected(value.error());

            return *value;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return str;
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return std::filesystem::path{str};
        } else if constexpr (detail::is_specialization<T, std::vector>) {
            tl::expected<T, std::error_code> value;

            for (const auto &token: strings::split(str, ",")) {
                auto v = parseValue<typename T::value_type>(strings::trim(token));

                if (!v) {
                    value = tl::unexpected(v.error());
                    break;
                }

                value->emplace_back(std::any_cast<typename T::value_type>(std::move(*v)));
            }

            return *value;
        } else {
            auto value = fromCommandLine<T>(str);

            if (!value)
                return tl::unexpected(value.error());

            return *value;
        }
    }

    template<typename T>
    std::string getType() {
        if constexpr (std::is_same_v<T, std::string>) {
            return "string";
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return "path";
        } else if constexpr (detail::is_specialization<T, std::vector>) {
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
        std::function<tl::expected<std::any, std::error_code>(const std::string &)> parse;
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

    public:
        template<typename T>
        void add(const char *name, const char *desc) {
            mPositionals.push_back(
                    Positional{
                            name,
                            desc,
                            std::any{},
                            TypeInfo{
                                    getType<T>(),
                                    parseValue<T>
                            }
                    }
            );
        }

        template<typename T>
        void addOptional(const char *name, char shortName, const char *desc, std::optional<T> def = std::nullopt) {
            mOptionals.push_back(
                Optional{
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

        void addOptional(const char *name, char shortName, const char *desc);

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
                throw std::runtime_error(strings::format("positional argument not found[%s]", name));

            return std::any_cast<T>(it->value);
        }

        template<typename T>
        std::optional<T> getOptional(const char *name) {
            const auto &optional = find(name);

            if (!optional.value.has_value())
                return std::nullopt;

            return std::any_cast<T>(optional.value);
        }

        bool exist(const char *name);

    public:
        std::string &footer();
        [[nodiscard]] std::vector<std::string> rest() const;

    public:
        void from(int argc, const char *const argv[]);
        void parse(int argc, const char *const argv[]);

    private:
        Optional &find(char shortName);
        Optional &find(const std::string &name);

    private:
        void help() const;

    private:
        std::string mFooter;
        std::vector<std::string> mRest;

    private:
        std::list<Optional> mOptionals;
        std::list<Positional> mPositionals;
    };
}

#endif //ZERO_CMDLINE_H
