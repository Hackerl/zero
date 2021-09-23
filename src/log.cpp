#include <zero/log.h>
#include <unistd.h>
#include <zero/filesystem/path.h>
#include <zero/sh/shell.h>

void zero::CConsoleProvider::write(const std::string &message) {
    fwrite(message.c_str(), 1, message.length(), stderr);
}

zero::CFileProvider::CFileProvider(const char *name, unsigned long limit, const char *directory, unsigned long remain) {
    mName = name;
    mLimit = limit;
    mDirectory = directory;
    mRemain = remain;

    mFile.open(getLogPath());
}

std::string zero::CFileProvider::getLogPath() {
    std::string filename = strings::format(
            "%s.%d.%ld.log",
            mName.c_str(),
            getpid(),
            std::time(nullptr)
    );

    return filesystem::path::join(mDirectory, filename);
}

void zero::CFileProvider::clean() {
    std::list<std::string> logs;
    std::string pattern = strings::format("%s.%d.*.log", mName.c_str(), getpid());

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
}
