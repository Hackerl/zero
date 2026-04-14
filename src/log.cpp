#include <zero/log.h>
#include <zero/env.h>
#include <zero/strings.h>
#include <zero/filesystem.h>
#include <ranges>
#include <algorithm>

constexpr auto LoggerBufferSize = 1024;

void zero::log::ConsoleProvider::init() {
    if (!stderr)
        throw error::StacktraceError<std::runtime_error>{"Standard error stream is not available"};
}

void zero::log::ConsoleProvider::rotate() {
}

void zero::log::ConsoleProvider::flush() {
    if (fflush(stderr) != 0)
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};
}

void zero::log::ConsoleProvider::write(const Record &record) {
    const auto message = fmt::format("{}\n", record);

    if (fwrite(message.data(), 1, message.size(), stderr) != message.size())
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};
}

zero::log::FileProvider::FileProvider(
    std::string name,
    std::optional<std::filesystem::path> directory,
    const std::size_t limit,
    const std::size_t maxFiles
) : mPID{os::process::currentProcessID()},
    mName{std::move(name)}, mDirectory{std::move(directory).value_or(filesystem::temporaryDirectory())},
    mLimit{limit}, mMaxFiles{maxFiles}, mPosition{0} {
}

void zero::log::FileProvider::init() {
    const auto name = fmt::format(
        "{}.{}.{}.log",
        mName,
        mPID,
        duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    );

    mStream.open(mDirectory / filesystem::path(name));

    if (!mStream.is_open())
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};
}

void zero::log::FileProvider::rotate() {
    if (mPosition < mLimit)
        return;

    mPosition = 0;
    mStream.close();
    mStream.clear();

    const auto prefix = fmt::format("{}.{}", mName, mPID);

    std::list<std::filesystem::path> logs;
    auto iterator = error::guard(filesystem::readDirectory(mDirectory));

    while (const auto entry = error::guard(iterator.next())) {
        if (!error::guard(entry->isRegularFile()))
            continue;

        const auto &path = entry->path();

        if (!filesystem::stringify(path.filename()).starts_with(prefix))
            continue;

        logs.push_back(path);
    }

    logs.sort();

    for (const auto &log: logs | std::views::reverse | std::views::drop(mMaxFiles))
        error::guard(filesystem::remove(log));

    return init();
}

void zero::log::FileProvider::flush() {
    if (!mStream.flush().good())
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};
}

void zero::log::FileProvider::write(const Record &record) {
    const auto message = fmt::format("{}\n", record);

    if (!mStream.write(message.c_str(), static_cast<std::streamsize>(message.size())))
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};

    mPosition += message.size();
}

zero::log::Logger::Logger() : mPending{0}, mChannel{concurrent::channel<Record>(LoggerBufferSize)} {
}

zero::log::Logger::~Logger() {
    mChannel.first.close();

    if (mThread.joinable())
        mThread.join();
}

void zero::log::Logger::consume() {
    auto &receiver = mChannel.second;

    while (true) {
        std::expected<Record, std::error_code> record = receiver.tryReceive();

        if (!record) {
            if (record.error() == concurrent::TryReceiveError::Disconnected)
                break;

            std::list<std::chrono::milliseconds> durations;

            {
                const std::lock_guard guard{mMutex};

                const auto now = std::chrono::system_clock::now();
                auto it = mConfigs.begin();

                while (it != mConfigs.end()) {
                    const auto duration = duration_cast<std::chrono::milliseconds>(it->flushDeadline - now);

                    if (duration.count() <= 0) {
                        try {
                            it->provider->flush();
                        }
                        catch (const std::exception &e) {
                            fmt::print(stderr, "Failed to flush log: {}\n", e);
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

        const std::lock_guard guard{mMutex};

        const auto now = std::chrono::system_clock::now();
        auto it = mConfigs.begin();

        while (it != mConfigs.end()) {
            if (record->level <= (std::max)(it->level, mMinLogLevel.value_or(Level::Error))) {
                try {
                    it->provider->write(*record);
                }
                catch (const std::exception &e) {
                    fmt::print(stderr, "Failed to write log: {}\n", e);
                    it = mConfigs.erase(it);
                    continue;
                }

                try {
                    it->provider->rotate();
                }
                catch (const std::exception &e) {
                    fmt::print(stderr, "Failed to rotate log: {}\n", e);
                    it = mConfigs.erase(it);
                    continue;
                }
            }

            if (it->flushDeadline <= now) {
                try {
                    it->provider->flush();
                }
                catch (const std::exception &e) {
                    fmt::print(stderr, "Failed to flush log: {}\n", e);
                    it = mConfigs.erase(it);
                    continue;
                }

                it++->flushDeadline = now + it->flushInterval;
                continue;
            }

            ++it;
        }

        if (--mPending == 0)
            mPending.notify_all();
    }
}

bool zero::log::Logger::enabled(const Level level) const {
    return mMaxLogLevel && *mMaxLogLevel >= level;
}

void zero::log::Logger::addProvider(
    const Level level,
    std::unique_ptr<IProvider> provider,
    const std::chrono::milliseconds interval
) {
    std::call_once(
        mInitFlag,
        [=, this] {
            const auto getOption = [](const std::string &name) -> std::optional<int> {
                const auto value = env::get(name);

                if (!value)
                    return std::nullopt;

                const auto option = strings::toNumber<int>(*value);

                if (!option)
                    return std::nullopt;

                return *option;
            };

            if (const auto value = getOption("ZERO_LOG_LEVEL")) {
                if (const auto lv = static_cast<Level>(*value); lv < Level::Error || lv > Level::Debug)
                    return;

                mMinLogLevel = static_cast<Level>(*value);
                mMaxLogLevel = mMinLogLevel;
            }

            if (const auto value = getOption("ZERO_LOG_TIMEOUT")) {
                if (*value <= 0) {
                    mSendTimeout.reset();
                    return;
                }

                mSendTimeout = std::chrono::milliseconds{*value};
            }

            mThread = std::thread{&Logger::consume, this};
        }
    );

    try {
        provider->init();
    }
    catch (const std::exception &e) {
        fmt::print(stderr, "Failed to initialize log provider: {}\n", e);
        return;
    }

    const std::lock_guard guard{mMutex};

    mMaxLogLevel = (std::max)(level, mMaxLogLevel.value_or(Level::Error));

    mConfigs.emplace_back(
        level,
        std::move(provider),
        interval,
        std::chrono::system_clock::now() + interval
    );
}

void zero::log::Logger::sync() {
    while (true) {
        const auto pending = mPending.load();

        if (pending == 0)
            break;

        mPending.wait(pending);
    }

    const std::lock_guard guard{mMutex};

    for (const auto &config: mConfigs) {
        try {
            config.provider->flush();
        }
        catch (const std::exception &e) {
            fmt::print(stderr, "Failed to flush log: {}\n", e);
        }
    }
}

zero::log::Logger &zero::log::globalLogger() {
    static Logger instance;
    return instance;
}
