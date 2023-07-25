#ifndef ZERO_LOG_H
#define ZERO_LOG_H

#include "interface.h"
#include "singleton.h"
#include "strings/strings.h"
#include "atomic/event.h"
#include "atomic/circular_buffer.h"
#include <thread>
#include <fstream>
#include <list>
#include <mutex>
#include <tuple>
#include <cstring>
#include <filesystem>

namespace zero {
    constexpr const char *LOG_TAGS[] = {"ERROR", "WARN", "INFO", "DEBUG"};

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

    std::string stringify(const LogMessage &message);

    class ILogProvider : public Interface {
    public:
        virtual bool init() = 0;
        virtual bool rotate() = 0;
        virtual bool flush() = 0;

    public:
        virtual LogResult write(const LogMessage &message) = 0;
    };

    class ConsoleProvider : public ILogProvider {
    public:
        bool init() override;
        bool rotate() override;
        bool flush() override;

    public:
        LogResult write(const LogMessage &message) override;
    };

    class FileProvider : public ILogProvider {
    public:
        explicit FileProvider(
                const char *name,
                const std::optional<std::filesystem::path> &directory = std::nullopt,
                size_t limit = 10 * 1024 * 1024,
                int remain = 10
        );

    public:
        bool init() override;
        bool rotate() override;
        bool flush() override;

    public:
        LogResult write(const LogMessage &message) override;

    private:
        std::string mName;
        std::filesystem::path mDirectory;

    private:
        int mPID;
        int mRemain;
        size_t mLimit;
        size_t mPosition;

    private:
        std::ofstream mStream;
    };

    class Logger {
    private:
        struct Config {
            LogLevel level;
            std::unique_ptr<ILogProvider> provider;
            std::chrono::milliseconds flushInterval;
            std::optional<std::chrono::time_point<std::chrono::system_clock>> flushDeadline;
        };

    public:
        Logger();
        ~Logger();

    private:
        void consume();

    public:
        void addProvider(
                LogLevel level,
                std::unique_ptr<ILogProvider> provider,
                std::chrono::milliseconds interval = std::chrono::seconds{5}
        );

    public:
        template<typename... Args>
        void log(LogLevel level, std::string_view filename, int line, const char *format, Args... args) {
            if (mExit)
                return;

            std::optional<size_t> index = mBuffer.reserve();

            if (!index)
                return;

            auto &message = mBuffer[*index];

            message = {
                    level,
                    line,
                    filename,
                    std::chrono::system_clock::now()
            };

            if constexpr (sizeof...(Args) > 0) {
                message.content = strings::format(format, args...);
            } else {
                message.content = format;
            }

            mBuffer.commit(*index);
            mEvent.notify();
        }

    private:
        std::mutex mMutex;
        std::thread mThread;
        atomic::Event mEvent;
        std::atomic<bool> mExit;
        std::once_flag mOnceFlag;
        std::list<Config> mConfigs;
        std::optional<LogLevel> mLogLevel;
        atomic::CircularBuffer<LogMessage> mBuffer;
    };

    static inline constexpr std::string_view sourceFilename(std::string_view path) {
        size_t pos = path.find_last_of("/\\");

        if (pos == std::string_view::npos)
            return path;

        return path.substr(pos + 1);
    }
}

#define GLOBAL_LOGGER                       zero::Singleton<zero::Logger>::getInstance()
#define INIT_CONSOLE_LOG(level)             GLOBAL_LOGGER->addProvider(level, std::make_unique<zero::ConsoleProvider>())
#define INIT_FILE_LOG(level, name, ...)     GLOBAL_LOGGER->addProvider(level, std::make_unique<zero::FileProvider>(name, ## __VA_ARGS__))

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
#define LOG_DEBUG(message, ...)             GLOBAL_LOGGER->log(zero::DEBUG_LEVEL, zero::sourceFilename(__FILE__), __LINE__, message, ## __VA_ARGS__)
#define LOG_INFO(message, ...)              GLOBAL_LOGGER->log(zero::INFO_LEVEL, zero::sourceFilename(__FILE__), __LINE__, message, ## __VA_ARGS__)
#define LOG_WARNING(message, ...)           GLOBAL_LOGGER->log(zero::WARNING_LEVEL, zero::sourceFilename(__FILE__), __LINE__, message, ## __VA_ARGS__)
#define LOG_ERROR(message, ...)             GLOBAL_LOGGER->log(zero::ERROR_LEVEL, zero::sourceFilename(__FILE__), __LINE__, message, ## __VA_ARGS__)
#endif

#endif //ZERO_LOG_H
