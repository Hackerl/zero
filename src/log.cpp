#include <zero/log.h>
#include <zero/filesystem/path.h>

#ifdef __linux__
#include <unistd.h>
#endif

void zero::ConsoleProvider::write(const std::string &message) {
    fwrite(message.c_str(), 1, message.length(), stderr);
}

zero::FileProvider::FileProvider(const std::string &name, const std::filesystem::path &directory, long limit, int remain) {
    mName = name;
    mLimit = limit;
    mRemain = remain;
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
    std::string pattern = strings::format("%s.%d.*.log", mName.c_str(), mPID);
    std::optional<std::list<std::filesystem::path>> logs = zero::filesystem::glob((mDirectory / pattern).string());

    if (!logs)
        return;

    size_t size = logs->size();

    if (size <= mRemain)
        return;

    std::list<std::filesystem::path> expired(logs->begin(), std::next(logs->begin(), (std::ptrdiff_t)(size - mRemain)));

    for (const auto &path: expired) {
        std::filesystem::remove(path);
    }
}

void zero::FileProvider::write(const std::string &message) {
    if (mFile.tellp() > mLimit) {
        mFile = std::ofstream(getLogPath());
        clean();
    }

    mFile.write(message.c_str(), (std::streamsize)message.length());
    mFile.flush();
}
