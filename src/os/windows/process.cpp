#include <zero/os/windows/process.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/fs.h>
#include <zero/os/windows/error.h>
#include <zero/expect.h>
#include <zero/defer.h>
#include <winternl.h>
#include <psapi.h>
#include <fmt/xchar.h>

#ifdef _WIN64
constexpr auto CurrentDirectoryOffset = 0x38;
constexpr auto EnvironmentOffset = 0x80;
constexpr auto EnvironmentSizeOffset = 0x03F0;
#elif defined(_WIN32)
constexpr auto CurrentDirectoryOffset = 0x24;
constexpr auto EnvironmentOffset = 0x48;
constexpr auto EnvironmentSizeOffset = 0x0290;
#endif

namespace {
    const auto queryInformationProcess = reinterpret_cast<decltype(&NtQueryInformationProcess)>(
        GetProcAddress(GetModuleHandleA("ntdll"), "NtQueryInformationProcess")
    );
}

zero::os::windows::process::Process::Process(Resource resource, const DWORD pid)
    : mResource{std::move(resource)}, mPID{pid} {
}

std::expected<zero::os::windows::process::Process, std::error_code>
zero::os::windows::process::Process::from(const HANDLE handle) {
    const auto pid = GetProcessId(handle);

    if (pid == 0)
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    return Process{Resource{handle}, pid};
}

std::expected<std::uintptr_t, std::error_code> zero::os::windows::process::Process::parameters() const {
    if (!queryInformationProcess)
        return std::unexpected{Error::APINotAvailable};

    PROCESS_BASIC_INFORMATION info{};

    Z_EXPECT(expected([&] {
        return NT_SUCCESS(queryInformationProcess(
            *mResource,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
        ));
    }));

    std::uintptr_t ptr{};

    Z_EXPECT(expected([&] {
        return ReadProcessMemory(
            *mResource,
            &info.PebBaseAddress->ProcessParameters,
            &ptr,
            sizeof(ptr),
            nullptr
        );
    }));

    return ptr;
}

const zero::os::Resource &zero::os::windows::process::Process::handle() const {
    return mResource;
}

DWORD zero::os::windows::process::Process::pid() const {
    return mPID;
}

std::expected<DWORD, std::error_code> zero::os::windows::process::Process::ppid() const {
    if (!queryInformationProcess)
        return std::unexpected{Error::APINotAvailable};

    PROCESS_BASIC_INFORMATION info{};

    Z_EXPECT(expected([&] {
        return NT_SUCCESS(queryInformationProcess(
            *mResource,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
        ));
    }));

#ifdef __MINGW32__
    return static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(info.InheritedFromUniqueProcessId));
#else
    return static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(info.Reserved3));
#endif
}

std::expected<std::string, std::error_code> zero::os::windows::process::Process::name() const {
    return exe().transform([](const auto &path) {
        return filesystem::stringify(path.filename());
    });
}

std::expected<std::filesystem::path, std::error_code> zero::os::windows::process::Process::cwd() const {
    const auto ptr = parameters();
    Z_EXPECT(ptr);

    UNICODE_STRING str{};

    Z_EXPECT(expected([&] {
        return ReadProcessMemory(
            *mResource,
            reinterpret_cast<LPCVOID>(*ptr + CurrentDirectoryOffset),
            &str,
            sizeof(str),
            nullptr
        );
    }));

    if (!str.Buffer || str.Length == 0)
        return std::unexpected{Error::UnexpectedData};

    const auto buffer = std::make_unique<WCHAR[]>(str.Length / sizeof(WCHAR) + 1);

    Z_EXPECT(expected([&] {
        return ReadProcessMemory(
            *mResource,
            str.Buffer,
            buffer.get(),
            str.Length,
            nullptr
        );
    }));

    return filesystem::canonical(buffer.get());
}

std::expected<std::filesystem::path, std::error_code> zero::os::windows::process::Process::exe() const {
    std::array<WCHAR, MAX_PATH> buffer{};
    auto size = static_cast<DWORD>(buffer.size());

    Z_EXPECT(expected([&] {
        return QueryFullProcessImageNameW(*mResource, 0, buffer.data(), &size);
    }));

    return buffer.data();
}

std::expected<std::vector<std::string>, std::error_code> zero::os::windows::process::Process::cmdline() const {
    const auto ptr = parameters();
    Z_EXPECT(ptr);

    UNICODE_STRING str{};

    Z_EXPECT(expected([&] {
        return ReadProcessMemory(
            *mResource,
            reinterpret_cast<LPCVOID>(*ptr + offsetof(RTL_USER_PROCESS_PARAMETERS, CommandLine)),
            &str,
            sizeof(str),
            nullptr
        );
    }));

    if (!str.Buffer || str.Length == 0)
        return std::unexpected{Error::UnexpectedData};

    const auto buffer = std::make_unique<WCHAR[]>(str.Length / sizeof(WCHAR) + 1);

    Z_EXPECT(expected([&] {
        return ReadProcessMemory(
            *mResource,
            str.Buffer,
            buffer.get(),
            str.Length,
            nullptr
        );
    }));

    int num{0};
    const auto args = CommandLineToArgvW(buffer.get(), &num);

    if (!args)
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    Z_DEFER(
        if (LocalFree(args))
            throw error::SystemError{static_cast<int>(GetLastError()), std::system_category()};
    );

    std::vector<std::string> cmdline;

    for (int i{0}; i < num; ++i) {
        auto arg = strings::encode(args[i]);
        Z_EXPECT(arg);
        cmdline.push_back(*std::move(arg));
    }

    return cmdline;
}

std::expected<std::map<std::string, std::string>, std::error_code> zero::os::windows::process::Process::envs() const {
    const auto ptr = parameters();
    Z_EXPECT(ptr);

    PVOID env{};

    Z_EXPECT(expected([&] {
        return ReadProcessMemory(
            *mResource,
            reinterpret_cast<LPCVOID>(*ptr + EnvironmentOffset),
            &env,
            sizeof(env),
            nullptr
        );
    }));

    ULONG size{};

    Z_EXPECT(expected([&] {
        return ReadProcessMemory(
            *mResource,
            reinterpret_cast<LPCVOID>(*ptr + EnvironmentSizeOffset),
            &size,
            sizeof(size),
            nullptr
        );
    }));

    const auto buffer = std::make_unique<WCHAR[]>(size / sizeof(WCHAR));

    Z_EXPECT(expected([&] {
        return ReadProcessMemory(
            *mResource,
            env,
            buffer.get(),
            size,
            nullptr
        );
    }));

    const auto str = strings::encode(std::wstring_view(buffer.get(), size / sizeof(WCHAR)));
    Z_EXPECT(str);

    std::map<std::string, std::string> envs;

    for (const auto &token: strings::split(*str, {"\0", 1})) {
        if (token.empty())
            break;

        const auto pos = token.find('=');

        if (pos == std::string::npos)
            return std::unexpected{Error::UnexpectedData};

        if (pos == 0)
            continue;

        envs.emplace(token.substr(0, pos), token.substr(pos + 1));
    }

    return envs;
}

std::expected<std::chrono::system_clock::time_point, std::error_code>
zero::os::windows::process::Process::startTime() const {
    FILETIME create{}, exit{}, kernel{}, user{};

    Z_EXPECT(expected([&] {
        return GetProcessTimes(*mResource, &create, &exit, &kernel, &user);
    }));

    return std::chrono::system_clock::from_time_t(
        (static_cast<std::int64_t>(create.dwHighDateTime) << 32 | create.dwLowDateTime) / 10000000 - 11644473600
    );
}

std::expected<zero::os::windows::process::CPUTime, std::error_code> zero::os::windows::process::Process::cpu() const {
    FILETIME create{}, exit{}, kernel{}, user{};

    Z_EXPECT(expected([&] {
        return GetProcessTimes(*mResource, &create, &exit, &kernel, &user);
    }));

    return CPUTime{
        static_cast<double>(user.dwHighDateTime) * 429.4967296 + static_cast<double>(user.dwLowDateTime) * 1e-7,
        static_cast<double>(kernel.dwHighDateTime) * 429.4967296 + static_cast<double>(kernel.dwLowDateTime) * 1e-7,
    };
}

std::expected<zero::os::windows::process::MemoryStat, std::error_code>
zero::os::windows::process::Process::memory() const {
    PROCESS_MEMORY_COUNTERS counters{};

    Z_EXPECT(expected([&] {
        return GetProcessMemoryInfo(*mResource, &counters, sizeof(counters));
    }));

    return MemoryStat{
        counters.WorkingSetSize,
        counters.PagefileUsage
    };
}

std::expected<zero::os::windows::process::IOStat, std::error_code> zero::os::windows::process::Process::io() const {
    IO_COUNTERS counters{};

    Z_EXPECT(expected([&] {
        return GetProcessIoCounters(*mResource, &counters);
    }));

    return IOStat{
        counters.ReadOperationCount,
        counters.ReadTransferCount,
        counters.WriteOperationCount,
        counters.WriteTransferCount
    };
}

std::expected<DWORD, std::error_code> zero::os::windows::process::Process::exitCode() const {
    DWORD code{};

    Z_EXPECT(expected([&] {
        return GetExitCodeProcess(*mResource, &code);
    }));

    if (code == STILL_ACTIVE)
        return std::unexpected{Error::ProcessStillActive};

    return code;
}

std::expected<void, std::error_code>
zero::os::windows::process::Process::wait(const std::optional<std::chrono::milliseconds> timeout) const {
    if (const auto result = WaitForSingleObject(*mResource, timeout ? static_cast<DWORD>(timeout->count()) : INFINITE);
        result != WAIT_OBJECT_0) {
        if (result == WAIT_TIMEOUT)
            return std::unexpected{Error::WaitProcessTimeout};

        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};
    }

    return {};
}

std::expected<std::string, std::error_code> zero::os::windows::process::Process::user() const {
    HANDLE token{};

    Z_EXPECT(expected([&] {
        return OpenProcessToken(*mResource, TOKEN_QUERY, &token);
    }));

    Z_DEFER(error::guard(expected([&] {
        return CloseHandle(token);
    })));

    DWORD size{1024};
    auto buffer = std::make_unique<std::byte[]>(size);

    while (true) {
        const auto result = expected([&] {
            return GetTokenInformation(token, TokenUser, buffer.get(), size, &size);
        });

        if (result)
            break;

        if (const auto &error = result.error();
            error != std::error_code{ERROR_INSUFFICIENT_BUFFER, std::system_category()})
            return std::unexpected{error};

        buffer = std::make_unique<std::byte[]>(size);
    }

    const auto sid = reinterpret_cast<const TOKEN_USER *>(buffer.get())->User.Sid;

    DWORD nameSize{1024};
    DWORD domainSize{1024};

    auto name = std::make_unique<WCHAR[]>(nameSize);
    auto domain = std::make_unique<WCHAR[]>(domainSize);

    while (true) {
        const auto result = expected([&] {
            SID_NAME_USE type{};
            return LookupAccountSidW(nullptr, sid, name.get(), &nameSize, domain.get(), &domainSize, &type);
        });

        if (result)
            break;

        if (const auto &error = result.error();
            error != std::error_code{ERROR_INSUFFICIENT_BUFFER, std::system_category()})
            return std::unexpected{error};

        name = std::make_unique<WCHAR[]>(nameSize);
        domain = std::make_unique<WCHAR[]>(domainSize);
    }

    return strings::encode(fmt::format(L"{}\\{}", domain.get(), name.get()));
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::windows::process::Process::terminate(const DWORD code) {
    return expected([&] {
        return TerminateProcess(*mResource, code);
    });
}

std::expected<zero::os::windows::process::Process, std::error_code> zero::os::windows::process::self() {
    return Process{Resource{GetCurrentProcess()}, GetCurrentProcessId()};
}

std::expected<zero::os::windows::process::Process, std::error_code> zero::os::windows::process::open(const DWORD pid) {
    const auto handle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE | PROCESS_TERMINATE,
        false,
        pid
    );

    if (!handle)
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    return Process{Resource{handle}, pid};
}

std::expected<std::list<DWORD>, std::error_code> zero::os::windows::process::all() {
    std::size_t size{4096};
    auto buffer = std::make_unique<DWORD[]>(size);

    while (true) {
        DWORD needed{};

        Z_EXPECT(expected([&] {
            return EnumProcesses(buffer.get(), size * sizeof(DWORD), &needed);
        }));

        if (needed / sizeof(DWORD) < size)
            return std::list<DWORD>{buffer.get(), buffer.get() + needed / sizeof(DWORD)};

        size *= 2;
        buffer = std::make_unique<DWORD[]>(size);
    }
}

Z_DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::windows::process::Process::Error)
