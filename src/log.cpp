#include <zero/log.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/path.h>
#include <ranges>
#include <algorithm>

#ifndef _WIN32
#include <unistd.h>
#endif

constexpr auto LOGGER_BUFFER_SIZE = 1024;

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
    auto msg = fmt::format("{}\n", message);
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
) : mName(name), mLimit(limit), mRemain(remain), mPosition(0),
    mDirectory(directory.value_or(std::filesystem::temp_directory_path())) {
#ifndef _WIN32
    mPID = getpid();
#else
    mPID = _getpid();
#endif
}

bool zero::FileProvider::init() {
    auto name = fmt::format(
            "{}.{}.{}.log",
            mName,
            mPID,
            std::time(nullptr)
    );

    mStream.open(mDirectory / name);

    if (!mStream.is_open())
        return false;

    return true;
}

bool zero::FileProvider::rotate() {
    std::error_code ec;
    auto iterator = std::filesystem::directory_iterator(mDirectory, ec);

    if (ec)
        return false;

    auto prefix = fmt::format("%s.%d", mName, mPID);
    auto v = iterator
             | std::views::filter([](const auto &entry) { return entry.is_regular_file(); })
             | std::views::transform(&std::filesystem::directory_entry::path)
             | std::views::filter([&](const auto &path) { return path.filename().string().starts_with(prefix); });

    std::list<std::filesystem::path> logs;

    for (const auto &path: v)
        logs.push_back(path);

    logs.sort();

    if (!std::ranges::all_of(
            logs | std::views::reverse | std::views::drop(mRemain),
            [](const auto &path) {
                std::error_code ec;
                return std::filesystem::remove(path, ec);
            }
    ))
        return false;

    return init();
}

bool zero::FileProvider::flush() {
    return mStream.flush().good();
}

zero::LogResult zero::FileProvider::write(const LogMessage &message) {
    auto msg = fmt::format("{}\n", message);

    if (!mStream.write(msg.c_str(), (std::streamsize) msg.length()))
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

zero::Logger::Logger() : mChannel(LOGGER_BUFFER_SIZE) {

}

zero::Logger::~Logger() {
    mChannel.close();

    if (mThread.joinable())
        mThread.join();
}

bool zero::Logger::enabled(zero::LogLevel level) {
    return mMaxLogLevel && *mMaxLogLevel >= level;
}

void zero::Logger::consume() {
    while (true) {
        auto message = mChannel.tryReceive();

        if (!message) {
            if (message.error() == concurrent::ChannelError::CHANNEL_EOF)
                break;

            std::list<std::chrono::milliseconds> durations;

            {
                std::lock_guard<std::mutex> guard(mMutex);

                auto now = std::chrono::system_clock::now();
                auto it = mConfigs.begin();

                while (it != mConfigs.end()) {
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(it->flushDeadline - now);

                    if (duration.count() <= 0) {
                        if (!it->provider->flush()) {
                            it = mConfigs.erase(it);
                            continue;
                        }

                        it++->flushDeadline = now + it->flushInterval;
                        continue;
                    }

                    durations.push_back(duration);
                    it++;
                }
            }

            if (durations.empty())
                message = mChannel.receive();
            else
                message = mChannel.receive(*std::min_element(durations.begin(), durations.end()));

            if (!message)
                continue;
        }

        std::lock_guard<std::mutex> guard(mMutex);

        auto now = std::chrono::system_clock::now();
        auto it = mConfigs.begin();

        while (it != mConfigs.end()) {
            if (message->level <= (std::max)(it->level, mMinLogLevel.value_or(ERROR_LEVEL))) {
                auto result = it->provider->write(*message);

                if (result == FAILED || (result == ROTATED && !it->provider->rotate())) {
                    it = mConfigs.erase(it);
                    continue;
                }
            }

            if (it->flushDeadline <= now) {
                if (!it->provider->flush()) {
                    it = mConfigs.erase(it);
                    continue;
                }

                it++->flushDeadline = now + it->flushInterval;
                continue;
            }

            it++;
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
            strings::toNumber<int>(env).transform([&](int level) {
                if (level < ERROR_LEVEL || level > DEBUG_LEVEL)
                    return;

                mMinLogLevel = (LogLevel) level;
                mMaxLogLevel = mMinLogLevel;
            });
        }

        env = getenv("ZERO_LOG_TIMEOUT");

        if (env) {
            strings::toNumber<int>(env).transform([&](int timeout) {
                if (timeout <= 0) {
                    mTimeout.reset();
                    return;
                }

                mTimeout = std::chrono::milliseconds{timeout};
            });
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
            interval,
            std::chrono::system_clock::now() + interval
    );
}
