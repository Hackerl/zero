#ifndef ZERO_LOG_H
#define ZERO_LOG_H

#include "thread.h"
#include "interface.h"
#include "singleton.h"
#include "zero/strings/strings.h"
#include "zero/time/date.h"
#include "zero/atomic/event.h"
#include "zero/atomic/circular_buffer.h"
#include <fstream>
#include <list>
#include <cstring>
#include <filesystem>

#undef ERROR
#undef WARNING
#undef INFO
#undef DEBUG

namespace zero {
    constexpr const char * LOG_TAGS[] = {"ERROR", "WARN", "INFO", "DEBUG"};

    enum LogLevel {
        ERROR,
        WARNING,
        INFO,
        DEBUG
    };

    class ILogProvider: public Interface {
    public:
        virtual void write(const std::string &message) = 0;
    };

    class ConsoleProvider: public ILogProvider {
    public:
        void write(const std::string &message) override;
    };

    class FileProvider: public ILogProvider {
    public:
        explicit FileProvider(
                const std::string &name,
                const std::filesystem::path &directory = "",
                long limit = 10 * 1024 * 1024,
                int remain = 10);

    private:
        std::filesystem::path getLogPath();

    private:
        void clean();

    public:
        void write(const std::string &message) override;

    private:
        std::string mName;
        std::filesystem::path mDirectory;

    private:
        int mPID;
        int mRemain;
        long mLimit;

    private:
        std::ofstream mFile;
    };

    template<typename T>
    class AsyncProvider: public T {
    public:
        template<typename... Args>
        explicit AsyncProvider<T>(Args... args) : T(args...) {
            mThread.start(&AsyncProvider<T>::loopThread);
        }

        ~AsyncProvider<T>() override {
            mExit = true;
            mEvent.notify();
            mThread.stop();
        }

    public:
        void write(const std::string &message) override {
            if (mBuffer.full())
                return;

            std::optional<size_t> index = mBuffer.reserve();

            if (!index)
                return;

            mBuffer[*index] = message;
            mBuffer.commit(*index);

            mEvent.notify();
        }

    public:
        void loopThread() {
            while (!mExit) {
                if (mBuffer.empty())
                    mEvent.wait();

                std::optional<size_t> index = mBuffer.acquire();

                if (!index)
                    return;

                T::write(mBuffer[*index]);
                mBuffer.release(*index);
            }
        }

    private:
        bool mExit{false};

    private:
        atomic::Event mEvent;
        atomic::CircularBuffer<std::string, 100> mBuffer;
        Thread<AsyncProvider<T>> mThread{this};
    };

    struct ProviderRegister {
        LogLevel level;
        ILogProvider *provider;
    };

    class Logger {
    public:
        ~Logger() {
            for (const auto &r : mRegistry) {
                delete r.provider;
            }
        }

    public:
        template<typename T, typename... Args>
        void log(LogLevel level, const T format, Args... args) {
            std::string message = strings::format(format, args...);

            for (const auto &r : mRegistry) {
                if (level > r.level)
                    continue;

                r.provider->write(message);
            }
        }

    public:
        void addProvider(LogLevel level, ILogProvider *provider) {
            mRegistry.push_back({level, provider});
        }

    private:
        std::list<ProviderRegister> mRegistry;
    };
}

#define GLOBAL_LOGGER                       zero::Singleton<zero::Logger>::getInstance()
#define INIT_CONSOLE_LOG(level)             GLOBAL_LOGGER->addProvider(level, new zero::ConsoleProvider())
#define INIT_FILE_LOG(level, name, ...)     GLOBAL_LOGGER->addProvider(level, new zero::AsyncProvider<zero::FileProvider>(name, ## __VA_ARGS__))

#define NEWLINE                             "\n"
#define SOURCE                              std::filesystem::path(__FILE__).filename().string().c_str()

#define LOG_FMT                             "%s | %-5s | %20s:%-4d] "
#define LOG_TAG(level)                      zero::LOG_TAGS[level]
#define LOG_ARGS(level)                     zero::time::getTimeString().c_str(), LOG_TAG(level), SOURCE, __LINE__

#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR

#define LOG_DEBUG(message, ...)             GLOBAL_LOGGER->log(zero::DEBUG, LOG_FMT message NEWLINE, LOG_ARGS(zero::DEBUG), ## __VA_ARGS__)
#define LOG_INFO(message, ...)              GLOBAL_LOGGER->log(zero::INFO, LOG_FMT message NEWLINE, LOG_ARGS(zero::INFO), ## __VA_ARGS__)
#define LOG_WARNING(message, ...)           GLOBAL_LOGGER->log(zero::WARNING, LOG_FMT message NEWLINE, LOG_ARGS(zero::WARNING), ## __VA_ARGS__)
#define LOG_ERROR(message, ...)             GLOBAL_LOGGER->log(zero::ERROR, LOG_FMT message NEWLINE, LOG_ARGS(zero::ERROR), ## __VA_ARGS__)

#endif //ZERO_LOG_H
