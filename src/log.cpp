#include <zero/log.h>
#include <zero/strings/strings.h>
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
    const std::string msg = fmt::format("{}\n", message);

    if (fwrite(msg.data(), 1, msg.length(), stderr) != msg.length())
        return FAILED;

    return SUCCEEDED;
}

zero::FileProvider::FileProvider(
    const char *name,
    const std::optional<std::filesystem::path> &directory,
    const std::size_t limit,
    const int remain
) : mRemain(remain), mLimit(limit), mPosition(0), mName(name),
    mDirectory(directory.value_or(std::filesystem::temp_directory_path())) {
#ifndef _WIN32
    mPID = getpid();
#else
    mPID = _getpid();
#endif
}

bool zero::FileProvider::init() {
    const std::string name = fmt::format(
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

    const std::string prefix = fmt::format("%s.%d", mName, mPID);
    auto v = iterator
        | std::views::filter([](const auto &entry) { return entry.is_regular_file(); })
        | std::views::transform(&std::filesystem::directory_entry::path)
        | std::views::filter([&](const auto &path) { return path.filename().string().starts_with(prefix); });

    std::list<std::filesystem::path> logs;
    std::ranges::copy(v, std::back_inserter(logs));
    logs.sort();

    if (!std::ranges::all_of(
        logs | std::views::reverse | std::views::drop(mRemain),
        [&](const auto &path) {
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
    const std::string msg = fmt::format("{}\n", message);

    if (!mStream.write(msg.c_str(), static_cast<std::streamsize>(msg.length())))
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

void zero::Logger::consume() {
    while (true) {
        auto message = mChannel.tryReceive();

        if (!message) {
            if (message.error() == concurrent::ChannelError::CHANNEL_EOF)
                break;

            std::list<std::chrono::milliseconds> durations;

            {
                std::lock_guard guard(mMutex);

                const auto now = std::chrono::system_clock::now();
                auto it = mConfigs.begin();

                while (it != mConfigs.end()) {
                    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        it->flushDeadline - now
                    );

                    if (duration.count() <= 0) {
                        if (!it->provider->flush()) {
                            it = mConfigs.erase(it);
                            continue;
                        }

                        it++->flushDeadline = now + it->flushInterval;
                        continue;
                    }

                    durations.push_back(duration);
                    ++it;
                }
            }

            if (durations.empty())
                message = mChannel.receive();
            else
                message = mChannel.receive(*std::ranges::min_element(durations));

            if (!message)
                continue;
        }

        std::lock_guard guard(mMutex);

        const auto now = std::chrono::system_clock::now();
        auto it = mConfigs.begin();

        while (it != mConfigs.end()) {
            if (message->level <= (std::max)(it->level, mMinLogLevel.value_or(ERROR_LEVEL))) {
                if (const auto result = it->provider->write(*message);
                    result == FAILED || (result == ROTATED && !it->provider->rotate())) {
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

            ++it;
        }
    }
}

bool zero::Logger::enabled(const LogLevel level) const {
    return mMaxLogLevel && *mMaxLogLevel >= level;
}

void zero::Logger::addProvider(
    LogLevel level,
    std::unique_ptr<ILogProvider> provider,
    std::chrono::milliseconds interval
) {
    std::call_once(mOnceFlag, [=, this] {
        const auto getEnv = [](const char *name) -> std::optional<int> {
#ifdef _WIN32
            std::size_t size;

            if (const auto err = getenv_s(&size, nullptr, 0, name); err != 0 || size == 0)
                return std::nullopt;

            const auto buffer = std::make_unique<char[]>(size);

            if (getenv_s(&size, buffer.get(), size, name) != 0 || size == 0)
                return std::nullopt;

            const char *env = buffer.get();
#else
            const char *env = getenv(name);

            if (!env)
                return std::nullopt;
#endif

            const auto result = strings::toNumber<int>(env);

            if (!result)
                return std::nullopt;

            return *result;
        };

        if (const auto value = getEnv("ZERO_LOG_LEVEL")) {
            if (*value < ERROR_LEVEL || *value > DEBUG_LEVEL)
                return;

            mMinLogLevel = static_cast<LogLevel>(*value);
            mMaxLogLevel = mMinLogLevel;
        }

        if (const auto value = getEnv("ZERO_LOG_TIMEOUT")) {
            if (*value <= 0) {
                mTimeout.reset();
                return;
            }

            mTimeout = std::chrono::milliseconds{*value};
        }

        mThread = std::thread(&Logger::consume, this);
    });

    if (!provider->init())
        return;

    std::lock_guard guard(mMutex);

    mMaxLogLevel = (std::max)(level, mMaxLogLevel.value_or(ERROR_LEVEL));

    mConfigs.emplace_back(
        level,
        std::move(provider),
        interval,
        std::chrono::system_clock::now() + interval
    );
}
