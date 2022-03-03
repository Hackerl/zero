#include <zero/log.h>
#include <zero/sh/shell.h>

#ifdef __linux__
#include <unistd.h>
#endif

void zero::CConsoleProvider::write(const std::string &message) {
    fwrite(message.c_str(), 1, message.length(), stderr);
}

zero::CFileProvider::CFileProvider(const char *name, const std::string &directory, unsigned long limit, unsigned long remain) {
    mName = name;
    mLimit = limit;
    mRemain = remain;
    mDirectory = !directory.empty() ? directory : zero::filesystem::path::getTemporaryDirectory();

#ifndef _WIN32
    mPID = getpid();
#else
    mPID = _getpid();
#endif

    mFile.open(getLogPath());
}

std::string zero::CFileProvider::getLogPath() {
    std::string filename = strings::format(
            "%s.%d.%ld.log",
            mName.c_str(),
            mPID,
            std::time(nullptr)
    );

    return filesystem::path::join(mDirectory, filename);
}

void zero::CFileProvider::clean() {
    std::list<std::string> logs;
    std::string pattern = strings::format("%s.%d.*.log", mName.c_str(), mPID);

    if (!sh::match(filesystem::path::join(mDirectory, pattern), logs))
        return;

    unsigned long size = logs.size();

    if (size <= mRemain)
        return;

    std::list<std::string> expired(logs.begin(), std::next(logs.begin(), (std::ptrdiff_t)(size - mRemain)));

    for (const auto &path: expired) {
        remove(path.c_str());
    }
}

void zero::CFileProvider::write(const std::string &message) {
    if (mFile.tellp() > mLimit) {
        mFile = std::ofstream(getLogPath());
        clean();
    }

    mFile.write(message.c_str(), (std::streamsize)message.length());
    mFile.flush();
}
