#include <zero/log.h>
#include <zero/env.h>
#include <zero/strings.h>
#include <zero/filesystem.h>
#include <ranges>
#include <algorithm>

constexpr auto LoggerBufferSize = 1024;
constexpr auto NoSinkSentinel = -1;

void zero::log::ConsoleSink::write(const Record &record) {
    const auto message = fmt::format("{}\n", record);

    if (fwrite(message.data(), 1, message.size(), stderr) != message.size())
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};
}

void zero::log::ConsoleSink::flush() {
    if (fflush(stderr) != 0)
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};
}

zero::log::FileSink::FileSink(
    std::string name,
    std::optional<std::filesystem::path> directory,
    const std::size_t maxFileSize,
    const std::size_t maxFiles
) : mPID{os::process::currentProcessID()},
    mName{std::move(name)}, mDirectory{std::move(directory).value_or(filesystem::temporaryDirectory())},
    mMaxFileSize{maxFileSize}, mMaxFiles{maxFiles}, mPosition{0} {
    init();
}

void zero::log::FileSink::init() {
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

void zero::log::FileSink::rotate() {
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

    init();
}

std::string zero::log::FileSink::encode(const Record &record) const {
    return fmt::format("{}\n", record);
}

void zero::log::FileSink::write(const Record &record) {
    const auto message = encode(record);

    if (!mStream.write(message.c_str(), static_cast<std::streamsize>(message.size())))
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};

    mPosition += message.size();

    if (mPosition >= mMaxFileSize)
        rotate();
}

void zero::log::FileSink::flush() {
    if (!mStream.flush().good())
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};
}

zero::log::Logger::Logger() : mMaxLogLevel{NoSinkSentinel}, mPending{0},
                              mChannel{concurrent::channel<Record>(LoggerBufferSize)} {
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
                            it->sink->flush();
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
            if (record->level <= std::max(it->level, mMinLogLevel.value_or(Level::Error)) &&
                (it->tags.empty()
                     ? !record->tag
                     : record->tag.has_value() && std::ranges::contains(it->tags, *record->tag))) {
                try {
                    it->sink->write(*record);
                }
                catch (const std::exception &e) {
                    fmt::print(stderr, "Failed to write log: {}\n", e);
                    it = mConfigs.erase(it);
                    continue;
                }
            }

            if (it->flushDeadline <= now) {
                try {
                    it->sink->flush();
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

void zero::log::Logger::refreshMaxLogLevel() {
    if (mConfigs.empty()) {
        mMaxLogLevel = NoSinkSentinel;
        return;
    }

    mMaxLogLevel = std::ranges::max(
        mConfigs | std::views::transform([](const auto &config) {
            return std::to_underlying(config.level);
        })
    );

    if (mMinLogLevel)
        mMaxLogLevel = std::max(std::to_underlying(*mMinLogLevel), mMaxLogLevel.load());
}

bool zero::log::Logger::enabled(const Level level) const {
    const auto maxLevel = mMaxLogLevel.load();
    return maxLevel != NoSinkSentinel && level <= static_cast<Level>(maxLevel);
}

bool zero::log::Logger::enabled(const Level level, const std::string_view tag) const {
    if (!enabled(level))
        return false;

    const std::lock_guard guard{mMutex};

    return std::ranges::any_of(
        mConfigs,
        [&](const auto &config) {
            if (mMinLogLevel)
                return std::max(config.level, *mMinLogLevel) >= level && std::ranges::contains(config.tags, tag);

            return config.level >= level && std::ranges::contains(config.tags, tag);
        }
    );
}

void zero::log::Logger::add(
    const Level level,
    std::unique_ptr<ISink> sink,
    std::optional<std::string> name,
    std::vector<std::string> tags,
    const std::chrono::milliseconds interval
) {
    std::call_once(
        mInitFlag,
        [this] {
            const auto getOption = [](const std::string &key) -> std::optional<int> {
                const auto value = env::get(key);

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

    const std::lock_guard guard{mMutex};

    if (name && std::ranges::any_of(
        mConfigs,
        [&](const auto &config) {
            return config.name == *name;
        }))
        throw std::runtime_error{fmt::format("Sink '{}' already exists", *name)};

    mConfigs.emplace_back(
        level,
        std::move(sink),
        std::move(name),
        std::move(tags),
        interval,
        std::chrono::system_clock::now() + interval
    );

    refreshMaxLogLevel();
}

void zero::log::Logger::remove(const std::string_view name) {
    const std::lock_guard guard{mMutex};

    if (mConfigs.remove_if([&](const auto &config) {
        return config.name == name;
    }) == 0)
        throw std::runtime_error{fmt::format("Sink '{}' not found", name)};

    refreshMaxLogLevel();
}

void zero::log::Logger::setLevel(const std::string_view name, const Level level) {
    const std::lock_guard guard{mMutex};
    const auto it = std::ranges::find_if(
        mConfigs,
        [&](const auto &config) {
            return config.name == name;
        }
    );

    if (it == mConfigs.end())
        throw std::runtime_error{fmt::format("Sink '{}' not found", name)};

    it->level = level;
    refreshMaxLogLevel();
}

void zero::log::Logger::setFlushInterval(const std::string_view name, const std::chrono::milliseconds interval) {
    const std::lock_guard guard{mMutex};
    const auto it = std::ranges::find_if(
        mConfigs,
        [&](const auto &config) {
            return config.name == name;
        }
    );

    if (it == mConfigs.end())
        throw std::runtime_error{fmt::format("Sink '{}' not found", name)};

    it->flushInterval = interval;
    it->flushDeadline = std::chrono::system_clock::now() + interval;
}

void zero::log::Logger::log(
    const Level level,
    const std::string_view filename,
    const int line,
    std::string content,
    const std::optional<std::string_view> &tag
) {
    if (const auto result = mChannel.first.send(
        {
            .level = level,
            .line = line,
            .filename = filename,
            .timestamp = std::chrono::system_clock::now(),
            .content = std::move(content),
            .tag = tag
        },
        mSendTimeout
    ); !result) {
        fmt::print(stderr, "Failed to send log: {}\n", std::error_code{result.error()});
        return;
    }

    ++mPending;
}

void zero::log::Logger::sync() const {
    while (true) {
        const auto pending = mPending.load();

        if (pending == 0)
            break;

        mPending.wait(pending);
    }

    const std::lock_guard guard{mMutex};

    for (const auto &config: mConfigs) {
        try {
            config.sink->flush();
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
