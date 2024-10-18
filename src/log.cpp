#include <zero/log.h>
#include <zero/env.h>
#include <zero/expect.h>
#include <zero/filesystem/std.h>
#include <zero/strings/strings.h>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#ifndef _WIN32
#include <unistd.h>
#endif

constexpr auto LOGGER_BUFFER_SIZE = 1024;

tl::expected<void, std::error_code> zero::log::ConsoleProvider::init() {
    assert(stderr);
    return {};
}

tl::expected<void, std::error_code> zero::log::ConsoleProvider::rotate() {
    return {};
}

tl::expected<void, std::error_code> zero::log::ConsoleProvider::flush() {
    if (fflush(stderr) != 0)
        return tl::unexpected{std::error_code{errno, std::generic_category()}};

    return {};
}

tl::expected<void, std::error_code> zero::log::ConsoleProvider::write(const Record &record) {
    const auto message = fmt::format("{}\n", record);

    if (fwrite(message.data(), 1, message.size(), stderr) != message.size())
        return tl::unexpected{std::error_code{errno, std::generic_category()}};

    return {};
}

zero::log::FileProvider::FileProvider(
    std::string name,
    std::optional<std::filesystem::path> directory,
    const std::size_t limit,
    const int remain
) : mPID{
#ifdef _WIN32
        _getpid()
#else
        getpid()
#endif
    },
    mRemain{remain}, mLimit{limit}, mPosition{0}, mName{std::move(name)},
    mDirectory{std::move(directory).value_or(std::filesystem::temp_directory_path())} {
}

tl::expected<void, std::error_code> zero::log::FileProvider::init() {
    const auto name = fmt::format(
        "{}.{}.{}.log",
        mName,
        mPID,
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    mStream.open(mDirectory / name);

    if (!mStream.is_open())
        return tl::unexpected{std::error_code{errno, std::generic_category()}};

    return {};
}

tl::expected<void, std::error_code> zero::log::FileProvider::rotate() {
    if (mPosition < mLimit)
        return {};

    mPosition = 0;
    mStream.close();
    mStream.clear();

    const auto iterator = filesystem::readDirectory(mDirectory);
    EXPECT(iterator);

    const auto prefix = fmt::format("{}.{}", mName, mPID);

    std::list<std::filesystem::path> logs;

    for (const auto &entry: *iterator) {
        EXPECT(entry);

        if (!entry->isRegularFile().value_or(false))
            continue;

        const auto &path = entry->path();

        if (!strings::startsWith(path.filename().string(), prefix))
            continue;

        logs.push_back(path);
    }

    logs.sort();

    for (const auto &log: logs | ranges::views::reverse | ranges::views::drop(mRemain)) {
        EXPECT(filesystem::remove(log));
    }

    return init();
}

tl::expected<void, std::error_code> zero::log::FileProvider::flush() {
    if (!mStream.flush().good())
        return tl::unexpected{std::error_code{errno, std::generic_category()}};

    return {};
}

tl::expected<void, std::error_code> zero::log::FileProvider::write(const Record &record) {
    const auto message = fmt::format("{}\n", record);

    if (!mStream.write(message.c_str(), static_cast<std::streamsize>(message.size())))
        return tl::unexpected{std::error_code{errno, std::generic_category()}};

    mPosition += message.size();
    return {};
}

zero::log::Logger::Logger() : mChannel{concurrent::channel<Record>(LOGGER_BUFFER_SIZE)} {
}

zero::log::Logger::~Logger() {
    mChannel.first.close();

    if (mThread.joinable())
        mThread.join();
}

void zero::log::Logger::consume() {
    auto &receiver = mChannel.second;

    while (true) {
        tl::expected<Record, std::error_code> record = receiver.tryReceive();

        if (!record) {
            if (record.error() == concurrent::TryReceiveError::DISCONNECTED)
                break;

            std::list<std::chrono::milliseconds> durations;

            {
                std::lock_guard guard{mMutex};

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
                record = receiver.receive(*ranges::min_element(durations));

            if (!record)
                continue;
        }

        std::lock_guard guard{mMutex};

        const auto now = std::chrono::system_clock::now();
        auto it = mConfigs.begin();

        while (it != mConfigs.end()) {
            if (record->level <= (std::max)(it->level, mMinLogLevel.value_or(Level::ERROR_LEVEL))) {
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

bool zero::log::Logger::enabled(const Level level) const {
    return mMaxLogLevel && *mMaxLogLevel >= level;
}

void zero::log::Logger::addProvider(
    const Level level,
    std::unique_ptr<IProvider> provider,
    const std::chrono::milliseconds interval
) {
    std::call_once(mOnceFlag, [=] {
        const auto getOption = [](const std::string &name) -> std::optional<int> {
            const auto value = env::get(name);

            if (!value)
                throw std::system_error(value.error());

            if (!*value)
                return std::nullopt;

            const auto option = strings::toNumber<int>(**value);

            if (!option)
                return std::nullopt;

            return *option;
        };

        if (const auto value = getOption("ZERO_LOG_LEVEL")) {
            if (const auto lv = static_cast<Level>(*value); lv < Level::ERROR_LEVEL || lv > Level::DEBUG_LEVEL)
                return;

            mMinLogLevel = static_cast<Level>(*value);
            mMaxLogLevel = mMinLogLevel;
        }

        if (const auto value = getOption("ZERO_LOG_TIMEOUT")) {
            if (*value <= 0) {
                mTimeout.reset();
                return;
            }

            mTimeout = std::chrono::milliseconds{*value};
        }

        mThread = std::thread{&Logger::consume, this};
    });

    if (!provider->init())
        return;

    std::lock_guard guard{mMutex};

    mMaxLogLevel = (std::max)(level, mMaxLogLevel.value_or(Level::ERROR_LEVEL));

    mConfigs.push_back({
        level,
        std::move(provider),
        interval,
        std::chrono::system_clock::now() + interval
    });
}

zero::log::Logger &zero::log::globalLogger() {
    static Logger instance;
    return instance;
}
