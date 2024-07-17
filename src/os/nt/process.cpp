#include <zero/os/nt/process.h>
#include <zero/strings/strings.h>
#include <zero/os/nt/error.h>
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

zero::os::nt::process::Process::Process(const HANDLE handle, const DWORD pid) : mPID(pid), mHandle(handle) {
}

zero::os::nt::process::Process::Process(Process &&rhs) noexcept
    : mPID(std::exchange(rhs.mPID, -1)), mHandle(std::exchange(rhs.mHandle, nullptr)) {
}

zero::os::nt::process::Process &zero::os::nt::process::Process::operator=(Process &&rhs) noexcept {
    mPID = std::exchange(rhs.mPID, -1);
    mHandle = std::exchange(rhs.mHandle, nullptr);
    return *this;
}

zero::os::nt::process::Process::~Process() {
    if (!mHandle)
        return;

    CloseHandle(mHandle);
}

std::expected<zero::os::nt::process::Process, std::error_code>
zero::os::nt::process::Process::from(const HANDLE handle) {
    const DWORD pid = GetProcessId(handle);

    if (pid == 0)
        return std::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    return Process{handle, pid};
}

std::expected<std::uintptr_t, std::error_code> zero::os::nt::process::Process::parameters() const {
    if (!queryInformationProcess)
        return std::unexpected(Error::API_NOT_AVAILABLE);

    PROCESS_BASIC_INFORMATION info = {};

    EXPECT(expected([&] {
        return NT_SUCCESS(queryInformationProcess(
            mHandle,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
        ));
    }));

    std::uintptr_t ptr;

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

HANDLE zero::os::nt::process::Process::handle() const {
    return mHandle;
}

DWORD zero::os::nt::process::Process::pid() const {
    return mPID;
}

std::expected<DWORD, std::error_code> zero::os::nt::process::Process::ppid() const {
    if (!queryInformationProcess)
        return std::unexpected(Error::API_NOT_AVAILABLE);

    PROCESS_BASIC_INFORMATION info = {};

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

std::expected<std::string, std::error_code> zero::os::nt::process::Process::name() const {
    return exe().transform([](const auto &path) {
        return path.filename().string();
    });
}

std::expected<std::filesystem::path, std::error_code> zero::os::nt::process::Process::cwd() const {
    const auto ptr = parameters();
    EXPECT(ptr);

    UNICODE_STRING str;

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            reinterpret_cast<LPCVOID>(*ptr + CURRENT_DIRECTORY_OFFSET),
            &str,
            sizeof(UNICODE_STRING),
            nullptr
        );
    }));

    if (!str.Buffer || str.Length == 0)
        return std::unexpected(Error::UNEXPECTED_DATA);

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

    std::error_code ec;
    auto path = std::filesystem::canonical(buffer.get(), ec);

    if (ec)
        return std::unexpected(ec);

    return path;
}

std::expected<std::filesystem::path, std::error_code> zero::os::nt::process::Process::exe() const {
    std::array<WCHAR, MAX_PATH> buffer = {};
    DWORD size = buffer.size();

    EXPECT(expected([&] {
        return QueryFullProcessImageNameW(mHandle, 0, buffer.data(), &size);
    }));

    return buffer.data();
}

std::expected<std::vector<std::string>, std::error_code> zero::os::nt::process::Process::cmdline() const {
    const auto ptr = parameters();
    EXPECT(ptr);

    UNICODE_STRING str;

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            reinterpret_cast<LPCVOID>(*ptr + offsetof(RTL_USER_PROCESS_PARAMETERS, CommandLine)),
            &str,
            sizeof(UNICODE_STRING),
            nullptr
        );
    }));

    if (!str.Buffer || str.Length == 0)
        return std::unexpected(Error::UNEXPECTED_DATA);

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

    int num = 0;
    LPWSTR *args = CommandLineToArgvW(buffer.get(), &num);

    if (!args)
        return std::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    DEFER(LocalFree(args));
    std::expected<std::vector<std::string>, std::error_code> result;

    for (int i = 0; i < num; ++i) {
        auto arg = strings::encode(args[i]);

        if (!arg) {
            result = std::unexpected(arg.error());
            break;
        }

        result->push_back(*std::move(arg));
    }

    return result;
}

std::expected<std::map<std::string, std::string>, std::error_code> zero::os::nt::process::Process::envs() const {
    const auto ptr = parameters();
    EXPECT(ptr);

    PVOID env;

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            reinterpret_cast<LPCVOID>(*ptr + ENVIRONMENT_OFFSET),
            &env,
            sizeof(PVOID),
            nullptr
        );
    }));

    ULONG size;

    EXPECT(expected([&] {
        return ReadProcessMemory(
            mHandle,
            reinterpret_cast<LPCVOID>(*ptr + ENVIRONMENT_SIZE_OFFSET),
            &size,
            sizeof(ULONG),
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

    std::expected<std::map<std::string, std::string>, std::error_code> result;

    for (const auto &token: strings::split(*str, {"\0", 1})) {
        if (token.empty())
            break;

        const size_t pos = token.find('=');

        if (pos == std::string::npos) {
            result = std::unexpected<std::error_code>(Error::UNEXPECTED_DATA);
            break;
        }

        if (pos == 0)
            continue;

        result->emplace(token.substr(0, pos), token.substr(pos + 1));
    }

    return result;
}

std::expected<zero::os::nt::process::CPUTime, std::error_code> zero::os::nt::process::Process::cpu() const {
    FILETIME create, exit, kernel, user;

    EXPECT(expected([&] {
        return GetProcessTimes(mHandle, &create, &exit, &kernel, &user);
    }));

    return CPUTime{
        static_cast<double>(user.dwHighDateTime) * 429.4967296 + static_cast<double>(user.dwLowDateTime) * 1e-7,
        static_cast<double>(kernel.dwHighDateTime) * 429.4967296 + static_cast<double>(kernel.dwLowDateTime) * 1e-7,
    };
}

std::expected<zero::os::nt::process::MemoryStat, std::error_code> zero::os::nt::process::Process::memory() const {
    PROCESS_MEMORY_COUNTERS counters;

    EXPECT(expected([&] {
        return GetProcessMemoryInfo(mHandle, &counters, sizeof(counters));
    }));

    return MemoryStat{
        counters.WorkingSetSize,
        counters.PagefileUsage
    };
}

std::expected<zero::os::nt::process::IOStat, std::error_code> zero::os::nt::process::Process::io() const {
    IO_COUNTERS counters;

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

std::expected<DWORD, std::error_code> zero::os::nt::process::Process::exitCode() const {
    DWORD code;

    EXPECT(expected([&] {
        return GetExitCodeProcess(mHandle, &code);
    }));

    if (code == STILL_ACTIVE)
        return std::unexpected(Error::PROCESS_STILL_ACTIVE);

    return code;
}

std::expected<void, std::error_code>
zero::os::nt::process::Process::wait(const std::optional<std::chrono::milliseconds> timeout) const {
    if (const DWORD result = WaitForSingleObject(mHandle, timeout ? static_cast<DWORD>(timeout->count()) : INFINITE);
        result != WAIT_OBJECT_0) {
        if (result == WAIT_TIMEOUT)
            return std::unexpected(Error::WAIT_PROCESS_TIMEOUT);

        return std::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));
    }

    return {};
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::nt::process::Process::terminate(const DWORD code) {
    return expected([&] {
        return TerminateProcess(mHandle, code);
    });
}

std::expected<zero::os::nt::process::Process, std::error_code> zero::os::nt::process::self() {
    return Process{GetCurrentProcess(), GetCurrentProcessId()};
}

std::expected<zero::os::nt::process::Process, std::error_code> zero::os::nt::process::open(const DWORD pid) {
    const auto handle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        false,
        pid
    );

    if (!handle)
        return std::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    return Process{handle, pid};
}

std::expected<std::list<DWORD>, std::error_code> zero::os::nt::process::all() {
    std::size_t size = 4096;
    auto buffer = std::make_unique<DWORD[]>(size);

    std::expected<std::list<DWORD>, std::error_code> result;

    while (true) {
        DWORD needed;

        if (const auto res = expected([&] {
            return EnumProcesses(buffer.get(), size * sizeof(DWORD), &needed);
        }); !res) {
            result = std::unexpected(res.error());
            break;
        }

        if (needed / sizeof(DWORD) == size) {
            size *= 2;
            buffer = std::make_unique<DWORD[]>(size);
        }

        result->assign(buffer.get(), buffer.get() + needed / sizeof(DWORD));
        break;
    }

    return result;
}
