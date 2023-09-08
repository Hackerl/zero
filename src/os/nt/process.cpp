#include <zero/os/nt/process.h>
#include <zero/strings/strings.h>
#include <algorithm>
#include <winternl.h>

static auto queryInformationProcess = (decltype(NtQueryInformationProcess) *) GetProcAddress(
        GetModuleHandleA("ntdll"),
        "NtQueryInformationProcess"
);

zero::os::nt::process::Process::Process(HANDLE handle, DWORD pid) : mHandle(handle), mPID(pid) {

}

zero::os::nt::process::Process::Process(zero::os::nt::process::Process &&rhs) noexcept: mHandle(nullptr), mPID(0) {
    std::swap(mHandle, rhs.mHandle);
    std::swap(mPID, rhs.mPID);
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
        return tl::unexpected(make_error_code(std::errc::function_not_supported));

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
        return tl::unexpected(make_error_code(std::errc::function_not_supported));

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
        return tl::unexpected(make_error_code(std::errc::invalid_argument));

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

    tl::expected<std::vector<std::string>, std::error_code> result;

    for (int i = 0; i < num; i++) {
        auto arg = zero::strings::encode(args[i]);

        if (!arg) {
            result = tl::unexpected(arg.error());
            break;
        }

        result->push_back(std::move(*arg));
    }

    LocalFree(args);
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
