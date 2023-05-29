#include <zero/log.h>
#include <zero/time/time.h>
#include <zero/filesystem/path.h>
#include <set>

#ifdef __linux__
#include <unistd.h>
#endif

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

        if (!strings::startsWith(entry.path().filename().string(), prefix))
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
