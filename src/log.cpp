#include <zero/log.h>
#include <zero/strings/strings.h>
#include <ranges>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

constexpr auto LOGGER_BUFFER_SIZE = 1024;

tl::expected<void, std::error_code> zero::ConsoleProvider::init() {
    assert(stderr);
    return {};
}

tl::expected<void, std::error_code> zero::ConsoleProvider::rotate() {
    return {};
}

tl::expected<void, std::error_code> zero::ConsoleProvider::flush() {
    if (fflush(stderr) != 0)
        return tl::unexpected<std::error_code>(errno, std::generic_category());

    return {};
}

tl::expected<void, std::error_code> zero::ConsoleProvider::write(const LogMessage &message) {
    const std::string msg = fmt::format("{}\n", message);

    if (fwrite(msg.data(), 1, msg.length(), stderr) != msg.length())
        return tl::unexpected<std::error_code>(errno, std::generic_category());

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

tl::expected<void, std::error_code> zero::FileProvider::init() {
    const std::string name = fmt::format(
        "{}.{}.{}.log",
        mName,
        mPID,
        std::time(nullptr)
    );

    mStream.open(mDirectory / name);

    if (!mStream.is_open())
        return tl::unexpected<std::error_code>(errno, std::generic_category());

    return {};
}

tl::expected<void, std::error_code> zero::FileProvider::rotate() {
    if (mPosition < mLimit)
        return {};

    mPosition = 0;
    mStream.close();
    mStream.clear();

    std::error_code ec;
    const auto iterator = std::filesystem::directory_iterator(mDirectory, ec);

    if (ec)
        return tl::unexpected(ec);

    const std::string prefix = fmt::format("%s.%d", mName, mPID);

    std::list<std::filesystem::path> logs;
    std::ranges::copy(
        iterator
        | std::views::filter([&](const auto &entry) { return entry.is_regular_file(ec); })
        | std::views::transform(&std::filesystem::directory_entry::path)
        | std::views::filter([&](const auto &path) { return path.filename().string().starts_with(prefix); }),
        std::back_inserter(logs)
    );

    logs.sort();

    if (!std::ranges::all_of(
        logs | std::views::reverse | std::views::drop(mRemain),
        [&](const auto &path) {
            return std::filesystem::remove(path, ec);
        }
    ))
        return tl::unexpected<std::error_code>(errno, std::generic_category());

    return init();
}

tl::expected<void, std::error_code> zero::FileProvider::flush() {
    if (!mStream.flush().good())
        return tl::unexpected<std::error_code>(errno, std::generic_category());

    return {};
}

tl::expected<void, std::error_code> zero::FileProvider::write(const LogMessage &message) {
    const std::string msg = fmt::format("{}\n", message);

    if (!mStream.write(msg.c_str(), static_cast<std::streamsize>(msg.length())))
        return tl::unexpected<std::error_code>(errno, std::generic_category());

    mPosition += msg.length();
    return {};
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
                if (!it->provider->write(*message) || !it->provider->rotate()) {
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
            char env[64] = {};

            if (const DWORD n = GetEnvironmentVariableA(name, env, sizeof(env)); n == 0 || n >= sizeof(env))
                return std::nullopt;
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
