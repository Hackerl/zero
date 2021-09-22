#ifndef ZERO_LOG_H
#define ZERO_LOG_H

#include "thread.h"
#include "interface.h"
#include "singleton.h"
#include "strings/str.h"
#include "time/date.h"
#include "atomic/event.h"
#include "atomic/circular_buffer.h"
#include <fstream>
#include <list>
#include <cstring>

namespace zero {
    constexpr const char * LOG_TAGS[] = {"ERROR", "WARN", "INFO", "DEBUG"};

    enum emLogLevel {
        ERROR,
        WARNING,
        INFO,
        DEBUG
    };

    class ILogProvider: public Interface {
    public:
        virtual void write(const std::string &message) = 0;

        ~ILogProvider() override = default;
    };

    class CConsoleProvider: public ILogProvider {
    public:
        void write(const std::string &message) override;
    };

    class CFileProvider: public ILogProvider {
    public:
        explicit CFileProvider(
                const char *name,
                unsigned long limit = 10 * 1024 * 1024,
                const char *directory = "/tmp",
                unsigned long remain = 10);

    private:
        std::string getLogPath();

    private:
        void clean();

    public:
        void write(const std::string &message) override;

    private:
        std::string mName;
        std::string mDirectory;

    private:
        unsigned long mLimit;
        unsigned long mRemain;

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

            mBuffer.enqueue(message);
            mEvent.notify();
        }

    public:
        void loopThread() {
            while (!mExit) {
                if (mBuffer.empty())
                    mEvent.wait();

                std::string message = {};

                if (!mBuffer.dequeue(message))
                    continue;

                T::write(message);
            }
        }

    private:
        bool mExit{false};

    private:
        atomic::CEvent mEvent;
        atomic::CircularBuffer<std::string, 100> mBuffer;
        Thread<AsyncProvider<T>> mThread{this};
    };

    struct CProviderRegister {
        emLogLevel level;
        ILogProvider *provider;
    };

    class CLogger {
    public:
        ~CLogger() {
            for (const auto &r : mRegistry) {
                delete r.provider;
            }
        }

    public:
        template<typename T, typename... Args>
        void log(emLogLevel level, const T format, Args... args) {
            std::string message = strings::format(format, args...);

            for (const auto &r : mRegistry) {
                if (level > r.level)
                    continue;

                r.provider->write(message);
            }
        }

    public:
        void addProvider(emLogLevel level, ILogProvider *provider) {
            mRegistry.push_back({level, provider});
        }

    private:
        std::list<CProviderRegister> mRegistry;
    };
}

#define INIT_CONSOLE_LOG(level)             zero::Singleton<zero::CLogger>::getInstance()->addProvider(level, new zero::CConsoleProvider())
#define INIT_FILE_LOG(level, name, args...) zero::Singleton<zero::CLogger>::getInstance()->addProvider(level, new zero::AsyncProvider<zero::CFileProvider>(name, ## args))

#define NEWLINE                             "\n"
#define SOURCE                              strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__

#define LOG_FMT                             "%s | %-5s | %20s:%-4d] "
#define LOG_TAG(level)                      zero::LOG_TAGS[level]
#define LOG_ARGS(level)                     zero::time::getTimeString().c_str(), LOG_TAG(level), SOURCE, __LINE__

#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR

#define LOG_DEBUG(message, args...)         zero::Singleton<zero::CLogger>::getInstance()->log(zero::DEBUG, LOG_FMT message NEWLINE, LOG_ARGS(zero::DEBUG), ## args)
#define LOG_INFO(message, args...)          zero::Singleton<zero::CLogger>::getInstance()->log(zero::INFO, LOG_FMT message NEWLINE, LOG_ARGS(zero::INFO), ## args)
#define LOG_WARNING(message, args...)       zero::Singleton<zero::CLogger>::getInstance()->log(zero::WARNING, LOG_FMT message NEWLINE, LOG_ARGS(zero::WARNING), ## args)
#define LOG_ERROR(message, args...)         zero::Singleton<zero::CLogger>::getInstance()->log(zero::ERROR, LOG_FMT message NEWLINE, LOG_ARGS(zero::ERROR), ## args)

#endif //ZERO_LOG_H
