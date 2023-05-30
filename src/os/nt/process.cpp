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

std::optional<DWORD> zero::os::nt::process::Process::ppid() const {
    if (!queryInformationProcess)
        return std::nullopt;

    PROCESS_BASIC_INFORMATION info = {};

    if (!NT_SUCCESS(queryInformationProcess(
            mHandle,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
    )))
        return std::nullopt;

    return (DWORD) (uintptr_t) info.Reserved3;
}

std::optional<std::string> zero::os::nt::process::Process::name() const {
    std::optional<std::filesystem::path> path = image();

    if (!path)
        return std::nullopt;

    return path->filename().string();
}

std::optional<std::filesystem::path> zero::os::nt::process::Process::image() const {
    WCHAR buffer[MAX_PATH] = {};
    DWORD size = ARRAYSIZE(buffer);

    if (!QueryFullProcessImageNameW(mHandle, 0, buffer, &size))
        return std::nullopt;

    return buffer;
}

std::optional<std::vector<std::string>> zero::os::nt::process::Process::cmdline() const {
    if (!queryInformationProcess)
        return std::nullopt;

    PROCESS_BASIC_INFORMATION info = {};

    if (!NT_SUCCESS(queryInformationProcess(
            mHandle,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
    )))
        return std::nullopt;

    void *ptr = nullptr;

    if (!ReadProcessMemory(
            mHandle,
            &info.PebBaseAddress->ProcessParameters,
            &ptr,
            sizeof(ptr),
            nullptr
    ))
        return std::nullopt;

    RTL_USER_PROCESS_PARAMETERS parameters = {};

    if (!ReadProcessMemory(
            mHandle,
            ptr,
            &parameters,
            sizeof(RTL_USER_PROCESS_PARAMETERS),
            nullptr
    ))
        return std::nullopt;

    if (!parameters.CommandLine.Buffer || parameters.CommandLine.Length == 0)
        return std::nullopt;

    std::unique_ptr<WCHAR[]> buffer = std::make_unique<WCHAR[]>(parameters.CommandLine.Length / sizeof(WCHAR) + 1);

    if (!ReadProcessMemory(
            mHandle,
            parameters.CommandLine.Buffer,
            buffer.get(),
            parameters.CommandLine.Length,
            nullptr
    ))
        return std::nullopt;

    int num = 0;
    LPWSTR *args = CommandLineToArgvW(buffer.get(), &num);

    if (!args)
        return std::nullopt;

    std::optional<std::vector<std::string>> cmdline = std::make_optional<std::vector<std::string>>();

    for (int i = 0; i < num; i++) {
        std::optional<std::string> arg = zero::strings::encode(args[i]);

        if (!arg) {
            cmdline.reset();
            break;
        }

        cmdline->push_back(*arg);
    }

    LocalFree(args);

    return cmdline;
}

std::optional<zero::os::nt::process::Process> zero::os::nt::process::openProcess(DWORD pid) {
    HANDLE handle = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            false,
            pid
    );

    if (!handle)
        return std::nullopt;

    return Process{handle, pid};
}
