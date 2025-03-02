#ifndef CATCH_EXTENSIONS_H
#define CATCH_EXTENSIONS_H

#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS

#include <random>
#include <fmt/std.h>
#include <zero/formatter.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

template<>
struct Catch::StringMaker<std::exception_ptr> {
    static std::string convert(const std::exception_ptr &ptr) {
        return fmt::to_string(ptr);
    }
};

template<typename T, typename E>
struct Catch::StringMaker<std::expected<T, E>> {
    static std::string convert(const std::expected<T, E> &expected) {
        if (!expected)
            return fmt::format("unexpected({})", StringMaker<E>::convert(expected.error()));

        if constexpr (std::is_void_v<T>)
            return "expected()";
        else
            return fmt::format("expected({})", StringMaker<T>::convert(*expected));
    }
};

template<>
struct Catch::StringMaker<std::error_code> {
    static std::string convert(const std::error_code &ec) {
        return fmt::format("{} ({})", ec.message(), ec);
    }
};

template<typename T>
    requires std::is_error_code_enum_v<T>
struct Catch::StringMaker<T> {
    static std::string convert(const T &error) {
        return StringMaker<std::error_code>::convert(error);
    }
};

template<>
struct Catch::StringMaker<std::error_condition> {
    static std::string convert(const std::error_condition &condition) {
        return fmt::format("{} ({}:{})", condition.message(), condition.category().name(), condition.value());
    }
};

template<typename T>
    requires std::is_error_condition_enum_v<T>
struct Catch::StringMaker<T> {
    static std::string convert(const T &error) {
        return StringMaker<std::error_condition>::convert(error);
    }
};

#define INTERNAL_REQUIRE_ERROR(var, expr, err)  \
    const auto var = expr;                      \
    REQUIRE_FALSE(var);                         \
    REQUIRE((var).error() == (err))

#define REQUIRE_ERROR(expr, err) INTERNAL_REQUIRE_ERROR(INTERNAL_CATCH_UNIQUE_NAME(_result), expr, err)

class RandomBytesGenerator final : public Catch::Generators::IGenerator<std::vector<std::byte>> {
public:
    RandomBytesGenerator(const std::size_t minLength, const std::size_t maxLength, const unsigned int seed)
        : mGenerator{seed},
          mLengthDistribution{minLength, maxLength},
          mByteDistribution{0, 255} {
        next();
    }

private:
    bool next() override {
        mBytes.resize(mLengthDistribution(mGenerator));
        std::ranges::generate(mBytes, [this] { return static_cast<std::byte>(mByteDistribution(mGenerator)); });
        return true;
    }

public:
    const std::vector<std::byte> &get() const override {
        return mBytes;
    }

private:
    std::mt19937 mGenerator;
    std::uniform_int_distribution<std::size_t> mLengthDistribution;
    std::uniform_int_distribution<std::size_t> mByteDistribution;
    std::vector<std::byte> mBytes;
};

inline Catch::Generators::GeneratorWrapper<std::vector<std::byte>>
randomBytes(const std::size_t minLength, const std::size_t maxLength) {
    return {
        Catch::Detail::make_unique<RandomBytesGenerator>(minLength, maxLength, Catch::Generators::Detail::getSeed())
    };
}

class RandomStringGenerator final : public Catch::Generators::IGenerator<std::string> {
public:
    RandomStringGenerator(
        const std::size_t minLength,
        const std::size_t maxLength,
        std::string charset,
        const unsigned int seed
    ): mGenerator{seed}, mLengthDistribution{minLength, maxLength},
       mIndexDistribution{0, charset.size() - 1}, mCharset{std::move(charset)} {
        next();
    }

private:
    bool next() override {
        mString.resize(mLengthDistribution(mGenerator));
        std::ranges::generate(mString, [this] { return mCharset[mIndexDistribution(mGenerator)]; });
        return true;
    }

public:
    const std::string &get() const override {
        return mString;
    }

private:
    std::mt19937 mGenerator;
    std::uniform_int_distribution<std::size_t> mLengthDistribution;
    std::uniform_int_distribution<std::size_t> mIndexDistribution;
    std::string mCharset;
    std::string mString;
};

inline Catch::Generators::GeneratorWrapper<std::string>
randomString(const std::size_t minLength, const std::size_t maxLength) {
    return {
        Catch::Detail::make_unique<RandomStringGenerator>(
            minLength,
            maxLength,
            "0123456789"
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            R"(!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~ )"
            "\t\n\r\x0b\x0c",
            Catch::Generators::Detail::getSeed()
        )
    };
}

inline Catch::Generators::GeneratorWrapper<std::string>
randomAlphanumericString(const std::size_t minLength, const std::size_t maxLength) {
    return {
        Catch::Detail::make_unique<RandomStringGenerator>(
            minLength,
            maxLength,
            "0123456789"
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
            Catch::Generators::Detail::getSeed()
        )
    };
}

#endif //CATCH_EXTENSIONS_H
