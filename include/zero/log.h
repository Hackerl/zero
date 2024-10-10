#ifndef ZERO_LOG_H
#define ZERO_LOG_H

#include "interface.h"
#include "concurrent/channel.h"
#include <thread>
#include <fstream>
#include <list>
#include <mutex>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace zero::log {
    constexpr std::array TAGS = {"ERROR", "WARN", "INFO", "DEBUG"};

    enum class Level {
        ERROR_LEVEL,
        WARNING_LEVEL,
        INFO_LEVEL,
        DEBUG_LEVEL
    };

    struct Record {
        Level level{};
        int line{};
        std::string_view filename;
        std::chrono::system_clock::time_point timestamp;
        std::string content;
    };

    class IProvider : public Interface {
    public:
        virtual std::expected<void, std::error_code> init() = 0;
        virtual std::expected<void, std::error_code> rotate() = 0;
        virtual std::expected<void, std::error_code> flush() = 0;
        virtual std::expected<void, std::error_code> write(const Record &record) = 0;
    };

    class ConsoleProvider final : public IProvider {
    public:
        std::expected<void, std::error_code> init() override;
        std::expected<void, std::error_code> rotate() override;
        std::expected<void, std::error_code> flush() override;
        std::expected<void, std::error_code> write(const Record &record) override;
    };

    class FileProvider final : public IProvider {
    public:
        explicit FileProvider(
            std::string name,
            std::optional<std::filesystem::path> directory = std::nullopt,
            std::size_t limit = 10 * 1024 * 1024,
            int remain = 10
        );

        std::expected<void, std::error_code> init() override;
        std::expected<void, std::error_code> rotate() override;
        std::expected<void, std::error_code> flush() override;
        std::expected<void, std::error_code> write(const Record &record) override;

    private:
        int mPID;
        int mRemain;
        std::size_t mLimit;
        std::size_t mPosition;
        std::ofstream mStream;
        std::string mName;
        std::filesystem::path mDirectory;
    };

    class Logger {
        struct Config {
            Level level;
            std::unique_ptr<IProvider> provider;
            std::chrono::milliseconds flushInterval;
            std::chrono::system_clock::time_point flushDeadline;
        };

    public:
        Logger();
        ~Logger();

    private:
        void consume();

    public:
        [[nodiscard]] bool enabled(Level level) const;

        void addProvider(
            Level level,
            std::unique_ptr<IProvider> provider,
            std::chrono::milliseconds interval = std::chrono::seconds{1}
        );

        void log(const Level level, const std::string_view filename, const int line, std::string content) {
            std::ignore = mChannel.first.send(
                {
                    level,
                    line,
                    filename,
                    std::chrono::system_clock::now(),
                    std::move(content)
                },
                mTimeout
            );
        }

    private:
        std::mutex mMutex;
        std::thread mThread;
        std::once_flag mOnceFlag;
        std::list<Config> mConfigs;
        std::optional<Level> mMinLogLevel;
        std::optional<Level> mMaxLogLevel;
        std::optional<std::chrono::milliseconds> mTimeout;
        concurrent::Channel<Record> mChannel;
    };

    Logger &globalLogger();

    // ReSharper disable once CppDFALocalValueEscapesFunction
    constexpr std::string_view sourceFilename(const std::string_view path) {
        const auto pos = path.find_last_of("/\\");

        if (pos == std::string_view::npos)
            return path;

        return path.substr(pos + 1);
    }
}

template<typename Char>
struct fmt::formatter<zero::log::Record, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const zero::log::Record &record, FmtContext &ctx) {
        return fmt::format_to(
            ctx.out(),
            "{:%Y-%m-%d %H:%M:%S} | {:<5} | {:>20}:{:<4}] {}",
            localtime(std::chrono::system_clock::to_time_t(record.timestamp)),
            zero::log::TAGS[std::to_underlying(record.level)],
            record.filename,
            record.line,
            record.content
        );
    }
};

#define GLOBAL_LOGGER                       zero::log::globalLogger()
#define INIT_CONSOLE_LOG(level)             GLOBAL_LOGGER.addProvider(level, std::make_unique<zero::log::ConsoleProvider>())
#define INIT_FILE_LOG(level, name, ...)     GLOBAL_LOGGER.addProvider(level, std::make_unique<zero::log::FileProvider>(name, ## __VA_ARGS__))

#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR

#define LOG_DEBUG(message, ...)             if (auto &logger = GLOBAL_LOGGER; logger.enabled(zero::log::Level::DEBUG_LEVEL)) logger.log(zero::log::Level::DEBUG_LEVEL, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define LOG_INFO(message, ...)              if (auto &logger = GLOBAL_LOGGER; logger.enabled(zero::log::Level::INFO_LEVEL)) logger.log(zero::log::Level::INFO_LEVEL, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define LOG_WARNING(message, ...)           if (auto &logger = GLOBAL_LOGGER; logger.enabled(zero::log::Level::WARNING_LEVEL)) logger.log(zero::log::Level::WARNING_LEVEL, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define LOG_ERROR(message, ...)             if (auto &logger = GLOBAL_LOGGER; logger.enabled(zero::log::Level::ERROR_LEVEL)) logger.log(zero::log::Level::ERROR_LEVEL, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))

#endif //ZERO_LOG_H
