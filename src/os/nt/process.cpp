#include <zero/os/nt/process.h>
#include <zero/strings/strings.h>
#include <zero/defer.h>
#include <winternl.h>

static auto queryInformationProcess = (decltype(NtQueryInformationProcess) *) GetProcAddress(
        GetModuleHandleA("ntdll"),
        "NtQueryInformationProcess"
);

const char *zero::os::nt::process::ErrorCategory::name() const noexcept {
    return "zero::concurrent::channel";
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
    return image().transform([](const auto &path) {
        return path.filename().string();
    });
}

tl::expected<std::filesystem::path, std::error_code> zero::os::nt::process::Process::image() const {
    WCHAR buffer[MAX_PATH] = {};
    DWORD size = ARRAYSIZE(buffer);

    if (!QueryFullProcessImageNameW(mHandle, 0, buffer, &size))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return buffer;
}

tl::expected<std::vector<std::string>, std::error_code> zero::os::nt::process::Process::cmdline() const {
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

    void *ptr = nullptr;

    if (!ReadProcessMemory(
            mHandle,
            &info.PebBaseAddress->ProcessParameters,
            &ptr,
            sizeof(ptr),
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    RTL_USER_PROCESS_PARAMETERS parameters = {};

    if (!ReadProcessMemory(
            mHandle,
            ptr,
            &parameters,
            sizeof(RTL_USER_PROCESS_PARAMETERS),
            nullptr
    ))
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    if (!parameters.CommandLine.Buffer || parameters.CommandLine.Length == 0)
        return tl::unexpected(Error::UNEXPECTED_DATA);

    auto buffer = std::make_unique<WCHAR[]>(parameters.CommandLine.Length / sizeof(WCHAR) + 1);

    if (!ReadProcessMemory(
            mHandle,
            parameters.CommandLine.Buffer,
            buffer.get(),
            parameters.CommandLine.Length,
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
        auto arg = zero::strings::encode(args[i]);

        if (!arg) {
            result = tl::unexpected(arg.error());
            break;
        }

        result->push_back(std::move(*arg));
    }

    return result;
}

tl::expected<zero::os::nt::process::Process, std::error_code> zero::os::nt::process::openProcess(DWORD pid) {
    HANDLE handle = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            false,
            pid
    );

    if (!handle)
        return tl::unexpected(std::error_code((int) GetLastError(), std::system_category()));

    return Process{handle, pid};
}
