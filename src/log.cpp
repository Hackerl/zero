#include <zero/log.h>
#include <zero/time/time.h>
#include <zero/filesystem/path.h>
#include <set>

#ifndef _WIN32
#include <unistd.h>
#endif

constexpr auto LOGGER_BUFFER_SIZE = 1024;

std::string zero::stringify(const zero::LogMessage &message) {
    return zero::strings::format(
            "%s | %-5s | %20.*s:%-4d] %s\n",
            zero::time::stringify(message.timestamp).c_str(),
            zero::LOG_TAGS[message.level],
            (int) message.filename.size(),
            message.filename.data(),
            message.line,
            message.content.c_str()
    );
}

bool zero::ConsoleProvider::init() {
    return stderr != nullptr;
}

bool zero::ConsoleProvider::rotate() {
    return true;
}

bool zero::ConsoleProvider::flush() {
    return fflush(stderr) == 0;
}

zero::LogResult zero::ConsoleProvider::write(const LogMessage &message) {
    std::string msg = stringify(message);
    size_t length = msg.length();

    if (fwrite(msg.data(), 1, length, stderr) != length)
        return FAILED;

    return SUCCEEDED;
}

zero::FileProvider::FileProvider(
        const char *name,
        const std::optional<std::filesystem::path> &directory,
        size_t limit,
        int remain
) : mName(name), mLimit(limit), mRemain(remain), mPosition(0) {
    std::error_code ec;
    mDirectory = directory ? *directory : std::filesystem::temp_directory_path(ec);
#ifndef _WIN32
    mPID = getpid();
#else
    mPID = _getpid();
#endif
}

bool zero::FileProvider::init() {
    std::filesystem::path name = strings::format(
            "%s.%d.%ld.log",
            mName.c_str(),
            mPID,
            std::time(nullptr)
    );

    mStream.open(mDirectory / name);

    if (!mStream.is_open())
        return false;

    return true;
}

bool zero::FileProvider::rotate() {
    std::string prefix = strings::format("%s.%d", mName.c_str(), mPID);
    std::set<std::filesystem::path> logs;
    std::error_code ec;

    for (const auto &entry: std::filesystem::directory_iterator(mDirectory, ec)) {
        if (!entry.is_regular_file())
            continue;

        if (!entry.path().filename().string().starts_with(prefix))
            continue;

        logs.insert(entry);
    }

    size_t size = logs.size();

    if (size <= mRemain)
        return init();

    for (auto it = logs.begin(); it != std::prev(logs.end(), mRemain); it++) {
        std::filesystem::remove(*it, ec);
    }

    return init();
}

bool zero::FileProvider::flush() {
    return !mStream.flush().bad();
}

zero::LogResult zero::FileProvider::write(const LogMessage &message) {
    std::string msg = stringify(message);

    if (mStream.write(msg.c_str(), (std::streamsize) msg.length()).bad())
        return FAILED;

    mPosition += msg.length();

    if (mPosition >= mLimit) {
        mPosition = 0;
        mStream.close();
        mStream.clear();
        return ROTATED;
    }

    return SUCCEEDED;
}

zero::Logger::Logger() : mExit(false), mBuffer(LOGGER_BUFFER_SIZE) {

}

zero::Logger::~Logger() {
    mExit = true;
    mEvent.notify();

    if (mThread.joinable())
        mThread.join();
}

bool zero::Logger::enabled(zero::LogLevel level) {
    return mMaxLogLevel && *mMaxLogLevel >= level;
}

void zero::Logger::consume() {
    while (true) {
        std::optional<size_t> index = mBuffer.acquire();

        if (!index) {
            if (mExit)
                break;

            std::list<std::chrono::milliseconds> durations;

            {
                std::lock_guard<std::mutex> guard(mMutex);
                std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

                auto it = mConfigs.begin();

                while (it != mConfigs.end()) {
                    if (!it->flushDeadline) {
                        it++;
                        continue;
                    }

                    if (it->flushDeadline <= now) {
                        if (!it->provider->flush()) {
                            it = mConfigs.erase(it);
                            continue;
                        }

                        it++->flushDeadline.reset();
                        continue;
                    }

                    durations.push_back(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                    *it->flushDeadline - now
                            )
                    );

                    it++;
                }
            }

            if (durations.empty()) {
                mEvent.wait();
                continue;
            }

            mEvent.wait(*std::min_element(durations.begin(), durations.end()));
            continue;
        }

        LogMessage message = std::move(mBuffer[*index]);
        mBuffer.release(*index);

        std::lock_guard<std::mutex> guard(mMutex);
        std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

        auto it = mConfigs.begin();

        while (it != mConfigs.end()) {
            if (message.level > (std::max)(it->level, mMinLogLevel.value_or(ERROR_LEVEL))) {
                if (it->flushDeadline && it->flushDeadline <= now) {
                    if (!it->provider->flush()) {
                        it = mConfigs.erase(it);
                        continue;
                    }

                    it->flushDeadline.reset();
                }

                it++;
                continue;
            }

            LogResult result = it->provider->write(message);

            if (result == FAILED || (result == ROTATED && !it->provider->rotate())) {
                it = mConfigs.erase(it);
                continue;
            }

            it++->flushDeadline = now + it->flushInterval;
        }
    }
}

void zero::Logger::addProvider(
        zero::LogLevel level,
        std::unique_ptr<ILogProvider> provider,
        std::chrono::milliseconds interval
) {
    std::call_once(mOnceFlag, [=, this]() {
        const char *env = getenv("ZERO_LOG_LEVEL");

        if (env) {
            std::optional<int> level = strings::toNumber<int>(env);

            if (level && *level >= ERROR_LEVEL && *level <= DEBUG_LEVEL) {
                mMinLogLevel = (LogLevel) *level;
                mMaxLogLevel = mMinLogLevel;
            }
        }

        mThread = std::thread(&Logger::consume, this);
    });

    if (!provider->init())
        return;

    std::lock_guard<std::mutex> guard(mMutex);

    mMaxLogLevel = (std::max)(level, mMaxLogLevel.value_or(ERROR_LEVEL));

    mConfigs.emplace_back(
            level,
            std::move(provider),
            interval
    );
}
