#include <zero/sh/shell.h>
#include <zero/filesystem/path.h>

#ifdef _WIN32
#include <tuple>
#include <Windows.h>
#include <algorithm>
#elif __linux__
#include <glob.h>
#include <wordexp.h>
#endif

#ifdef _WIN32
bool zero::sh::match(const std::string &pattern, std::list<std::string> &paths) {
    std::list<std::tuple<FILETIME, std::string>> files;
    std::string directory = zero::filesystem::path::getDirectoryName(pattern);

    WIN32_FIND_DATAA data = {};
    HANDLE handle = FindFirstFileA(pattern.c_str(), &data);

    if (handle == INVALID_HANDLE_VALUE)
        return false;

    do {
        files.emplace_back(data.ftCreationTime, zero::filesystem::path::join(directory, data.cFileName));
    } while (FindNextFileA(handle, &data));

    FindClose(handle);

    files.sort([](const auto &prev, const auto &next) {
        return CompareFileTime(&std::get<0>(prev), &std::get<0>(next)) == -1;
    });

    std::transform(files.begin(), files.end(), paths.begin(), [](const auto &file) {
        return std::get<1>(file);
    });

    return true;
}

#elif __linux__
bool zero::sh::expansion(const std::string &str, std::list<std::string> &words) {
    wordexp_t p = {};

    if (wordexp(str.c_str(), &p, WRDE_NOCMD) != 0) {
        wordfree(&p);
        return false;
    }

    for (int i = 0; i < p.we_wordc; i++) {
        words.emplace_back(p.we_wordv[i]);
    }

    wordfree(&p);

    return true;
}

bool zero::sh::match(const std::string &pattern, std::list<std::string> &paths) {
    glob_t g = {};

    if (glob(pattern.c_str(), 0, nullptr, &g) != 0) {
        globfree(&g);
        return false;
    }

    for (int i = 0; i < g.gl_pathc; i++) {
        paths.emplace_back(g.gl_pathv[i]);
    }

    globfree(&g);

    return true;
}

#endif
