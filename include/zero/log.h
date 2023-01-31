#ifndef ZERO_LOG_H
#define ZERO_LOG_H

#include "thread.h"
#include "interface.h"
#include "singleton.h"
#include "strings/strings.h"
#include "time/date.h"
#include "atomic/event.h"
#include "atomic/circular_buffer.h"
#include <fstream>
#include <list>
#include <cstring>
#include <filesystem>

#undef ERROR
#undef WARNING
#undef INFO
#undef DEBUG

namespace zero {
    constexpr const char *LOG_TAGS[] = {"ERROR", "WARN", "INFO", "DEBUG"};

    enum LogLevel {
        ERROR,
        WARNING,
        INFO,
        DEBUG
    };

    enum LogResult {
        FAILED,
        SUCCEEDED,
        ROTATED
    };

    class ILogProvider : public Interface {
    public:
        virtual bool init() = 0;
        virtual bool rotate() = 0;

    public:
        virtual LogResult write(std::string_view message) = 0;
    };

    class ConsoleProvider : public ILogProvider {
    public:
        bool init() override;
        bool rotate() override;

    public:
        LogResult write(std::string_view message) override;
    };

    class FileProvider : public ILogProvider {
    public:
        explicit FileProvider(
                const char *name,
                std::filesystem::path directory = {},
                size_t limit = 10 * 1024 * 1024,
                int remain = 10
        );

    public:
        bool init() override;
        bool rotate() override;

    public:
        LogResult write(std::string_view message) override;

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

    template<typename T, size_t N>
    class AsyncProvider : public T {
    public:
        template<typename... Args>
        explicit AsyncProvider<T, N>(Args &&... args) : T(std::forward<Args>(args)...) {
            mThread.start(&AsyncProvider<T, N>::consume);
        }

        ~AsyncProvider<T, N>() override {
            mExit = true;
            mEvent.notify();
        }

    public:
        bool init() override {
            return T::init();
        }

        bool rotate() override {
            if (!T::rotate())
                return false;

            mResult = SUCCEEDED;
            mEvent.notify();

            return true;
        }

    public:
        LogResult write(std::string_view message) override {
            std::optional<size_t> index = mBuffer.reserve();

            if (!index)
                return mResult;

            mBuffer[*index] = message;
            mBuffer.commit(*index);
            mEvent.notify();

            return mResult;
        }

    public:
        void consume() {
            while (!mExit) {
                if (mResult == ROTATED) {
                    mEvent.wait();
                    continue;
                }

                std::optional<size_t> index = mBuffer.acquire();

                if (!index) {
                    mEvent.wait();
                    continue;
                }

                mResult = T::write(mBuffer[*index]);

                if (mResult == FAILED)
                    mExit = true;

                mBuffer.release(*index);
            }
        }

    private:
        bool mExit{false};
        std::atomic<LogResult> mResult{SUCCEEDED};

    private:
        atomic::Event mEvent;
        atomic::CircularBuffer<std::string, N> mBuffer;
        Thread<AsyncProvider<T, N>> mThread{this};
    };

    class Logger {
    public:
        template<typename T, typename... Args>
        void log(LogLevel level, const T format, Args... args) {
            std::string message = strings::format(format, args...);

            auto it = mProviders.begin();

            while (it != mProviders.end()) {
                if (level > it->first) {
                    it++;
                    continue;
                }

                LogResult result = it->second->write(message);

                if (result == FAILED || (result == ROTATED && !it->second->rotate())) {
                    it = mProviders.erase(it);
                    continue;
                }

                it++;
            }
        }

    public:
        void addProvider(LogLevel level, std::unique_ptr<ILogProvider> provider) {
            if (!provider->init())
                return;

            mProviders.emplace_back(level, std::move(provider));
        }

    private:
        std::list<std::pair<LogLevel, std::unique_ptr<ILogProvider>>> mProviders;
    };
}

#define GLOBAL_LOGGER                       zero::Singleton<zero::Logger>::getInstance()
#define INIT_CONSOLE_LOG(level)             GLOBAL_LOGGER->addProvider(level, std::make_unique<zero::ConsoleProvider>())
#define INIT_FILE_LOG(level, name, ...)     GLOBAL_LOGGER->addProvider(level, std::make_unique<zero::AsyncProvider<zero::FileProvider, 100>>(name, ## __VA_ARGS__))

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
