#include <zero/os/process.h>
#include <algorithm>
#include <winternl.h>

zero::os::process::Process::Process(HANDLE handle, int pid) : mHandle(handle), mPID(pid) {

}

zero::os::process::Process::Process(zero::os::process::Process &&rhs) noexcept: mHandle(nullptr), mPID(0) {
    std::swap(mHandle, rhs.mHandle);
    std::swap(mPID, rhs.mPID);
}

zero::os::process::Process::~Process() {
    if (!mHandle)
        return;

    CloseHandle(mHandle);
}

int zero::os::process::Process::pid() const {
    return mPID;
}

std::optional<std::string> zero::os::process::Process::name() const {
    std::optional<std::filesystem::path> path = image();

    if (!path)
        return std::nullopt;

    return path->filename().string();
}

std::optional<std::filesystem::path> zero::os::process::Process::image() const {
    char buffer[MAX_PATH] = {};
    DWORD size = sizeof(buffer);

    if (!QueryFullProcessImageNameA(mHandle, 0, buffer, &size))
        return std::nullopt;

    return buffer;
}

std::optional<std::vector<std::string>> zero::os::process::Process::cmdline() const {
    static auto queryInformationProcess = (decltype(NtQueryInformationProcess) *) GetProcAddress(
            GetModuleHandleA("ntdll"),
            "NtQueryInformationProcess"
    );

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

    std::vector<std::string> cmdline;

    std::transform(
            args,
            args + num,
            std::back_inserter(cmdline),
            [](const auto &arg) -> std::string {
                int n = WideCharToMultiByte(CP_UTF8, 0, arg, -1, nullptr, 0, nullptr, nullptr);

                if (n == 0)
                    return "";

                std::unique_ptr<char[]> buffer = std::make_unique<char[]>(n);

                if (WideCharToMultiByte(CP_UTF8, 0, arg, -1, buffer.get(), n, nullptr, nullptr) == 0)
                    return "";

                return buffer.get();
            }
    );

    LocalFree(args);

    return cmdline;
}

std::optional<zero::os::process::Process> zero::os::process::openProcess(int pid) {
    HANDLE handle = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            false,
            pid
    );

    if (!handle)
        return std::nullopt;

    return Process{handle, pid};
}
