#ifndef ZERO_LOG_H
#define ZERO_LOG_H

#include "utility.h"
#include "os/process.h"
#include "concurrent/channel.h"
#include <thread>
#include <fstream>
#include <mutex>
#include <list>
#include <fmt/std.h>
#include <fmt/chrono.h>

namespace zero::log {
    constexpr std::array LevelNames = {"ERROR", "WARN", "INFO", "DEBUG"};

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
        std::optional<std::string_view> tag;
    };

    class ISink {
    public:
        virtual ~ISink() = default;
        virtual void write(const Record &record) = 0;
        virtual void flush() = 0;
    };

    class ConsoleSink final : public ISink {
    public:
        void write(const Record &record) override;
        void flush() override;
    };

    class FileSink : public ISink {
    public:
        explicit FileSink(
            std::string name,
            std::optional<std::filesystem::path> directory = std::nullopt,
            std::size_t limit = 10 * 1024 * 1024,
            std::size_t maxFiles = 10
        );

    private:
        void init();
        void rotate();

    protected:
        virtual std::string encode(const Record &record) const;

    public:
        void write(const Record &record) override;
        void flush() override;

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
        static constexpr auto DefaultFlushInterval = std::chrono::seconds{1};

        struct Config {
            Level level{};
            std::unique_ptr<ISink> sink;
            std::optional<std::string> name;
            std::vector<std::string> tags;
            std::chrono::milliseconds flushInterval{};
            std::chrono::system_clock::time_point flushDeadline;
        };

    public:
        Logger();
        ~Logger();

    private:
        void consume();
        void refreshMaxLogLevel();

    public:
        [[nodiscard]] bool enabled(Level level) const;
        [[nodiscard]] bool enabled(Level level, std::string_view tag) const;

        void add(
            Level level,
            std::unique_ptr<ISink> sink,
            std::optional<std::string> name = std::nullopt,
            std::vector<std::string> tags = {},
            std::chrono::milliseconds interval = DefaultFlushInterval
        );

        void remove(std::string_view name);

        void setLevel(std::string_view name, Level level);
        void setFlushInterval(std::string_view name, std::chrono::milliseconds interval);

        void log(
            Level level,
            std::string_view filename,
            int line,
            std::string content,
            const std::optional<std::string_view> &tag = std::nullopt
        );

        void sync() const;

    private:
        mutable std::mutex mMutex;
        std::thread mThread;
        std::once_flag mInitFlag;
        std::list<Config> mConfigs;
        std::optional<Level> mMinLogLevel;
        std::atomic<int> mMaxLogLevel;
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
            zero::log::LevelNames[std::to_underlying(record.level)],
            record.filename,
            record.line,
            record.content
        );
    }
};

#define Z_GLOBAL_LOGGER                       zero::log::globalLogger()
#define Z_INIT_CONSOLE_LOG(level)             Z_GLOBAL_LOGGER.add(level, std::make_unique<zero::log::ConsoleSink>())
#define Z_INIT_FILE_LOG(level, name, ...)     Z_GLOBAL_LOGGER.add(level, std::make_unique<zero::log::FileSink>(name, ## __VA_ARGS__))

#define Z_LOG_DEBUG(message, ...)             if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Debug)) logger.log(zero::log::Level::Debug, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define Z_LOG_INFO(message, ...)              if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Info)) logger.log(zero::log::Level::Info, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define Z_LOG_WARNING(message, ...)           if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Warning)) logger.log(zero::log::Level::Warning, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))
#define Z_LOG_ERROR(message, ...)             if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Error)) logger.log(zero::log::Level::Error, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__))

#define Z_LOG_DEBUG_T(tag, message, ...)      if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Debug, tag)) logger.log(zero::log::Level::Debug, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__), tag)
#define Z_LOG_INFO_T(tag, message, ...)       if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Info, tag)) logger.log(zero::log::Level::Info, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__), tag)
#define Z_LOG_WARNING_T(tag, message, ...)    if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Warning, tag)) logger.log(zero::log::Level::Warning, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__), tag)
#define Z_LOG_ERROR_T(tag, message, ...)      if (auto &logger = Z_GLOBAL_LOGGER; logger.enabled(zero::log::Level::Error, tag)) logger.log(zero::log::Level::Error, zero::log::sourceFilename(__FILE__), __LINE__, fmt::format(message, ## __VA_ARGS__), tag)

#endif //ZERO_LOG_H
