#include <zero/log.h>
#include <zero/filesystem/path.h>
#include <set>

#ifdef __linux__
#include <unistd.h>
#endif

bool zero::ConsoleProvider::init() {
    return stderr != nullptr;
}

bool zero::ConsoleProvider::rotate() {
    return true;
}

zero::LogResult zero::ConsoleProvider::write(std::string_view message) {
    size_t length = message.length();

    if (fwrite(message.data(), 1, length, stderr) != length)
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

zero::LogResult zero::FileProvider::write(std::string_view message) {
    mStream << message << std::flush;

    if (mStream.bad())
        return FAILED;

    mPosition += message.length();

    if (mPosition >= mLimit) {
        mPosition = 0;
        mStream.close();
        mStream.clear();
        return ROTATED;
    }

    return SUCCEEDED;
}
