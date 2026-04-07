#ifndef ZERO_LOG_H
#define ZERO_LOG_H

#include "utility.h"
#include "interface.h"
#include "os/process.h"
#include "concurrent/channel.h"
#include <thread>
#include <fstream>
#include <mutex>
#include <list>
#include <fmt/std.h>
#include <fmt/chrono.h>

namespace zero::log {
    constexpr std::array Tags = {"ERROR", "WARN", "INFO", "DEBUG"};

    enum class Level {
        Error,
        Warning,
        Info,
        Debug
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
            std::size_t maxFiles = 10
        );

        std::expected<void, std::error_code> init() override;
        std::expected<void, std::error_code> rotate() override;
        std::expected<void, std::error_code> flush() override;
        std::expected<void, std::error_code> write(const Record &record) override;

    private:
        os::process::ID mPID;
        std::string mName;
        std::filesystem::path mDirectory;
        std::size_t mLimit;
        std::size_t mMaxFiles;
        std::size_t mPosition;
        std::ofstream mStream;
    };

    class Logger {
        struct Config {
            Level level{};
            std::unique_ptr<IProvider> provider;
            std::chrono::milliseconds flushInterval{};
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
            if (const auto result = mChannel.first.send(
                {
                    level,
                    line,
                    filename,
                    std::chrono::system_clock::now(),
                    std::move(content)
                },
                mSendTimeout
            ); !result) {
                fmt::print(stderr, "Failed to send log: {}\n", std::error_code{result.error()});
                return;
            }

            ++mPending;
        }

        void sync();

    private:
        std::mutex mMutex;
        std::thread mThread;
        std::once_flag mInitFlag;
        std::list<Config> mConfigs;
        std::optional<Level> mMinLogLevel;
        std::optional<Level> mMaxLogLevel;
        std::optional<std::chrono::milliseconds> mSendTimeout;
        std::atomic<std::size_t> mPending;
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
            zero::localTime(std::chrono::system_clock::to_time_t(record.timestamp)),
            zero::log::Tags[std::to_underlying(record.level)],
            record.filename,
            record.line,
            record.content
        );
    }
};

#define Z_GLOBAL_LOGGER                       zero::log::globalLogger()
#define Z_INIT_CONSOLE_LOG(level)             Z_GLOBAL_LOGGER.addProvider(level, std::make_unique<zero::log::ConsoleProvider>())
#define Z_INIT_FILE_LOG(level, name, ...)     Z_GLOBAL_LOGGER.addProvider(level, std::make_unique<zero::log::FileProvider>(name, ## __VA_ARGS__))

#define Z_LOG_DEBUG(message, ...)             if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Debug)) logger.log(zero::log::Level::Debug, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define Z_LOG_INFO(message, ...)              if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Info)) logger.log(zero::log::Level::Info, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define Z_LOG_WARNING(message, ...)           if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Warning)) logger.log(zero::log::Level::Warning, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define Z_LOG_ERROR(message, ...)             if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Error)) logger.log(zero::log::Level::Error, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))

#endif //ZERO_LOG_H
