#include <zero/os/nt/process.h>
#include <zero/strings/strings.h>
#include <zero/try.h>
#include <zero/defer.h>
#include <winternl.h>
#include <psapi.h>
#include <ranges>

#ifdef _WIN64
constexpr auto CURRENT_DIRECTORY_OFFSET = 0x38;
constexpr auto ENVIRONMENT_OFFSET = 0x80;
constexpr auto ENVIRONMENT_SIZE_OFFSET = 0x03F0;
#elif _WIN32
constexpr auto CURRENT_DIRECTORY_OFFSET = 0x24;
constexpr auto ENVIRONMENT_OFFSET = 0x48;
constexpr auto ENVIRONMENT_SIZE_OFFSET = 0x0290;
#endif

static auto queryInformationProcess = (decltype(NtQueryInformationProcess) *) GetProcAddress(
        GetModuleHandleA("ntdll"),
        "NtQueryInformationProcess"
);

const char *zero::os::nt::process::ErrorCategory::name() const noexcept {
    return "zero::os::nt::process";
}

std::string zero::os::nt::process::ErrorCategory::message(int value) const {
    std::string msg;

    switch (value) {
        case API_NOT_AVAILABLE:
            msg = "api not available";
            break;

        case UNEXPECTED_DATA:
            msg = "unexpected data";
            break;

        default:
            msg = "unknown";
            break;
    }

    return msg;
}

std::error_condition zero::os::nt::process::ErrorCategory::default_error_condition(int value) const noexcept {
    if (value == API_NOT_AVAILABLE)
        return std::errc::function_not_supported;

    return error_category::default_error_condition(value);
}

const std::error_category &zero::os::nt::process::errorCategory() {
    static ErrorCategory instance;
    return instance;
}

std::error_code zero::os::nt::process::make_error_code(Error e) {
    return {static_cast<int>(e), errorCategory()};
}

zero::os::nt::process::Process::Process(HANDLE handle, DWORD pid) : mHandle(handle), mPID(pid) {

}

zero::os::nt::process::Process::Process(zero::os::nt::process::Process &&rhs) noexcept
        : mHandle(std::exchange(rhs.mHandle, nullptr)), mPID(std::exchange(rhs.mPID, 0)) {

}

zero::os::nt::process::Process::~Process() {
    if (!mHandle)
        return;

    CloseHandle(mHandle);
}

HANDLE zero::os::nt::process::Process::handle() const {
    return mHandle;
}

DWORD zero::os::nt::process::Process::pid() const {
    return mPID;
}

tl::expected<DWORD, std::error_code> zero::os::nt::process::Process::ppid() const {
    if (!queryInformationProcess)
        return tl::unexpected(Error::API_NOT_AVAILABLE);

    PROCESS_BASIC_INFORMATION info = {};

    if (!NT_SUCCESS(queryInformationProcess(
            mHandle,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
    )))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return (DWORD) (uintptr_t) info.Reserved3;
}

tl::expected<std::string, std::error_code> zero::os::nt::process::Process::name() const {
    return exe().transform([](const auto &path) {
        return path.filename().string();
    });
}

tl::expected<std::filesystem::path, std::error_code> zero::os::nt::process::Process::cwd() const {
    auto ptr = TRY(parameters());
    UNICODE_STRING str;

    if (!ReadProcessMemory(
            mHandle,
            (LPCVOID) (*ptr + CURRENT_DIRECTORY_OFFSET),
            &str,
            sizeof(UNICODE_STRING),
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    if (!str.Buffer || str.Length == 0)
        return tl::unexpected(Error::UNEXPECTED_DATA);

    auto buffer = std::make_unique<WCHAR[]>(str.Length / sizeof(WCHAR) + 1);

    if (!ReadProcessMemory(
            mHandle,
            str.Buffer,
            buffer.get(),
            str.Length,
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    std::error_code ec;
    auto path = std::filesystem::canonical(buffer.get());

    if (ec)
        return tl::unexpected(ec);

    return path;
}

tl::expected<std::filesystem::path, std::error_code> zero::os::nt::process::Process::exe() const {
    WCHAR buffer[MAX_PATH] = {};
    DWORD size = ARRAYSIZE(buffer);

    if (!QueryFullProcessImageNameW(mHandle, 0, buffer, &size))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return buffer;
}

tl::expected<std::vector<std::string>, std::error_code> zero::os::nt::process::Process::cmdline() const {
    auto ptr = TRY(parameters());
    UNICODE_STRING str;

    if (!ReadProcessMemory(
            mHandle,
            (LPCVOID) (*ptr + offsetof(RTL_USER_PROCESS_PARAMETERS, CommandLine)),
            &str,
            sizeof(UNICODE_STRING),
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    if (!str.Buffer || str.Length == 0)
        return tl::unexpected(Error::UNEXPECTED_DATA);

    auto buffer = std::make_unique<WCHAR[]>(str.Length / sizeof(WCHAR) + 1);

    if (!ReadProcessMemory(
            mHandle,
            str.Buffer,
            buffer.get(),
            str.Length,
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    int num = 0;
    LPWSTR *args = CommandLineToArgvW(buffer.get(), &num);

    if (!args)
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    DEFER(LocalFree(args));
    tl::expected<std::vector<std::string>, std::error_code> result;

    for (int i = 0; i < num; i++) {
        auto arg = strings::encode(args[i]);

        if (!arg) {
            result = tl::unexpected(arg.error());
            break;
        }

        result->push_back(std::move(*arg));
    }

    return result;
}

tl::expected<std::map<std::string, std::string>, std::error_code> zero::os::nt::process::Process::env() const {
    auto ptr = TRY(parameters());
    PVOID env;

    if (!ReadProcessMemory(
            mHandle,
            (LPCVOID) (*ptr + ENVIRONMENT_OFFSET),
            &env,
            sizeof(PVOID),
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    ULONG size;

    if (!ReadProcessMemory(
            mHandle,
            (LPCVOID) (*ptr + ENVIRONMENT_SIZE_OFFSET),
            &size,
            sizeof(ULONG),
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    auto buffer = std::make_unique<WCHAR[]>(size / sizeof(WCHAR));

    if (!ReadProcessMemory(
            mHandle,
            env,
            buffer.get(),
            size,
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    auto str = TRY(strings::encode({buffer.get(), size / sizeof(WCHAR)}));

    tl::expected<std::map<std::string, std::string>, std::error_code> result;

    for (const auto &token: strings::split(*str, {"\0", 1}) | std::views::drop(1)) {
        if (token.empty())
            break;

        size_t pos = token.find('=');

        if (pos == std::string::npos) {
            result = tl::unexpected<std::error_code>(Error::UNEXPECTED_DATA);
            break;
        }

        if (pos == 0)
            continue;

        result->operator[](token.substr(0, pos)) = token.substr(pos + 1);
    }

    return result;
}

tl::expected<zero::os::nt::process::CPUStat, std::error_code> zero::os::nt::process::Process::cpu() const {
    FILETIME create, exit, kernel, user;

    if (!GetProcessTimes(mHandle, &create, &exit, &kernel, &user))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return CPUStat{
            double(user.dwHighDateTime) * 429.4967296 + double(user.dwLowDateTime) * 1e-7,
            double(kernel.dwHighDateTime) * 429.4967296 + double(kernel.dwLowDateTime) * 1e-7,
    };
}

tl::expected<zero::os::nt::process::MemoryStat, std::error_code> zero::os::nt::process::Process::memory() const {
    PROCESS_MEMORY_COUNTERS counters;

    if (!GetProcessMemoryInfo(mHandle, &counters, sizeof(counters)))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return MemoryStat{
            counters.WorkingSetSize,
            counters.PagefileUsage
    };
}

tl::expected<zero::os::nt::process::IOStat, std::error_code> zero::os::nt::process::Process::io() const {
    IO_COUNTERS counters;

    if (!GetProcessIoCounters(mHandle, &counters))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return IOStat{
            counters.ReadOperationCount,
            counters.ReadTransferCount,
            counters.WriteOperationCount,
            counters.WriteTransferCount
    };
}

tl::expected<uintptr_t, std::error_code> zero::os::nt::process::Process::parameters() const {
    if (!queryInformationProcess)
        return tl::unexpected(Error::API_NOT_AVAILABLE);

    PROCESS_BASIC_INFORMATION info = {};

    if (!NT_SUCCESS(queryInformationProcess(
            mHandle,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
    )))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    uintptr_t ptr;

    if (!ReadProcessMemory(
            mHandle,
            &info.PebBaseAddress->ProcessParameters,
            &ptr,
            sizeof(ptr),
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return ptr;
}

tl::expected<zero::os::nt::process::Process, std::error_code> zero::os::nt::process::self() {
    return Process{GetCurrentProcess(), GetCurrentProcessId()};
}

tl::expected<zero::os::nt::process::Process, std::error_code> zero::os::nt::process::open(DWORD pid) {
    HANDLE handle = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            false,
            pid
    );

    if (!handle)
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return Process{handle, pid};
}

tl::expected<std::list<DWORD>, std::error_code> zero::os::nt::process::all() {
    size_t size = 4096;
    auto buffer = std::make_unique<DWORD[]>(size);

    tl::expected<std::list<DWORD>, std::error_code> result;

    while (true) {
        DWORD needed;

        if (!EnumProcesses(buffer.get(), size * sizeof(DWORD), &needed)) {
            result = tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));
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
