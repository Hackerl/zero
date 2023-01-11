#include <zero/log.h>
#include <zero/filesystem/path.h>
#include <set>

#ifdef __linux__
#include <unistd.h>
#endif

void zero::ConsoleProvider::write(std::string_view message) {
    fwrite(message.data(), 1, message.length(), stderr);
}

zero::FileProvider::FileProvider(
        const char *name,
        const std::filesystem::path &directory,
        long limit,
        int remain
) : mName(name), mLimit(limit), mRemain(remain) {
    mDirectory = std::filesystem::is_directory(directory) ? directory : std::filesystem::temp_directory_path();

#ifndef _WIN32
    mPID = getpid();
#else
    mPID = _getpid();
#endif

    mFile.open(getLogPath());
}

std::filesystem::path zero::FileProvider::getLogPath() {
    std::string filename = strings::format(
            "%s.%d.%ld.log",
            mName.c_str(),
            mPID,
            std::time(nullptr)
    );

    return mDirectory / filename;
}

void zero::FileProvider::clean() {
    std::string prefix = strings::format("%s.%d", mName.c_str(), mPID);
    std::set<std::filesystem::path> logs;

    for (const auto &entry: std::filesystem::directory_iterator(mDirectory)) {
        if (!entry.is_regular_file())
            continue;

        if (!strings::startsWith(entry.path().filename().string(), prefix))
            continue;

        logs.insert(entry);
    }

    size_t size = logs.size();

    if (size <= mRemain)
        return;

    for (auto it = logs.begin(); it != std::prev(logs.end(), mRemain); it++) {
        std::error_code ec;
        std::filesystem::remove(*it, ec);
    }
}

void zero::FileProvider::write(std::string_view message) {
    if (mFile.tellp() > mLimit) {
        mFile = std::ofstream(getLogPath());
        clean();
    }

    mFile.write(message.data(), (std::streamsize) message.length());
    mFile.flush();
}
