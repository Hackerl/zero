#include <zero/os/windows/process.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/std.h>
#include <zero/os/windows/error.h>
#include <zero/expect.h>
#include <zero/defer.h>
#include <winternl.h>
#include <psapi.h>

#ifdef _WIN64
constexpr auto CURRENT_DIRECTORY_OFFSET = 0x38;
constexpr auto ENVIRONMENT_OFFSET = 0x80;
constexpr auto ENVIRONMENT_SIZE_OFFSET = 0x03F0;
#elif defined(_WIN32)
constexpr auto CURRENT_DIRECTORY_OFFSET = 0x24;
constexpr auto ENVIRONMENT_OFFSET = 0x48;
constexpr auto ENVIRONMENT_SIZE_OFFSET = 0x0290;
#endif

static const auto queryInformationProcess = reinterpret_cast<decltype(&NtQueryInformationProcess)>(
    GetProcAddress(GetModuleHandleA("ntdll"), "NtQueryInformationProcess")
);

zero::os::windows::process::Process::Process(const HANDLE handle, const DWORD pid) : mPID{pid}, mHandle{handle} {
}

zero::os::windows::process::Process::Process(Process &&rhs) noexcept
    : mPID{std::exchange(rhs.mPID, -1)}, mHandle{std::exchange(rhs.mHandle, nullptr)} {
}

zero::os::windows::process::Process &zero::os::windows::process::Process::operator=(Process &&rhs) noexcept {
    mPID = std::exchange(rhs.mPID, -1);
    mHandle = std::exchange(rhs.mHandle, nullptr);
    return *this;
}

zero::os::windows::process::Process::~Process() {
    if (!mHandle)
        return;

    CloseHandle(mHandle);
}

tl::expected<zero::os::windows::process::Process, std::error_code>
zero::os::windows::process::Process::from(const HANDLE handle) {
    const auto pid = GetProcessId(handle);

    if (pid == 0)
        return tl::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    return Process{handle, pid};
}

tl::expected<std::uintptr_t, std::error_code> zero::os::windows::process::Process::parameters() const {
    if (!queryInformationProcess)
        return tl::unexpected{Error::API_NOT_AVAILABLE};

    PROCESS_BASIC_INFORMATION info{};

    EXPECT(expected([&] {
        return NT_SUCCESS(queryInformationProcess(
            mHandle,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
        ));
    }));

    std::uintptr_t ptr{};

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            &info.PebBaseAddress->ProcessParameters,
            &ptr,
            sizeof(ptr),
            nullptr
        );
    }));

    return ptr;
}

HANDLE zero::os::windows::process::Process::handle() const {
    return mHandle;
}

DWORD zero::os::windows::process::Process::pid() const {
    return mPID;
}

tl::expected<DWORD, std::error_code> zero::os::windows::process::Process::ppid() const {
    if (!queryInformationProcess)
        return tl::unexpected{Error::API_NOT_AVAILABLE};

    PROCESS_BASIC_INFORMATION info{};

    EXPECT(expected([&] {
        return NT_SUCCESS(queryInformationProcess(
            mHandle,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
        ));
    }));

    return static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(info.Reserved3));
}

tl::expected<std::string, std::error_code> zero::os::windows::process::Process::name() const {
    return exe().transform([](const auto &path) {
        return path.filename().string();
    });
}

tl::expected<std::filesystem::path, std::error_code> zero::os::windows::process::Process::cwd() const {
    const auto ptr = parameters();
    EXPECT(ptr);

    UNICODE_STRING str{};

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            reinterpret_cast<LPCVOID>(*ptr + CURRENT_DIRECTORY_OFFSET),
            &str,
            sizeof(str),
            nullptr
        );
    }));

    if (!str.Buffer || str.Length == 0)
        return tl::unexpected{Error::UNEXPECTED_DATA};

    const auto buffer = std::make_unique<WCHAR[]>(str.Length / sizeof(WCHAR) + 1);

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            str.Buffer,
            buffer.get(),
            str.Length,
            nullptr
        );
    }));

    return filesystem::canonical(buffer.get());
}

tl::expected<std::filesystem::path, std::error_code> zero::os::windows::process::Process::exe() const {
    std::array<WCHAR, MAX_PATH> buffer{};
    auto size = static_cast<DWORD>(buffer.size());

    EXPECT(expected([&] {
        return QueryFullProcessImageNameW(mHandle, 0, buffer.data(), &size);
    }));

    return buffer.data();
}

tl::expected<std::vector<std::string>, std::error_code> zero::os::windows::process::Process::cmdline() const {
    const auto ptr = parameters();
    EXPECT(ptr);

    UNICODE_STRING str{};

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            reinterpret_cast<LPCVOID>(*ptr + offsetof(RTL_USER_PROCESS_PARAMETERS, CommandLine)),
            &str,
            sizeof(str),
            nullptr
        );
    }));

    if (!str.Buffer || str.Length == 0)
        return tl::unexpected{Error::UNEXPECTED_DATA};

    const auto buffer = std::make_unique<WCHAR[]>(str.Length / sizeof(WCHAR) + 1);

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            str.Buffer,
            buffer.get(),
            str.Length,
            nullptr
        );
    }));

    int num{0};
    const auto args = CommandLineToArgvW(buffer.get(), &num);

    if (!args)
        return tl::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    DEFER(LocalFree(args));
    std::vector<std::string> cmdline;

    for (int i{0}; i < num; ++i) {
        auto arg = strings::encode(args[i]);
        EXPECT(arg);
        cmdline.push_back(*std::move(arg));
    }

    return cmdline;
}

tl::expected<std::map<std::string, std::string>, std::error_code> zero::os::windows::process::Process::envs() const {
    const auto ptr = parameters();
    EXPECT(ptr);

    PVOID env{};

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            reinterpret_cast<LPCVOID>(*ptr + ENVIRONMENT_OFFSET),
            &env,
            sizeof(env),
            nullptr
        );
    }));

    ULONG size{};

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            reinterpret_cast<LPCVOID>(*ptr + ENVIRONMENT_SIZE_OFFSET),
            &size,
            sizeof(size),
            nullptr
        );
    }));

    const auto buffer = std::make_unique<WCHAR[]>(size / sizeof(WCHAR));

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            env,
            buffer.get(),
            size,
            nullptr
        );
    }));

    const auto str = strings::encode(std::wstring_view(buffer.get(), size / sizeof(WCHAR)));
    EXPECT(str);

    std::map<std::string, std::string> envs;

    for (const auto &token: strings::split(*str, {"\0", 1})) {
        if (token.empty())
            break;

        const auto pos = token.find('=');

        if (pos == std::string::npos)
            return tl::unexpected{Error::UNEXPECTED_DATA};

        if (pos == 0)
            continue;

        envs.emplace(token.substr(0, pos), token.substr(pos + 1));
    }

    return envs;
}

tl::expected<std::chrono::system_clock::time_point, std::error_code>
zero::os::windows::process::Process::startTime() const {
    FILETIME create{}, exit{}, kernel{}, user{};

    EXPECT(expected([&] {
        return GetProcessTimes(mHandle, &create, &exit, &kernel, &user);
    }));

    return std::chrono::system_clock::from_time_t(
        (static_cast<std::int64_t>(create.dwHighDateTime) << 32 | create.dwLowDateTime) / 10000000 - 11644473600
    );
}

tl::expected<zero::os::windows::process::CPUTime, std::error_code> zero::os::windows::process::Process::cpu() const {
    FILETIME create{}, exit{}, kernel{}, user{};

    EXPECT(expected([&] {
        return GetProcessTimes(mHandle, &create, &exit, &kernel, &user);
    }));

    return CPUTime{
        static_cast<double>(user.dwHighDateTime) * 429.4967296 + static_cast<double>(user.dwLowDateTime) * 1e-7,
        static_cast<double>(kernel.dwHighDateTime) * 429.4967296 + static_cast<double>(kernel.dwLowDateTime) * 1e-7,
    };
}

tl::expected<zero::os::windows::process::MemoryStat, std::error_code>
zero::os::windows::process::Process::memory() const {
    PROCESS_MEMORY_COUNTERS counters{};

    EXPECT(expected([&] {
        return GetProcessMemoryInfo(mHandle, &counters, sizeof(counters));
    }));

    return MemoryStat{
        counters.WorkingSetSize,
        counters.PagefileUsage
    };
}

tl::expected<zero::os::windows::process::IOStat, std::error_code> zero::os::windows::process::Process::io() const {
    IO_COUNTERS counters{};

    EXPECT(expected([&] {
        return GetProcessIoCounters(mHandle, &counters);
    }));

    return IOStat{
        counters.ReadOperationCount,
        counters.ReadTransferCount,
        counters.WriteOperationCount,
        counters.WriteTransferCount
    };
}

tl::expected<DWORD, std::error_code> zero::os::windows::process::Process::exitCode() const {
    DWORD code{};

    EXPECT(expected([&] {
        return GetExitCodeProcess(mHandle, &code);
    }));

    if (code == STILL_ACTIVE)
        return tl::unexpected{Error::PROCESS_STILL_ACTIVE};

    return code;
}

tl::expected<void, std::error_code>
zero::os::windows::process::Process::wait(const std::optional<std::chrono::milliseconds> timeout) const {
    if (const auto result = WaitForSingleObject(mHandle, timeout ? static_cast<DWORD>(timeout->count()) : INFINITE);
        result != WAIT_OBJECT_0) {
        if (result == WAIT_TIMEOUT)
            return tl::unexpected{Error::WAIT_PROCESS_TIMEOUT};

        return tl::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};
    }

    return {};
}

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<void, std::error_code> zero::os::windows::process::Process::terminate(const DWORD code) {
    return expected([&] {
        return TerminateProcess(mHandle, code);
    });
}

tl::expected<zero::os::windows::process::Process, std::error_code> zero::os::windows::process::self() {
    return Process{GetCurrentProcess(), GetCurrentProcessId()};
}

tl::expected<zero::os::windows::process::Process, std::error_code> zero::os::windows::process::open(const DWORD pid) {
    const auto handle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        false,
        pid
    );

    if (!handle)
        return tl::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    return Process{handle, pid};
}

tl::expected<std::list<DWORD>, std::error_code> zero::os::windows::process::all() {
    std::size_t size{4096};
    auto buffer = std::make_unique<DWORD[]>(size);

    while (true) {
        DWORD needed{};

        EXPECT(expected([&] {
            return EnumProcesses(buffer.get(), size * sizeof(DWORD), &needed);
        }));

        if (needed / sizeof(DWORD) < size)
            return std::list<DWORD>{buffer.get(), buffer.get() + needed / sizeof(DWORD)};

        size *= 2;
        buffer = std::make_unique<DWORD[]>(size);
    }
}

DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::windows::process::Process::Error)
