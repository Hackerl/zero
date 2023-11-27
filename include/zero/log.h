#ifndef ZERO_LOG_H
#define ZERO_LOG_H

#include "singleton.h"
#include "interface.h"
#include "concurrent/channel.h"
#include <thread>
#include <fstream>
#include <list>
#include <mutex>
#include <array>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace zero {
    constexpr auto LOG_TAGS = std::array{"ERROR", "WARN", "INFO", "DEBUG"};

    enum LogLevel {
        ERROR_LEVEL,
        WARNING_LEVEL,
        INFO_LEVEL,
        DEBUG_LEVEL
    };

    enum LogResult {
        FAILED,
        SUCCEEDED,
        ROTATED
    };

    struct LogMessage {
        LogLevel level{};
        int line{};
        std::string_view filename;
        std::chrono::time_point<std::chrono::system_clock> timestamp;
        std::string content;
    };

    class ILogProvider : public Interface {
    public:
        virtual bool init() = 0;
        virtual bool rotate() = 0;
        virtual bool flush() = 0;
        virtual LogResult write(const LogMessage &message) = 0;
    };

    class ConsoleProvider final : public ILogProvider {
    public:
        bool init() override;
        bool rotate() override;
        bool flush() override;
        LogResult write(const LogMessage &message) override;
    };

    class FileProvider final : public ILogProvider {
    public:
        explicit FileProvider(
                const char *name,
                const std::optional<std::filesystem::path> &directory = std::nullopt,
                std::size_t limit = 10 * 1024 * 1024,
                int remain = 10
        );

        bool init() override;
        bool rotate() override;
        bool flush() override;

        LogResult write(const LogMessage &message) override;

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
            LogLevel level;
            std::unique_ptr<ILogProvider> provider;
            std::chrono::milliseconds flushInterval;
            std::chrono::time_point<std::chrono::system_clock> flushDeadline;
        };

    public:
        Logger();
        ~Logger();

    private:
        void consume();

    public:
        [[nodiscard]] bool enabled(LogLevel level) const;

        void addProvider(
                LogLevel level,
                std::unique_ptr<ILogProvider> provider,
                std::chrono::milliseconds interval = std::chrono::seconds{1}
        );

        void log(const LogLevel level, const std::string_view filename, const int line, std::string content) {
            mChannel.send(
                    LogMessage{
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
        std::optional<LogLevel> mMinLogLevel;
        std::optional<LogLevel> mMaxLogLevel;
        std::optional<std::chrono::milliseconds> mTimeout;
        concurrent::Channel<LogMessage> mChannel;
    };

    static constexpr std::string_view sourceFilename(const std::string_view path) {
        const auto pos = path.find_last_of("/\\");

        if (pos == std::string_view::npos)
            return path;

        return path.substr(pos + 1);
    }
}

template<typename Char>
struct fmt::formatter<zero::LogMessage, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const zero::LogMessage &message, FmtContext &ctx) {
        return fmt::format_to(
                ctx.out(),
                "{:%Y-%m-%d %H:%M:%S} | {:<5} | {:>20}:{:<4}] {}",
                localtime(std::chrono::system_clock::to_time_t(message.timestamp)),
                zero::LOG_TAGS[message.level],
                message.filename,
                message.line,
                message.content
        );
    }
};

#define GLOBAL_LOGGER                       zero::Singleton<zero::Logger>::getInstance()
#define INIT_CONSOLE_LOG(level)             GLOBAL_LOGGER.addProvider(level, std::make_unique<zero::ConsoleProvider>())
#define INIT_FILE_LOG(level, name, ...)     GLOBAL_LOGGER.addProvider(level, std::make_unique<zero::FileProvider>(name, ## __VA_ARGS__))

#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR

#ifdef ZERO_NO_LOG
#define LOG_DEBUG(message, ...)
#define LOG_INFO(message, ...)
#define LOG_WARNING(message, ...)
#define LOG_ERROR(message, ...)
#else
#define LOG_DEBUG(message, ...)             if (auto &logger = GLOBAL_LOGGER; logger.enabled(zero::DEBUG_LEVEL)) logger.log(zero::DEBUG_LEVEL, zero::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define LOG_INFO(message, ...)              if (auto &logger = GLOBAL_LOGGER; logger.enabled(zero::INFO_LEVEL)) logger.log(zero::INFO_LEVEL, zero::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define LOG_WARNING(message, ...)           if (auto &logger = GLOBAL_LOGGER; logger.enabled(zero::WARNING_LEVEL)) logger.log(zero::WARNING_LEVEL, zero::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define LOG_ERROR(message, ...)             if (auto &logger = GLOBAL_LOGGER; logger.enabled(zero::ERROR_LEVEL)) logger.log(zero::ERROR_LEVEL, zero::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#endif

#endif //ZERO_LOG_H
