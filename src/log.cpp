#include <zero/log.h>
#include <zero/expect.h>
#include <zero/filesystem/std.h>
#include <zero/strings/strings.h>
#include <ranges>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

constexpr auto LOGGER_BUFFER_SIZE = 1024;

std::expected<void, std::error_code> zero::ConsoleProvider::init() {
    assert(stderr);
    return {};
}

std::expected<void, std::error_code> zero::ConsoleProvider::rotate() {
    return {};
}

std::expected<void, std::error_code> zero::ConsoleProvider::flush() {
    if (fflush(stderr) != 0)
        return std::unexpected(std::error_code(errno, std::generic_category()));

    return {};
}

std::expected<void, std::error_code> zero::ConsoleProvider::write(const LogRecord &record) {
    const std::string message = fmt::format("{}\n", record);

    if (fwrite(message.data(), 1, message.length(), stderr) != message.length())
        return std::unexpected(std::error_code(errno, std::generic_category()));

    return {};
}

zero::FileProvider::FileProvider(
    const char *name,
    std::optional<std::filesystem::path> directory,
    const std::size_t limit,
    const int remain
) : mRemain(remain), mLimit(limit), mPosition(0), mName(name),
    mDirectory(std::move(directory).value_or(std::filesystem::temp_directory_path())) {
#ifndef _WIN32
    mPID = getpid();
#else
    mPID = _getpid();
#endif
}

std::expected<void, std::error_code> zero::FileProvider::init() {
    const std::string name = fmt::format(
        "{}.{}.{}.log",
        mName,
        mPID,
        duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    );

    mStream.open(mDirectory / name);

    if (!mStream.is_open())
        return std::unexpected(std::error_code(errno, std::generic_category()));

    return {};
}

std::expected<void, std::error_code> zero::FileProvider::rotate() {
    if (mPosition < mLimit)
        return {};

    mPosition = 0;
    mStream.close();
    mStream.clear();

    const auto iterator = filesystem::readDirectory(mDirectory);
    EXPECT(iterator);

    const std::string prefix = fmt::format("{}.{}", mName, mPID);

    std::list<std::filesystem::path> logs;

    for (const auto &entry: *iterator) {
        EXPECT(entry);

        if (!entry->isRegularFile().value_or(false))
            continue;

        const auto &path = entry->path();

        if (!path.filename().string().starts_with(prefix))
            continue;

        logs.push_back(path);
    }

    logs.sort();

    for (const auto &log: logs | std::views::reverse | std::views::drop(mRemain)) {
        EXPECT(filesystem::remove(log));
    }

    return init();
}

std::expected<void, std::error_code> zero::FileProvider::flush() {
    if (!mStream.flush().good())
        return std::unexpected(std::error_code(errno, std::generic_category()));

    return {};
}

std::expected<void, std::error_code> zero::FileProvider::write(const LogRecord &record) {
    const std::string message = fmt::format("{}\n", record);

    if (!mStream.write(message.c_str(), static_cast<std::streamsize>(message.length())))
        return std::unexpected(std::error_code(errno, std::generic_category()));

    mPosition += message.length();
    return {};
}

zero::Logger::Logger() : mChannel(concurrent::channel<LogRecord>(LOGGER_BUFFER_SIZE)) {
}

zero::Logger::~Logger() {
    mChannel.first.close();

    if (mThread.joinable())
        mThread.join();
}

void zero::Logger::consume() {
    auto &receiver = mChannel.second;

    while (true) {
        std::expected<LogRecord, std::error_code> record = receiver.tryReceive();

        if (!record) {
            if (record.error() == concurrent::TryReceiveError::DISCONNECTED)
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
                record = receiver.receive();
            else
                record = receiver.receive(*std::ranges::min_element(durations));

            if (!record)
                continue;
        }

        std::lock_guard guard(mMutex);

        const auto now = std::chrono::system_clock::now();
        auto it = mConfigs.begin();

        while (it != mConfigs.end()) {
            if (record->level <= (std::max)(it->level, mMinLogLevel.value_or(LogLevel::ERROR_LEVEL))) {
                if (!it->provider->write(*record) || !it->provider->rotate()) {
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
    const LogLevel level,
    std::unique_ptr<ILogProvider> provider,
    const std::chrono::milliseconds interval
) {
    std::call_once(mOnceFlag, [=, this] {
        const auto getEnv = [](const char *name) -> std::optional<int> {
#ifdef _WIN32
            std::array<char, 64> env = {};

            if (const DWORD n = GetEnvironmentVariableA(name, env.data(), env.size()); n == 0 || n >= env.size())
                return std::nullopt;

            const auto result = strings::toNumber<int>(env.data());
#else
            const char *env = getenv(name);

            if (!env)
                return std::nullopt;

            const auto result = strings::toNumber<int>(env);
#endif
            if (!result)
                return std::nullopt;

            return *result;
        };

        if (const auto value = getEnv("ZERO_LOG_LEVEL")) {
            if (const auto lv = static_cast<LogLevel>(*value); lv < LogLevel::ERROR_LEVEL || lv > LogLevel::DEBUG_LEVEL)
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

    mMaxLogLevel = (std::max)(level, mMaxLogLevel.value_or(LogLevel::ERROR_LEVEL));

    mConfigs.emplace_back(
        level,
        std::move(provider),
        interval,
        std::chrono::system_clock::now() + interval
    );
}

zero::Logger &zero::globalLogger() {
    static Logger instance;
    return instance;
}
